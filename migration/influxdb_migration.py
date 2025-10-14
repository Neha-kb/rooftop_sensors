from influxdb_client import InfluxDBClient
from influxdb_client_3 import InfluxDBClient3, WritePrecision, write_client_options
from influxdb_client.client.write_api import SYNCHRONOUS
import datetime
import os
from collections import defaultdict
import pytz

# --- OSS config ---
OSS_URL = "https://xxx.net:8086"
OSS_TOKEN = "xxx"
OSS_ORG = "IPV"
OSS_BUCKET = "Uni"       


# --- Cloud (destination) config ---
CLOUD_URL = "https://xxx.com"
CLOUD_TOKEN = os.environ.get("INFLUXDB_TOKEN")
CLOUD_ORG = "Institute of Photovoltaics"
CLOUD_BUCKET = "migration_test"

# --- Time range (UTC aware) ---
start_time = datetime.datetime(2025, 8, 10, 14, 2, 0, tzinfo=pytz.UTC)
end_time   = datetime.datetime(2025, 8, 10, 14, 3, 0, tzinfo=pytz.UTC)

# --- Connect to OSS ---
client = InfluxDBClient(url=OSS_URL, token=OSS_TOKEN, org=OSS_ORG)
query_api = client.query_api()

# --- Flux query ---
flux_query = f"""
from(bucket: "{OSS_BUCKET}")
  |> range(start: {start_time.isoformat().replace('+00:00', 'Z')}, stop: {end_time.isoformat().replace('+00:00', 'Z')})
  |> filter(fn: (r) => r["_measurement"] == "ParkData")
"""

# --- Run query ---
tables = query_api.query(org=OSS_ORG, query=flux_query)

# for table in tables:
#     for record in table.records:
#         print(record.values["_time"], record.get_value())

points = []

for table in tables:
    for record in table.records:
        measurement = record.get_measurement()

        # Tags: all non-field, non-time, non-measurement columns
        tags = {
            k: v
            for k, v in record.values.items()
            if k not in ['_time', '_value', '_field', '_measurement',
                         'result', 'table', '_start', '_stop']
        }

        # Fields: current _field → _value
        fields = {record.get_field(): record.get_value()}

        # Time: handle both datetime and string
        time_val = record.values["_time"]
        if isinstance(time_val, datetime.datetime):
            time = int(time_val.timestamp())  # seconds
        else:  # assume string
            time = int(datetime.datetime.fromisoformat(
                time_val.replace("Z", "+00:00")
            ).timestamp())

        points.append({
            "measurement": measurement,
            "tags": tags,
            "fields": fields,
            "time": time
        })

print(points)



# # # --- 2. Write to Cloud Serverless ---
cloud_client = InfluxDBClient3(host=CLOUD_URL, token=CLOUD_TOKEN, org=CLOUD_ORG, database=CLOUD_BUCKET)
# write_api = write_client_options(write_options=SYNCHRONOUS)

cloud_client.write(record=points, write_precision="s")
cloud_client.close()


client.close()