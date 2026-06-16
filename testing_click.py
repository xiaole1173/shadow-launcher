"""
自动化测试：启动器安装流程全链路测试
"""
import sys, time, os, json

os.chdir(os.path.dirname(os.path.abspath(__file__)))
if '.' not in sys.path:
    sys.path.insert(0, '.')

from PySide6.QtWidgets import QApplication
from PySide6.QtCore import QUrl
from PySide6.QtQml import QQmlApplicationEngine
from shadow_launcher.ui.backend import ShadowBackend

STATE_FILE = "shadow_launcher/config/install_state.json"
TEST_VERSION = "1.20.4"

app = QApplication(sys.argv)
engine = QQmlApplicationEngine()
backend = ShadowBackend()
engine.rootContext().setContextProperty('backend', backend)
engine.load(QUrl.fromLocalFile('shadow_launcher/ui/qml/MainWindow.qml'))
time.sleep(2)

print(f"\n{'='*60}")
print(f"TEST: 安装 {TEST_VERSION}")
print(f"{'='*60}")

# Trigger install
print(f"[TEST] 调用 backend.installVersion({TEST_VERSION!r}, 0)...")
backend.installVersion(TEST_VERSION, 0)
time.sleep(1)

# Monitor
print(f"\n[TEST] 监控开始...\n")
last_prog = -1
stuck_count = 0
errors = []
done = False

for i in range(120):  # max 60s
    time.sleep(0.5)
    
    # Read state directly from file
    if os.path.exists(STATE_FILE):
        with open(STATE_FILE, 'r', encoding='utf-8') as f:
            raw = f.read()
        try:
            s = json.loads(raw)
        except Exception as e:
            errors.append(f"JSON parse error at {i*0.5}s: {e}")
            continue
    else:
        s = None

    status = s.get("status") if s else "NO STATE FILE"
    ver = s.get("version", "") if s else ""
    prog = s.get("progress", 0) if s else 0
    total = s.get("total", 0) if s else 0
    pct = prog / total * 100 if total > 0 else 0
    spd = s.get("speed", 0) if s else 0
    fname = (s.get("current_file", "") or "")[:30] if s else ""
    elapsed = s.get("elapsed", 0) if s else 0

    # Check stuck
    if prog == last_prog and status == "downloading":
        stuck_count += 1
    else:
        stuck_count = 0
    last_prog = prog

    # Clean the display: strip non-ascii
    safe_fname = fname.encode('ascii', 'replace').decode()
    safe_status = status.encode('ascii', 'replace').decode()
    print(f"[{i*0.5:4.1f}s] {safe_status:12s} | {pct:6.2f}% ({prog}/{total}) | {spd/1048576:7.1f} MB/s | {safe_fname}")

    # Backend property checks
    b_inst = backend.installing
    b_prog = backend.installProgress
    b_total = backend.installTotal

    if status in ("done", "failed", "cancelled"):
        done = True
        break

print(f"\n{'='*60}")
print("RESULTS:")
print(f"  Status:     {status}")
print(f"  Progress:   {prog}/{total} ({pct:.2f}%)")
print(f"  Speed max:  {spd/1048576:.1f} MB/s")
print(f"  Elapsed:    {elapsed:.1f}s")
print(f"  Errors:     {errors if errors else 'None'}")
print(f"  Done:       {done}")
print(f"  Backend:    installing={b_inst} prog={b_prog}/{b_total}")

# Verify files on disk
ver_dir = os.path.expandvars(r"%USERPROFILE%\.shadow\minecraft\versions") + "\\" + TEST_VERSION
jar = ver_dir + "\\" + TEST_VERSION + ".jar"
jsonf = ver_dir + "\\" + TEST_VERSION + ".json"
print(f"  Jar exists: {os.path.exists(jar)} ({os.path.getsize(jar) if os.path.exists(jar) else 0} bytes)")
print(f"  JSON exists:{os.path.exists(jsonf)}")
print(f"{'='*60}")

time.sleep(3)
