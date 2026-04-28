import sys
import json
import time
import requests

# ArcGIS Credentials
CLIENT_ID = "y1pRAghSidocx8rt"
CLIENT_SECRET = "f51a4e676cb446a6b0bbc1a3ff99e4cb"
LAYER_URL = "https://services1.arcgis.com/mLNdQKiKsj5Z5YMN/arcgis/rest/services/Live_Dumpster_Sensors/FeatureServer/0"

# Path to imported text file
DATA_FILE_PATH = "relay4.cpp.txt"


# Get ArcGIS Access Token
def get_auth_token():
    url = "https://www.arcgis.com/sharing/rest/oauth2/token"
    payload = {
        "client_id": CLIENT_ID,
        "client_secret": CLIENT_SECRET,
        "grant_type": "client_credentials",
        "f": "json"
    }

    r = requests.post(url, data=payload)
    if r.status_code == 200:
        return r.json().get("access_token", "")
    return ""


# Push data to ArcGIS
def push_to_arcgis(token, data):
    dumpster_id = f"D-{data['id']}"

    # 1. Query feature
    query_params = {
        "where": f"Dumpster_ID='{dumpster_id}'",
        "outFields": "*",
        "f": "json",
        "token": token
    }

    q = requests.get(f"{LAYER_URL}/query", params=query_params)
    res = q.json()

    if not res.get("features"):
        print(f"Bin {dumpster_id} not found!")
        return

    # 2. Prepare update
    feature = res["features"][0]
    feature["attributes"]["Fill_Level"] = data["fullness"]
    # feature["attributes"]["Temperature"] = data["temp"]     -Currently don't have temperature reading
    feature["attributes"]["Last_Updated"] = int(time.time() * 1000)

    # 3. Send update
    update_payload = {
        "features": json.dumps([feature]),
        "token": token,
        "f": "json"
    }

    requests.post(f"{LAYER_URL}/updateFeatures", data=update_payload)

    print(f"ArcGIS Updated for {dumpster_id} | Fill: {data['fullness']}%")


# # Read data from text file
# def read_data_from_file(file_path):
#     with open(file_path, "r") as file:
#         for line in file:
#             try:
#                 if "{" in line:
#                     yield json.loads(line.strip())
#             except json.JSONDecodeError:
#                 # Skip bad lines
#                 continue

def read_data_from_file(file_path):
    with open(file_path, "r") as file:
        current_reading = {}

        for line in file:
            line = line.strip()

            if line.startswith("Dumpster ID:"):
                current_reading["id"] = int(line.split(":")[1].strip())

            elif line.startswith("Fullness:"):
                fullness_value = line.split(":")[1].strip().replace("%", "")
                current_reading["fullness"] = float(fullness_value)

            elif line.startswith("Distance 60"):
                # optional: use this as temperature if needed
                pass

            # When we have both required fields, yield it
            if "id" in current_reading and "fullness" in current_reading:
                yield current_reading
                current_reading = {}

def main():
    token = get_auth_token()
    if not token:
        print("Auth Failed!")
        sys.exit(1)

    print("Reading imported data from text file...")

    # --- IMPORTED DATA SOURCE ---
    for sensor_data in read_data_from_file(DATA_FILE_PATH):
        push_to_arcgis(token, sensor_data)
        time.sleep(0.2)  # optional throttle

    print("Finished processing file.")


if __name__ == "__main__":
    main()