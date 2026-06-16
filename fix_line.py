import sys
sys.stdout.reconfigure(encoding='utf-8')

path = r'C:\Users\蔡朝彬\.openclaw\workspace\shadow-launcher\shadow_launcher\ui\qml\MainWindow.qml'
with open(path, 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Fix line 1504 (0-indexed 1503)
new = '                                            onClicked: { if (!currentSelectedVersion) { showToast("请先选择一个版本"); return }; if (backend) backend.repairVersion(currentSelectedVersion) }\n'
lines[1503] = new

with open(path, 'w', encoding='utf-8') as f:
    f.writelines(lines)
print('Fixed line 1504')
