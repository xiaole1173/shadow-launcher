"""
Button Classification Scanner
Classifies inline Rectangle+MouseArea items by type
"""
import os, re

QML_DIR = r'D:\latest-code\cpp\qml'
OUTPUT = r'C:\Users\蔡朝彬\.openclaw\workspace\ui_buttons_classification.txt'

SKIP_FILES = {'ShadowDropdown.qml', 'RefreshButton.qml', 'ShadowButton.qml'}

def get_block(c, start):
    """Get matching brace block from position"""
    depth = 1
    i = start
    while i < len(c) and depth > 0:
        if c[i] == '{': depth += 1
        elif c[i] == '}': depth -= 1
        i += 1
    return c[start:i]

def classify_block(block, line_no):
    """Classify an inline Rectangle block"""
    b = block[:600]  # enough
    
    # Skip if inside unified component
    if any(x in block for x in ['RefreshButton', 'ShadowButton', 'ShadowDropdown']):
        return None
    
    # Check dimensions
    w = re.search(r'\bwidth\s*:\s*(\d+)', b)
    h = re.search(r'\bheight\s*:\s*(\d+)', b)
    has_fill = 'fillWidth' in b or 'fillHeight' in b or 'anchors.fill' in b
    w_val = int(w.group(1)) if w else 0
    h_val = int(h.group(1)) if h else 0
    
    # Count child elements
    text_count = len(re.findall(r'\bText\s*\{', b))
    img_count = len(re.findall(r'\bImage\s*\{', b))
    has_layout = 'RowLayout' in b or 'ColumnLayout' in b or 'GridLayout' in b
    has_child_rect = len(re.findall(r'Rectangle\s*\{', b)) > 1
    has_text_input = 'TextInput' in b or 'TextField' in b
    has_checked = 'checked' in b or 'selected' in b
    has_text_label = bool(re.search(r'text\s*:\s*["\']', b))
    has_cursor = 'cursorShape' in b
    has_mouse = 'MouseArea' in b
    
    # ── Rule chain (narrow to broad) ──
    
    # E: Checkbox/Radio
    if w_val <= 20 and h_val <= 20 and (has_checked or 'checked' in b):
        return ('E', f'{w_val}x{h_val} checkbox')
    
    # F: Toggle/Switch
    if w_val > h_val * 1.3 and w_val >= 28 and h_val <= 28 and not has_layout and text_count <= 1:
        return ('F', f'{w_val}x{h_val} toggle')
    
    # A: Icon Button (small, single text or image)
    if 24 <= w_val <= 36 and 24 <= h_val <= 36 and not has_layout and not has_child_rect:
        if text_count <= 1 and img_count <= 1:
            snippet = b[:200].replace('\n', ' ')
            # Check if it has a single icon char or SVG
            has_icon_text = bool(re.search(r'text\s*:\s*["\'][✕✚➕➖↻+×✖✓✔▲▼△▽⟨⟩\u25B6\u25C0\u25B2\u25BC\u25CF\u2190\u2191\u2192\u2193]{1,2}', b))
            has_svg = 'source:' in b and 'svg' in b
            if has_icon_text or has_svg or text_count <= 1:
                return ('A', f'{w_val}x{h_val} icon btn')
    
    # G: Background container
    if has_fill and not has_text_label and not has_checked and text_count <= 1:
        return ('G', 'container')
    
    # D: Segment/Pill (multiple in a row, has checked state or filter context)
    if 26 <= h_val <= 36 and has_checked:
        return ('D', f'{w_val}x{h_val} segment/filter')
    
    # B: Text Button
    if 26 <= h_val <= 44 and has_text_label and text_count <= 2 and not has_layout:
        return ('B', f'{w_val}x{h_val} text btn')
    
    # C: Card/List item / complex
    if h_val > 44 or has_layout or has_child_rect or has_text_input:
        return ('C', f'{w_val}x{h_val} card')
    
    # Fallback: if has cursor shape and mouse area, it's some kind of button
    if has_cursor and has_mouse:
        if has_text_label:
            return ('B', f'{w_val}x{h_val} text btn?')
        elif 24 <= w_val <= 32 and 24 <= h_val <= 32:
            return ('A', f'{w_val}x{h_val} icon?')
        else:
            return ('?', f'{w_val}x{h_val} unknown')
    
    return None

def main():
    files = sorted([os.path.join(QML_DIR, f) for f in os.listdir(QML_DIR) if f.endswith('.qml') and f not in SKIP_FILES])
    
    results = {}  # file -> {cat: [(line, detail)]}
    cat_counts = {'A':0,'B':0,'C':0,'D':0,'E':0,'F':0,'G':0,'?':0}
    size_dist = {}  # for category A
    
    for filepath in files:
        fname = os.path.relpath(filepath, QML_DIR)
        c = open(filepath, 'r', encoding='utf-8').read()
        
        file_cats = {}
        
        for m in re.finditer(r'Rectangle\s*\{', c):
            start = m.start()
            block = get_block(c, start)
            
            if 'cursorShape: Qt.PointingHandCursor' not in block and 'MouseArea' not in block:
                continue
            if 'ShadowDropdown' in block or 'RefreshButton' in block or 'ShadowButton' in block:
                continue
            
            line_no = c[:start].count('\n') + 1
            result = classify_block(block, line_no)
            
            if result:
                cat, detail = result
                file_cats.setdefault(cat, []).append((line_no, detail))
                cat_counts[cat] = cat_counts.get(cat, 0) + 1
                
                if cat == 'A':
                    w = re.search(r'\bwidth\s*:\s*(\d+)', block[:200])
                    if w:
                        sz = w.group(1)
                        size_dist[sz] = size_dist.get(sz, 0) + 1
        
        if file_cats:
            results[fname] = file_cats
    
    # Generate report
    lines = []
    lines.append("=" * 65)
    lines.append("  QML 按钮分类审计")
    lines.append(f"  扫描文件: {len(files)} 个")
    lines.append(f"  扫描结果: {sum(cat_counts.values())} 个内联按钮")
    lines.append("=" * 65)
    lines.append("")
    
    lines.append("── 分类总计 ──")
    lines.append("")
    cat_names = {
        'A': 'A: 纯图标按钮 (24~36px, 单图标)  ✅ 适合统一为ShadowButton',
        'B': 'B: 文字按钮 (26~44px, 有文字标签)  ✅ 适合统一为ShadowButton',
        'C': 'C: 卡片/列表项 (>44px 或含布局)  ❌ 保持不动',
        'D': 'D: 功能段/筛选 (带状态判断)  ⚠️ 可考虑统一',
        'E': 'E: 复选框 (16~20px)  ❌ 保持',
        'F': 'F: 滑动开关 (宽>高, 布尔态)  ⚠️ 可考虑替换为Switch组件',
        'G': 'G: 背景/容器 (fill)  ❌ 保持',
        '?': '?: 未能分类'
    }
    for cat in ['A','B','C','D','E','F','G','?']:
        cnt = cat_counts.get(cat, 0)
        if cnt > 0:
            lines.append(f"  {cat_names[cat]}: {cnt}")
    
    lines.append(f"\n  高优先级(A+B): {cat_counts.get('A',0) + cat_counts.get('B',0)}")
    lines.append(f"  中优先级(D+F): {cat_counts.get('D',0) + cat_counts.get('F',0)}")
    lines.append(f"  低/不动(C+E+G+?): {cat_counts.get('C',0) + cat_counts.get('E',0) + cat_counts.get('G',0) + cat_counts.get('?',0)}")
    
    if size_dist:
        lines.append(f"\n  图标按钮(A)尺寸分布: {', '.join(f'{k}px: {v}' for k,v in sorted(size_dist.items()))}")
    
    lines.append("")
    lines.append("─" * 65)
    lines.append("")
    lines.append("── 按文件详情 ──")
    lines.append("")
    
    for fname in sorted(results.keys()):
        fc = results[fname]
        summary = ' | '.join(f'{cat}={len(items)}' for cat, items in sorted(fc.items()))
        lines.append(f"📄 {fname}: {summary}")
        
        for cat in sorted(fc.keys()):
            for line_no, detail in fc[cat]:
                lines.append(f"    L{line_no} [{cat}] {detail}")
        
        lines.append("")
    
    with open(OUTPUT, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    
    print(f"Done! {sum(cat_counts.values())} items classified")
    print(f"A={cat_counts.get('A',0)} B={cat_counts.get('B',0)} C={cat_counts.get('C',0)} D={cat_counts.get('D',0)} E/F/G/?={cat_counts.get('E',0)+cat_counts.get('F',0)+cat_counts.get('G',0)+cat_counts.get('?',0)}")
    print(f"Report: {OUTPUT}")

if __name__ == '__main__':
    main()
