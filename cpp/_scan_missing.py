"""
Missing Patterns Scanner
Detects UI patterns the first scan might have missed:
- Switches/Toggles
- Sliders
- Segmented/ButtonGroup/TabBar
- Custom Checkboxes/RadioButtons
- Tooltips
- BusyIndicator/Spinner
- ScrollBar styling
- Combo boxes that aren't ShadowDropdown
- Custom Dropdown indicators (▼)
- Inline SVG icons used as buttons
"""
import os, re

QML_DIR = r'D:\latest-code\cpp\qml'
OUTPUT = r'C:\Users\蔡朝彬\.openclaw\workspace\ui_missing_patterns.txt'

SKIP_FILES = {'ShadowDropdown.qml', 'RefreshButton.qml', 'ShadowButton.qml'}

def analyze_file(filepath):
    c = open(filepath, 'r', encoding='utf-8').read()
    fname = os.path.relpath(filepath, QML_DIR)
    findings = []
    
    # 1. Native Switch
    for m in re.finditer(r'Switch\s*\{', c):
        line = c[:m.start()].count('\n') + 1
        findings.append(('Switch组件', line, 'Native Switch — 是否有统一 ShadowSwitch?'))
    
    # 2. Custom toggle (Rectangle with checked/toggled, not checkbox-sized)
    # Find clickable Rectangles that toggle state
    for m in re.finditer(r'onClicked\s*:\s*\{[^}]*?(?:checked\s*=\s*!|toggled\s*=\s*!|switch|toggle)', c, re.DOTALL):
        line = c[:m.start()].count('\n') + 1
        ctx = c[max(0,m.start()-80):m.end()+50].replace('\n',' ')
        if 'width: 16' not in ctx and 'width: 18' not in ctx:  # not checkbox
            findings.append(('自定义Toggle', line, ctx[:200]))
    
    # 3. Slider
    for m in re.finditer(r'Slider\s*\{', c):
        line = c[:m.start()].count('\n') + 1
        findings.append(('Slider', line, 'Native Slider'))
    
    # 4. ButtonGroup / TabBar
    for m in re.finditer(r'ButtonGroup\s*\{|TabBar\s*\{|TabButton\s*\{', c):
        line = c[:m.start()].count('\n') + 1
        findings.append(('TabBar/ButtonGroup', line, ''))
    
    # 5. Custom segmented control (multiple adjacent small Rectangles with same height)
    rects = []
    for m in re.finditer(r'Rectangle\s*\{', c):
        start = m.start()
        depth = 1
        i = m.end()
        while i < len(c) and depth > 0:
            if c[i] == '{': depth += 1
            elif c[i] == '}': depth -= 1
            i += 1
        block = c[start:i]
        rects.append((start, block))
    
    # Find adjacent same-height Rectangles (potential segmented controls)
    btn_groups = []
    for i in range(len(rects)):
        r = rects[i]
        h = re.search(r'\bheight\s*:\s*(\d+)', r[1][:200])
        has_cursor = 'cursorShape' in r[1]
        if h and has_cursor:
            h_val = int(h.group(1))
            # Check if next rect is nearby with same height
            if i+1 < len(rects):
                r2 = rects[i+1]
                h2 = re.search(r'\bheight\s*:\s*(\d+)', r2[1][:200])
                if h2 and int(h2.group(1)) == h_val:
                    line = c[:r[0]].count('\n') + 1
                    btn_groups.append((line, h_val))
    
    # Deduplicate groups
    seen_lines = set()
    for line, h_val in btn_groups:
        if line not in seen_lines:
            seen_lines.add(line)
            findings.append(('疑似分段按钮/选项组', line, f'高度{h_val}px, 多个并列'))
    
    # 6. ToolTip
    for m in re.finditer(r'ToolTip\s*\{', c):
        line = c[:m.start()].count('\n') + 1
        ctx = c[m.start():m.end()+80].replace('\n',' ')
        findings.append(('ToolTip', line, ctx[:150]))
    
    # 7. BusyIndicator / Spinner
    for m in re.finditer(r'BusyIndicator\s*\{', c):
        line = c[:m.start()].count('\n') + 1
        findings.append(('BusyIndicator', line, ''))
    
    # 8. Custom spinner (animated rotation icon)
    for m in re.finditer(r'NumberAnimation\s*\{[^}]*?rotation.*?360|rotation\s*:\s*[0-9].*?Animation.*?running', c, re.DOTALL):
        line = c[:m.start()].count('\n') + 1
        ctx = c[max(0,m.start()-80):m.end()+80].replace('\n',' ')
        findings.append(('自定义旋转动画(Spinner?)', line, ctx[:200]))
    
    # 9. ▼ dropdown indicators (inline, not ShadowDropdown)
    for m in re.finditer(r'["\']\u25BE["\']|["\']\\u25BE["\']|["\']▼["\']', c):
        line = c[:m.start()].count('\n') + 1
        ctx = c[max(0,m.start()-100):m.end()+50].replace('\n',' ')
        # Check if it's inside ShadowDropdown
        before = c[max(0,m.start()-200):m.start()]
        if 'ShadowDropdown' not in before:
            findings.append(('内联▼下拉箭头', line, ctx[:200]))
    
    # 10. ScrollView/ScrollBar customization
    for m in re.finditer(r'ScrollBar\s*\{[^}]*?policy|ScrollBar\s*\{[^}]*?color|ScrollBar\s*\{[^}]*?width', c, re.DOTALL):
        line = c[:m.start()].count('\n') + 1
        ctx = c[m.start():m.end()+100].replace('\n',' ')
        findings.append(('自定义ScrollBar', line, ctx[:200]))
    
    return findings

def main():
    files = sorted([os.path.join(QML_DIR, f) for f in os.listdir(QML_DIR) if f.endswith('.qml') and f not in SKIP_FILES])
    
    type_counts = {}
    all_findings = {}
    
    for filepath in files:
        fname = os.path.relpath(filepath, QML_DIR)
        findings = analyze_file(filepath)
        if findings:
            all_findings[fname] = findings
            for t, line, ctx in findings:
                type_counts[t] = type_counts.get(t, 0) + 1
    
    lines = []
    lines.append("=" * 65)
    lines.append("  遗漏模式扫描")
    lines.append(f"  扫描文件: {len(files)} 个")
    lines.append("=" * 65)
    lines.append("")
    
    lines.append("── 按类型统计 ──")
    lines.append("")
    for t, cnt in sorted(type_counts.items(), key=lambda x: -x[1]):
        lines.append(f"  {t}: {cnt}")
    lines.append("")
    
    lines.append("─" * 65)
    lines.append("")
    
    for fname in sorted(all_findings.keys()):
        lines.append(f"\n📄 {fname}:")
        for t, line, ctx in all_findings[fname]:
            lines.append(f"  L{line} [{t}] {ctx[:200]}")
    
    with open(OUTPUT, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    
    print(f"Done! Found {sum(type_counts.values())} items")
    for t, cnt in sorted(type_counts.items(), key=lambda x: -x[1]):
        print(f"  {t}: {cnt}")
    print(f"Report: {OUTPUT}")

if __name__ == '__main__':
    main()
