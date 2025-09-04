import requests
import json

# Your Uptime Kuma instance URL and a status page slug
base_url = "https://uptime.cloud.rcfortress.site"
# Let's try a common default slug, please change if yours is different
status_page_slug = "main" 
api_endpoint = f"/api/status-page/{status_page_slug}"


def get_status_page_data(base_url, slug):
    """Fetches the main status page data (monitor names and structure)."""
    endpoint = f"/api/status-page/{slug}"
    url = f"{base_url}{endpoint}"
    print(f"Requesting Monitor List from: {url}")
    response = requests.get(url)
    response.raise_for_status()
    return response.json()

def get_heartbeat_data(base_url, slug):
    """Fetches the heartbeat data (status, ping)."""
    endpoint = f"/api/status-page/heartbeat/{slug}"
    url = f"{base_url}{endpoint}"
    print(f"Requesting Heartbeat Data from: {url}")
    response = requests.get(url)
    response.raise_for_status()
    return response.json()

try:
    # 1. Get the list of monitors
    status_page_data = get_status_page_data(base_url, status_page_slug)
    
    # 2. Get the heartbeat data
    heartbeat_data = get_heartbeat_data(base_url, status_page_slug)

    # 3. Combine the data and display
    print("\n--- Combined Monitor Status ---")
    
    monitors_by_id = {}
    if status_page_data and 'publicGroupList' in status_page_data:
        for group in status_page_data['publicGroupList']:
            for monitor in group.get('monitorList', []):
                monitors_by_id[str(monitor.get('id'))] = monitor
    
    if heartbeat_data and 'heartbeatList' in heartbeat_data:
        for monitor_id, heartbeats in heartbeat_data['heartbeatList'].items():
            monitor_name = monitors_by_id.get(monitor_id, {}).get('name', 'Unknown Monitor')
            if heartbeats:
                latest_heartbeat = heartbeats[0]
                status = latest_heartbeat.get('status')
                ping = latest_heartbeat.get('ping')
                status_text = "UP" if status == 1 else "DOWN" if status == 0 else "PENDING/OTHER"
                print(f"- Monitor: {monitor_name} (ID: {monitor_id})")
                print(f"  - Status: {status_text}")
                print(f"  - Ping: {ping} ms")
            else:
                print(f"- Monitor: {monitor_name} (ID: {monitor_id}) has no heartbeat data.")

except requests.exceptions.RequestException as e:
    print(f"\nAn error occurred: {e}")
    print("Please check if the instance URL is correct and the status page with the specified slug exists and is public.")
except json.JSONDecodeError as e:
    print(f"\nFailed to decode JSON from the response: {e}")
