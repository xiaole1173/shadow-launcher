import urllib.request, json, time

BASE = "http://127.0.0.1:9999"

def nav(page):
    req = urllib.request.Request(f"{BASE}/navigate",
        data=json.dumps({"page": page}).encode(),
        headers={"Content-Type": "application/json"})
    urllib.request.urlopen(req, timeout=5)

def eval_qml(code):
    req = urllib.request.Request(f"{BASE}/eval",
        data=json.dumps({"qml": code}).encode(),
        headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=5) as r:
            return json.loads(r.read().decode())
    except Exception as e:
        return str(e)

# Navigate to home
nav(0)
time.sleep(1)

# Check overlay state
result = eval_qml('(function() { return JSON.stringify({vs: showVersionSelect, al: versionSelectLoader.active}); })()')
print("State (before):", result)

# Open overlay
result = eval_qml('(function() { showVersionSelect = true; return "opened"; })()')
print("Open:", result)
time.sleep(2)

# Check again
result = eval_qml('(function() { return JSON.stringify({vs: showVersionSelect, al: versionSelectLoader.active}); })()')
print("State (after):", result)

# Check overlay children
result = eval_qml('(function() { try { var item = versionSelectLoader.item; if(!item) return "null"; return JSON.stringify({len: item.children.length}); } catch(e) { return e.message; } })()')
print("Overlay children:", result)

# Check size
result = eval_qml('(function() { try { var item = versionSelectLoader.item; if(!item) return "null"; return JSON.stringify({w: item.width, h: item.height, v: item.visible, o: item.opacity}); } catch(e) { return e.message; } })()')
print("Overlay size:", result)

# Check if version list has items
result = eval_qml('(function() { try { var item = versionSelectLoader.item; if(!item || !item.versionList) return "no versionList"; return JSON.stringify({count: item.versionList.count}); } catch(e) { return e.message; } })()')
print("Version list:", result)

# Close overlay
result = eval_qml('(function() { showVersionSelect = false; return "closed"; })()')
print("Close:", result)
