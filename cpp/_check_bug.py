import urllib.request, json, time

def nav(page):
    req = urllib.request.Request("http://127.0.0.1:9999/navigate",
        data=json.dumps({"page": page}).encode(),
        headers={"Content-Type": "application/json"})
    urllib.request.urlopen(req, timeout=5)

def qml_eval(code):
    req = urllib.request.Request("http://127.0.0.1:9999/eval",
        data=json.dumps({"qml": code}).encode(),
        headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=5) as r:
        return r.read().decode()

# Check what version select overlay shows
nav(0)
time.sleep(0.5)
result = qml_eval("(function(){ return appWindow ? 'appWindow found' : 'no appWindow'; })()")
print("appWindow:", result)

result = qml_eval("(function(){ if(typeof backend !== 'undefined' && backend) return JSON.stringify({showVersionSelect: backend.showVersionSelect, showVersionSettings: backend.showVersionSettings}); return 'no backend'; })()")
print("backend state:", result)
