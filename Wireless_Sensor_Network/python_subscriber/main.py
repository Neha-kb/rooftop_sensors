import json
import time
import os
import threading
from queue import Queue
from paho.mqtt import client as mqtt
from influxdb_client_3 import InfluxDBClient3
from dotenv import load_dotenv
from datetime import datetime, timezone

# Load environment variables
load_dotenv()

# --- MQTT config ---
MQTT_BROKER = os.getenv("MQTT_BROKER")
MQTT_PORT = int(os.getenv("MQTT_PORT", 1883))
MQTT_TOPIC = os.getenv("MQTT_TOPIC", "sensor/#")

# --- InfluxDB Cloud config ---
CLOUD_URL = os.getenv("CLOUD_URL")
CLOUD_TOKEN = os.getenv("INFLUXDB_TOKEN")
CLOUD_ORG = os.getenv("CLOUD_ORG")
CLOUD_BUCKET = os.getenv("CLOUD_BUCKET")

# --- Mapping file ---
DEVICE_MAP_FILE = "device_map.json"

# --- Batch configuration ---
BATCH_SIZE = 20
BATCH_INTERVAL = 5

# --- Load or create device map ---
def load_device_map():
    if os.path.exists(DEVICE_MAP_FILE):
        with open(DEVICE_MAP_FILE) as f:
            return json.load(f)
    else:
        print("No device_map.json found. Creating a new one.")
        with open(DEVICE_MAP_FILE, "w") as f:
            json.dump({}, f, indent=2)
        return {}

def save_device_map():
    with open(DEVICE_MAP_FILE, "w") as f:
        json.dump(device_map, f, indent=2)

device_map = load_device_map()

# --- Connect to InfluxDB Cloud ---
cloud_client = InfluxDBClient3(
    host=CLOUD_URL,
    token=CLOUD_TOKEN,
    org=CLOUD_ORG,
    database=CLOUD_BUCKET
)

# --- Queue for batching ---
data_queue = Queue()

# --- Background batch writer thread ---
def batch_writer():
    buffer = []
    last_flush = time.time()

    while True:
        try:
            item = data_queue.get(timeout=1)
            buffer.append(item)
        except:
            pass  # queue empty

        if len(buffer) >= BATCH_SIZE or (time.time() - last_flush) > BATCH_INTERVAL:
            if buffer:
                try:
                    # Make sure to use the correct precision — here we use 's' since ESP32 timestamp is in seconds
                    cloud_client.write(record=buffer, write_precision="s")
                    print(f"Wrote batch of {len(buffer)} points to InfluxDB Cloud")
                except Exception as e:
                    print("Batch write failed:", e)
                buffer.clear()
                last_flush = time.time()

# Start batch writer thread
threading.Thread(target=batch_writer, daemon=True).start()

# --- MQTT Callbacks ---
def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT broker with result code", rc)
    client.subscribe(MQTT_TOPIC)

def on_message(client, userdata, msg):
    global device_map
    try:
        payload = msg.payload.decode()
        data = json.loads(payload)

        # Topic format: sensor/<mac_id>
        topic_parts = msg.topic.split("/")
        device_id = topic_parts[1] if len(topic_parts) > 1 else "unknown"

        # Lookup or auto-add device mapping
        mapping = device_map.get(device_id)
        if mapping is None:
            mapping = {"panel": "Unknown", "number": "Unknown"}
            device_map[device_id] = mapping
            save_device_map()
            print(f" New device detected: {device_id} → added to device_map.json")

        print(f"Data from {device_id}: {data}")

        # --- Extract and convert timestamp ---
        ts = data.pop("timestamp", None)   # remove timestamp from fields
        if ts is not None:
            try:
                # Convert epoch seconds to RFC3339 (UTC)
                ts_iso = datetime.fromtimestamp(float(ts), tz=timezone.utc).isoformat()
            except Exception:
                ts_iso = datetime.now(timezone.utc).isoformat()
        else:
            ts_iso = datetime.now(timezone.utc).isoformat()

        # --- Prepare InfluxDB point ---
        point = {
            "measurement": "solar_data",
            "tags": {
                "device": device_id,
                "panel": mapping.get("panel", "Unknown"),
                "number": mapping.get("number", "Unknown")
            },
            "fields": {k: float(v) for k, v in data.items()},
            "time": ts_iso
        }

        # Add to queue instead of direct write
        data_queue.put(point)

    except Exception as e:
        print("Error handling MQTT message:", e)

# --- MQTT Client ---
mqtt_client = mqtt.Client(client_id="esp32-multi-client")
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
mqtt_client.loop_forever()
