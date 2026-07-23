import urllib.request, json, time

BASE = "http://127.0.0.1:9999"

def eval_qml(code):
    req = urllib.request.Request(f"{BASE}/eval",
        data=json.dumps({"qml": code}).encode(),
        headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=5) as r:
            return json.loads(r.read().decode())
    except Exception as e:
        return {"error": str(e)}

# Navigate home, wait, open overlay, wait long for async load, check
nav_req = urllib.request.Request(f"{BASE}/navigate",
    data=json.dumps({"page": 0}).encode(),
    headers={"Content-Type": "application/json"})
urllib.request.urlopen(nav_req, timeout=5)
time.sleep(1)

# Show version select
result = eval_qml("showVersionSelect = true")
print("Open:", result)
time.sleep(3)

# Wait more for async load
r1 = eval_qml("showVersionSelect")
print("showVersionSelect:", r1)

# Take screenshot
with urllib.request.urlopen(f"{BASE}/screenshot", timeout=5) as r:
    with open(r"C:\Users\蔡朝彬\.openclaw\workspace\screenshots\bug_overlay2.png", "wb") as f:
        f.write(r.read())
print("Screenshot saved")

# Close
result = eval_qml("showVersionSelect = false")
print("Close:", result)
