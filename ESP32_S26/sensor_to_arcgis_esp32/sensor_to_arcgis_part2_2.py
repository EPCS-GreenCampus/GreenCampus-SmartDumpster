import sys
import json
import time
import requests
import os

# ArcGIS Credentials
CLIENT_ID = "y1pRAghSidocx8rt"
CLIENT_SECRET = "f51a4e676cb446a6b0bbc1a3ff99e4cb"
LAYER_URL = "https://services1.arcgis.com/mLNdQKiKsj5Z5YMN/arcgis/rest/services/Live_Dumpster_Sensors/FeatureServer/0"

# Always use absolute path
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_FILE_PATH = os.path.join(BASE_DIR, "relay1.txt") #CHANGE FILE NAME TO THE FILE YOU CAPTURED


# -----------------------------
# Get ArcGIS Access Token
# -----------------------------
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
        return r.json().get("access_token")

    print("Auth failed:", r.text)
    return None


# -----------------------------
# Convert text file → JSON objects
# -----------------------------
def read_data_from_file(file_path):

 with open(file_path, "r", encoding="utf-8") as file:

        for line in file:
            line = line.strip()

            if not line:
                continue

            try:
                data = json.loads(line)

                feature = {
                    "attributes": {
                        "Dumpster_ID": f"D-{data['id']}",
                        "Fill_Level": float(data["fullness"]),
                        "Last_Updated": int(time.time() * 1000)
                    }
                }

                print("Parsed reading:", feature)

                yield feature

            except json.JSONDecodeError:
                print("Skipping invalid line:", line)

# -----------------------------
# Send JSON package to ArcGIS
# -----------------------------
# 3️⃣ Send update
def send_to_arcgis(token, feature):

    dumpster_id = feature["attributes"]["Dumpster_ID"]

    # 1️⃣ Query to get OBJECTID
    query_url = f"{LAYER_URL}/query"

    query_params = {
        "where": f"Dumpster_ID='{dumpster_id}'",
        "outFields": "*",
        "f": "json",
        "token": token
    }

    q = requests.get(query_url, params=query_params)

    if q.status_code != 200:
        print("Query failed:", q.text)
        return

    query_result = q.json()

    if not query_result.get("features"):
        print("No feature found for", dumpster_id)
        return

    # 2️⃣ Get existing feature (this defines existing_feature)
    existing_feature = query_result["features"][0]

    # 3️⃣ Update attributes
    existing_feature["attributes"]["Fill_Level"] = feature["attributes"]["Fill_Level"]
    existing_feature["attributes"]["Last_Updated"] = feature["attributes"]["Last_Updated"]

    # 4️⃣ Send update (features MUST be JSON string)
    update_url = f"{LAYER_URL}/updateFeatures"

    update_payload = {
        "features": json.dumps([existing_feature]),  # MUST be string
        "f": "json",
        "token": token
    }

    response = requests.post(update_url, data=update_payload)

    print("RAW RESPONSE:", response.text)

    try:
        result = response.json()
    except:
        print("Server did not return JSON.")
        return

    if "updateResults" in result and result["updateResults"][0]["success"]:
        print(f"Updated {dumpster_id} | Fill: {existing_feature['attributes']['Fill_Level']}%")
    else:
        print("Update failed:", result)

# -----------------------------
# Main
# -----------------------------
def main():
    token = get_auth_token()

    if not token:
        sys.exit(1)

    print("Reading imported data from text file...")

    for feature in read_data_from_file(DATA_FILE_PATH):
        send_to_arcgis(token, feature)
        time.sleep(0.2)

    print("Finished processing file.")


if __name__ == "__main__":
    main()