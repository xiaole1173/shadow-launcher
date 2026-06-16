import sys
sys.stdout.reconfigure(encoding='utf-8')

path = r'C:\Users\蔡朝彬\.openclaw\workspace\shadow-launcher\shadow_launcher\ui\qml\MainWindow.qml'
with open(path, 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Fix line 1568 (0-indexed 1567)
new = '                                                if (!currentSelectedVersion) { showToast("请先选择一个版本"); return }\n'
lines[1567] = new

# Fix lines 1569-1570 (0-indexed 1568-1569)
lines[1568] = '                                                confirmDialog.title = "删除版本"\n'
# line 1570 has garbled confirm message - let's fix it
lines[1569] = '                                                confirmDialog.message = "确认要删除版本 " + currentSelectedVersion + " 吗？\\n此操作不可撤销，版本的所有文件将被删除。"\n'

with open(path, 'w', encoding='utf-8') as f:
    f.writelines(lines)
print('Fixed lines 1568-1570')
