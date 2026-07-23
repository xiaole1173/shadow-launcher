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

# Simple evals
r = eval_qml("showVersionSelect")
print("showVersionSelect:", r)

r = eval_qml("navListIndex")
print("navListIndex:", r)

r = eval_qml("versionSelectLoader")
print("versionSelectLoader:", r)

r = eval_qml("versionSelectLoader.active")
print("loader.active:", r)
