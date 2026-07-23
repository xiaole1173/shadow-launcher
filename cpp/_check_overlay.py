import urllib.request, json, time

BASE = "http://127.0.0.1:9999"

def nav(page):
    req = urllib.request.Request(f"{BASE}/navigate",
        data=json.dumps({"page": page}).encode(),
        headers={"Content-Type": "application/json"})
    urllib.request.urlopen(req, timeout=5)

def screenshot(path):
    with urllib.request.urlopen(f"{BASE}/screenshot", timeout=5) as r:
        with open(path, "wb") as f:
            f.write(r.read())

def qml_eval(code):
    try:
        req = urllib.request.Request(f"{BASE}/eval",
            data=json.dumps({"qml": code}).encode(),
            headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=5) as r:
            return r.read().decode()
    except Exception as e:
        return f"ERROR: {e}"

# Go to home page
nav(0)
time.sleep(1.5)

# Check backend state
result = qml_eval("showVersionSelect")
print(f"showVersionSelect (direct): {result}")

# Try to set showVersionSelect to true to open the overlay
result = qml_eval("showVersionSelect = true; 'done'")
print(f"Set showVersionSelect=true: {result}")

time.sleep(1.5)

# Take screenshot of the overlay
screenshot(r"C:\Users\蔡朝彬\.openclaw\workspace\screenshots\bug_overlay.png")
print("Screenshot taken")

# Check if versionSelectLoader is active
result = qml_eval("versionSelectLoader.active")
print(f"versionSelectLoader.active: {result}")

# Check if the overlay item is loaded
result = qml_eval("versionSelectLoader.item ? 'loaded' : 'null'")
print(f"versionSelectLoader.item: {result}")

# Close overlay
result = qml_eval("showVersionSelect = false; 'done'")
print(f"Set showVersionSelect=false: {result}")
