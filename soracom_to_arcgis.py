import requests
import getpass
import time
import json
from arcgis.gis import GIS

# --- 1. SORACOM HARVEST CONFIGURATION ---
SORACOM_API_KEY = "keyId-arjRcDLUwvMgCqnRENJWlKObyHyRjVMh"
SORACOM_API_SECRET = "secret-fI9MDaK4kmL5mAfGdzU30ROf788j5KO9DwNFvHdbxRWDjGSbPSSHiD31lHVPLT5K"
SORACOM_IMSI = "311588112047892"

# Soracom API endpoints
soracom_auth_url = "https://g.api.soracom.io/v1/auth"
soracom_data_url_template = f"https://g.api.soracom.io/v1/data/Subscriber/{SORACOM_IMSI}?limit=1"

# --- 2. ARCGIS CONFIGURATION ---
ARCGIS_URL = "https://www.arcgis.com"
ARCGIS_ITEM_ID = "8666b76b27a74b1b8569d340cd3928e6"
ARCGIS_LAYER_URL = "https://services1.arcgis.com/mLNdQKiKsj5Z5YMN/arcgis/rest/services/Live_Dumpster_Sensors/FeatureServer/0"

# --- FOR AUTOMATED LOGIN ---
ARCGIS_CLIENT_ID = "y1pRAghSidocx8rt"
ARCGIS_CLIENT_SECRET = "f51a4e676cb446a6b0bbc1a3ff99e4cb"

# --- 3. THE SCRIPT ---
def get_soracom_session(key_id, secret):
    """
    Authenticates with Soracom to get a temporary session token.
    """
    try:
        print("Authenticating with Soracom...")
        auth_payload = {
            'authKeyId': key_id,
            'authKey': secret
        }
        response = requests.post(soracom_auth_url, json=auth_payload)
        response.raise_for_status() # Check for auth errors (4xx, 5xx)
        
        session_data = response.json()
        print("Successfully authenticated.")
        # The response gives us an 'apiKey' (same as key_id) and a 'token'
        return session_data
        
    except requests.exceptions.RequestException as e:
        print(f"Error authenticating with Soracom: {e}")
        # This will print the detailed error message from Soracom (e.g., "invalid_credentials")
        if e.response:
            print(f"Error details: {e.response.text}")
        return None

def fetch_soracom_data(data_url, headers):
    """
    Fetches the latest data point from Soracom Harvest.
    """
    try:
        print(f"Fetching data from Soracom for SIM: {SORACOM_IMSI}...")
        response = requests.get(data_url, headers=headers)
        response.raise_for_status()  # Raises an error for bad responses (4xx or 5xx)
        
        data = response.json()
        if not data:
            print("No data returned from Soracom.")
            return None
        
        # Get the most recent entry
        latest_entry = data[0]
        # The 'Parsed data' is what we want
        content = latest_entry.get('parsedData', latest_entry.get('content'))
        print(f"Successfully fetched data: {content}")
        
        if isinstance(content, str):
            try:
                return json.loads(content)
            except json.JSONDecodeError:
                print(f"Error: Could not parse Soracom data string: {content}")
                return None
        return content # Return as-is if it's already an object
        
    except requests.exceptions.RequestException as e:
        print(f"Error fetching from Soracom: {e}")
        return None

def connect_to_arcgis():
    """
    Connects to ArcGIS Online and gets the feature layer.
    """
    try:
        print("\n--- ArcGIS Authentication ---")
        print("Connecting to ArcGIS Online using Client ID...")
        gis = GIS(ARCGIS_URL, client_id=ARCGIS_CLIENT_ID, client_secret=ARCGIS_CLIENT_SECRET)
        print(f"Successfully connected via App: {ARCGIS_CLIENT_ID}.")

        feature_layer_item = gis.content.get(ARCGIS_ITEM_ID)
        dumpster_layer = feature_layer_item.layers[0]
        return dumpster_layer
        
    except Exception as e:
        print(f"Error connecting to ArcGIS: {e}")
        return None

def update_arcgis_feature(layer, soracom_data):
    """
    Updates a single feature in ArcGIS with new data.
    """
    try:
        # --- Data Mapping ---
        soracom_id = soracom_data.get('id')
        if soracom_id is None:
            print("Error: 'id' not found in Soracom data.")
            return
            
        arcgis_dumpster_id = f"D-{soracom_id}" # e.g., "1" -> "D-1"
        
        new_fill_level = soracom_data.get('fullness')
        new_temperature = soracom_data.get('temperature')
        new_last_updated = int(time.time() * 1000)

        # Find the feature in ArcGIS that matches our Dumpster_ID
        where_clause = f"Dumpster_ID = '{arcgis_dumpster_id}'"
        features_to_update = layer.query(where=where_clause)
        
        if not features_to_update.features:
            print(f"Error: Could not find a dumpster with ID '{arcgis_dumpster_id}' in ArcGIS.")
            return

        feature_to_edit = features_to_update.features[0]
        feature_to_edit.attributes['Fill_Level'] = new_fill_level
        feature_to_edit.attributes['Temperature'] = new_temperature
        feature_to_edit.attributes['Last_Updated'] = new_last_updated

        print(f"Updating feature: {arcgis_dumpster_id}...")
        update_result = layer.edit_features(updates=[feature_to_edit])
        
        if update_result['updateResults'] and update_result['updateResults'][0]['success']:
            print(f"Successfully updated '{arcgis_dumpster_id}' with data: Fill={new_fill_level}, Temp={new_temperature}")
        else:
            print(f"Error updating feature: {update_result}")
            
    except Exception as e:
        print(f"An error occurred during the update: {e}")

# --- 4. RUN THE SCRIPT ---
if __name__ == "__main__":
    
    # 1. Authenticate with Soracom
    session = get_soracom_session(SORACOM_API_KEY, SORACOM_API_SECRET)
    
    if session:
        # 2. Build the correct headers using the session token
        soracom_headers = {
            "X-Soracom-API-Key": session['apiKey'],
            "X-Soracom-Token": session['token'],
            "Accept": "application/json"
        }
        
        # 3. Get data from Soracom
        data = fetch_soracom_data(soracom_data_url_template, soracom_headers)
        
        if data:
            # 4. Connect to ArcGIS
            dumpster_layer = connect_to_arcgis()
            
            if dumpster_layer:
                # 5. Send data to ArcGIS
                update_arcgis_feature(dumpster_layer, data)