import json
import time
import os
import threading
from queue import Queue
from paho.mqtt import client as mqtt
from influxdb_client_3 import InfluxDBClient3

# --- MQTT config ---
MQTT_BROKER = "xxx"          # your VM’s Mosquitto broker IP
MQTT_PORT = 1883
MQTT_TOPIC = "sensor/#"

# --- InfluxDB Cloud config ---
CLOUD_URL = "xxx"  # Example — replace with yours
CLOUD_TOKEN = os.environ.get("INFLUXDB_TOKEN")
CLOUD_ORG = "Institute of Photovoltaics"
CLOUD_BUCKET = "Sesnsor_test"

# --- Mapping file ---
DEVICE_MAP_FILE = "device_map.json"

# --- Batch configuration ---
BATCH_SIZE = 20          # number of points before write
BATCH_INTERVAL = 5       # seconds

# --- Load or create device map ---
def load_device_map():
    if os.path.exists(DEVICE_MAP_FILE):
        with open(DEVICE_MAP_FILE) as f:
            return json.load(f)
    else:
        print(" No device_map.json found. Creating a new one.")
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
            # Wait up to 1 sec for a message
            item = data_queue.get(timeout=1)
            buffer.append(item)
        except:
            pass  # queue empty

        # Check flush conditions
        if len(buffer) >= BATCH_SIZE or (time.time() - last_flush) > BATCH_INTERVAL:
            if buffer:
                try:
                    cloud_client.write(record=buffer, write_precision="s")
                    print(f"Wrote batch of {len(buffer)} points to InfluxDB Cloud")
                except Exception as e:
                    print(" Batch write failed:", e)
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

        # Prepare InfluxDB point
        point = {
            "measurement": "solar_data",
            "tags": {
                "device": device_id,
                "panel": mapping.get("panel", "Unknown"),
                "number": mapping.get("number", "Unknown")
            },
            "fields": {k: float(v) for k, v in data.items()},
            "time": int(time.time())
        }

        # Add to queue instead of direct write
        data_queue.put(point)

    except Exception as e:
        print("Error handling MQTT message:", e)

# --- MQTT Client ---
mqtt_client = mqtt.Client("esp32-multi-client")
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
mqtt_client.loop_forever()