import json
import os
import logging
import datetime as dt
from datetime import timezone

from paho.mqtt import client as mqtt
import influxdb_client_3 as influx
from influxdb_client_3.exceptions.exceptions import InfluxDBError
from dotenv import load_dotenv

# Load environment variables
load_dotenv()

# MQTT config
MQTT_BROKER = os.getenv("MQTT_BROKER")
MQTT_PORT = int(os.getenv("MQTT_PORT", 1883))
MQTT_TOPIC = os.getenv("MQTT_TOPIC", "sensor/#")

# InfluxDB Cloud config
CLOUD_URL = os.getenv("CLOUD_URL")
CLOUD_ORG = os.getenv("CLOUD_ORG")
CLOUD_TOKEN = os.getenv("CLOUD_TOKEN")
CLOUD_BUCKET = os.getenv("CLOUD_BUCKET")

# Set your measurement name
INFLUXDB_MEASUREMENT_NAME = "solar_data"

DEVICE_MAP_FILE = "device_map.json"

logger = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO)

# Load or create device map
def load_device_map() -> dict:
    if os.path.exists(DEVICE_MAP_FILE):
        with open(DEVICE_MAP_FILE) as f:
            return json.load(f)
    with open(DEVICE_MAP_FILE, "w") as f:
        json.dump({}, f, indent=2)
    return {}

def save_device_map(device_map: dict) -> None:
    with open(DEVICE_MAP_FILE, "w") as f:
        json.dump(device_map, f, indent=2)

device_map = load_device_map()

# InfluxDB callbacks
def influx_success(self, data: bytes):
    logger.info("influxdb_write_success")

def influx_error(self, data: str, exception: InfluxDBError):
    logger.error("influxdb_write_failure", extra=dict(data=data, cause=str(exception)))

def influx_retry(self, data: str, exception: InfluxDBError):
    logger.debug("influxdb_retry", extra=dict(data=data, cause=str(exception)))

# Create InfluxDB client
def create_influx_client() -> influx.InfluxDBClient3:
    for var_name, var_value in [
        ("CLOUD_TOKEN", CLOUD_TOKEN),
        ("CLOUD_URL", CLOUD_URL),
        ("CLOUD_BUCKET", CLOUD_BUCKET),
        ("CLOUD_ORG", CLOUD_ORG)
    ]:
        if var_value is None:
            raise RuntimeError(f"environment variable {var_name} is not set")

    write_options = influx.WriteOptions(
        flush_interval=2_000,
        jitter_interval=500,
        retry_interval=2_000,
        max_retries=5,
        max_retry_delay=15_000,
        exponential_base=2,
    )

    options = influx.write_client_options(
        success_callback=influx_success,
        error_callback=influx_error,
        retry_callback=influx_retry,
        write_options=write_options,
    )

    return influx.InfluxDBClient3(
        host=CLOUD_URL,
        token=CLOUD_TOKEN,
        org=CLOUD_ORG,             # specify org
        database=CLOUD_BUCKET,     # specify bucket
        write_client_options=options,
    )

influx_client = create_influx_client()

# MQTT callbacks
def on_connect(client, userdata, flags, rc):
    logger.info("mqtt_connected", extra=dict(rc=rc))
    client.subscribe(MQTT_TOPIC)

def on_message(client, userdata, msg):
    global device_map

    try:
        payload = json.loads(msg.payload.decode())
        topic_parts = msg.topic.split("/")
        device_id = topic_parts[1] if len(topic_parts) > 1 else "unknown"

        mapping = device_map.get(device_id)
        if mapping is None:
            mapping = {"panel": "Unknown", "number": "Unknown"}
            device_map[device_id] = mapping
            save_device_map(device_map)
            logger.info("new_device_detected", extra=dict(device=device_id))

        # Timestamp handling
        ts = payload.pop("timestamp", None)
        if ts is not None:
            try:
                timestamp = dt.datetime.fromtimestamp(float(ts), tz=timezone.utc)
            except Exception:
                timestamp = dt.datetime.now(timezone.utc)
        else:
            timestamp = dt.datetime.now(timezone.utc)

        # Build Influx point with measurement name
        point = (
            influx.Point(INFLUXDB_MEASUREMENT_NAME)
            .time(timestamp, write_precision=influx.WritePrecision.S)
            .tag("device", device_id)
            .tag("panel", mapping.get("panel", "Unknown"))
            .tag("number", mapping.get("number", "Unknown"))
        )

        for key, value in payload.items():
            point = point.field(key, float(value))

        # Write to InfluxDB
        influx_client.write(point)

    except Exception as e:
        logger.exception("mqtt_message_processing_failed", extra=dict(error=str(e)))

# Main
def main():
    mqtt_client = mqtt.Client(client_id="esp32-multi-client")
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message

    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    mqtt_client.loop_forever()  # Blocking loop

if __name__ == "__main__":
    main()
