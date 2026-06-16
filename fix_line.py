import sys
sys.stdout.reconfigure(encoding='utf-8')

path = r'C:\Users\蔡朝彬\.openclaw\workspace\shadow-launcher\shadow_launcher\ui\qml\MainWindow.qml'
with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

# Fix line 923: garbled showToast text
old_prefix = 'if (!currentSelectedVersion) { showToast("'
idx = content.find(old_prefix)
if idx >= 0:
    # Find the closing of this garbled string
    end_marker = '"); return }'
    end_idx = content.index(end_marker, idx)
    new_line = 'if (!currentSelectedVersion) { showToast("请先选择一个版本"); return }'
    content = content[:idx] + new_line + content[end_idx + len(end_marker):]
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    print('Fixed line 923')
else:
    print('Prefix not found')
