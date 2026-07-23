import os, re, sys

QML_DIR = r'D:\latest-code\cpp\qml'
OUTPUT_DIR = r'C:\Users\蔡朝彬\.openclaw\workspace'

# Components that are already unified
UNIFIED = {'ShadowDropdown.qml', 'RefreshButton.qml', 'ShadowButton.qml'}
# Components that use unified already (skip false positives)
UNIFIED_IMPORTS = {'ShadowDropdown', 'RefreshButton', 'ShadowButton'}

def scan_qml_files():
    files = []
    for root, dirs, fnames in os.walk(QML_DIR):
        for f in fnames:
            if f.endswith('.qml') and f not in UNIFIED:
                files.append(os.path.join(root, f))
    return sorted(files)

def is_unified_already(c):
    """Check if file already imports unified components heavily."""
    counts = {}
    for cmp in UNIFIED_IMPORTS:
        counts[cmp] = c.count(cmp)
    return counts

def analyze_file(filepath):
    filename = os.path.basename(filepath)
    c = open(filepath, 'r', encoding='utf-8').read()
    lines = c.split('\n')
    
    results = {
        'file': os.path.relpath(filepath, QML_DIR),
        'lines': len(lines),
        'patterns': [],
        'unified_usage': is_unified_already(c)
    }
    
    # ═══ Inline clickable buttons (Rectangles with MouseArea + cursorShape) ═══
    # Find Rectangle { ... cursorShape: Qt.PointingHandCursor ... }
    pattern = re.compile(r'Rectangle\s*\{', re.DOTALL)
    for m in pattern.finditer(c):
        start = m.start()
        # Find the matching closing brace (simple brace counter)
        depth = 1
        i = m.end()
        while i < len(c) and depth > 0:
            if c[i] == '{': depth += 1
            elif c[i] == '}': depth -= 1
            i += 1
        block = c[start:i]
        
        # Check if it has MouseArea + cursorShape
        if 'cursorShape: Qt.PointingHandCursor' in block and 'MouseArea' in block:
            line_no = c[:start].count('\n') + 1
            # Skip if inside an already unified component
            if any(u in block for u in ['RefreshButton', 'ShadowButton', 'ShadowDropdown']):
                continue
            # Skip obvious non-buttons (too big, or has multiple children)
            props = block[:300].replace('\n', ' ')
            w = re.search(r'width\s*:\s*(\d+)', block)
            h = re.search(r'height\s*:\s*(\d+)', block)
            dim = f"{w.group(1) if w else '?'}x{h.group(1) if h else '?'}" if w or h else 'auto'
            results['patterns'].append({
                'type': 'inline_button',
                'line': line_no,
                'snippet': props[:300],
                'dim': dim
            })
    
    # ═══ Popup items (not inside ShadowDropdown) ═══
    for m in re.finditer(r'Popup\s*\{', c):
        start = m.start()
        depth = 1
        i = m.end()
        while i < len(c) and depth > 0:
            if c[i] == '{': depth += 1
            elif c[i] == '}': depth -= 1
            i += 1
        block = c[start:i]
        line_no = c[:start].count('\n') + 1
        # Skip if inside ShadowDropdown or RefreshButton
        if 'ShadowDropdown' in block or 'RefreshButton' in block:
            continue
        ctx = block[:200].replace('\n', ' ')
        results['patterns'].append({
            'type': 'inline_popup',
            'line': line_no,
            'snippet': ctx[:250]
        })
    
    # ═══ ComboBox items ═══
    for m in re.finditer(r'ComboBox\s*\{', c):
        line_no = c[:m.start()].count('\n') + 1
        ctx_start = max(0, m.start()-80)
        ctx_end = min(len(c), m.start()+200)
        ctx = c[ctx_start:ctx_end].replace('\n', ' ')
        results['patterns'].append({
            'type': 'inline_combobox',
            'line': line_no,
            'snippet': ctx[:250]
        })
    
    # ═══ Dialog / Overlay with modality ═══
    for m in re.finditer(r'(?:Dialog|Window)\s*\{[^{}]*?modal', c, re.DOTALL):
        line_no = c[:m.start()].count('\n') + 1
        ctx_start = max(0, m.start()-60)
        ctx_end = min(len(c), m.start()+200)
        ctx = c[ctx_start:ctx_end].replace('\n', ' ')
        results['patterns'].append({
            'type': 'dialog',
            'line': line_no,
            'snippet': ctx[:250]
        })
    
    # ═══ Search bars (TextInput in Rectangle with search-related names or comments) ═══
    for m in re.finditer(r'(?:search|搜索|filter|过滤|查找)', c, re.IGNORECASE):
        line_no = c[:m.start()].count('\n') + 1
        # Get surrounding context
        ctx_start = max(0, m.start()-150)
        ctx_end = min(len(c), m.start()+150)
        ctx = c[ctx_start:ctx_end].replace('\n', ' ')
        # Only report if there's a TextInput/TextField nearby
        if 'TextInput' in ctx or 'TextField' in ctx:
            # Deduplicate by checking if we already reported this line
            existing = [p for p in results['patterns'] if p['type'] == 'search_bar' and abs(p['line'] - line_no) < 5]
            if not existing:
                results['patterns'].append({
                    'type': 'search_bar',
                    'line': line_no,
                    'snippet': ctx[:250]
                })
    
    # ═══ Native ProgressBar ═══
    for m in re.finditer(r'ProgressBar\s*\{', c):
        line_no = c[:m.start()].count('\n') + 1
        ctx_start = max(0, m.start()-60)
        ctx_end = min(len(c), m.end()+200)
        ctx = c[ctx_start:ctx_end].replace('\n', ' ')
        results['patterns'].append({
            'type': 'progress_bar',
            'line': line_no,
            'snippet': ctx[:250]
        })
    
    return results

def main():
    files = scan_qml_files()
    print(f"Scanning {len(files)} QML files...")
    
    all_results = {}
    category_counts = {}
    unified_counts = {}
    
    for f in files:
        r = analyze_file(f)
        if r['patterns'] or any(v > 0 for v in r['unified_usage'].values()):
            all_results[r['file']] = r
            for p in r['patterns']:
                t = p['type']
                category_counts[t] = category_counts.get(t, 0) + 1
            for cmp, cnt in r['unified_usage'].items():
                if cnt > 0:
                    unified_counts.setdefault(cmp, []).append((r['file'], cnt))
    
    output = []
    output.append("=" * 65)
    output.append("  QML 组件统一审计报告")
    output.append(f"  扫描文件: {len(files)} 个 QML")
    output.append("  (已排除 ShadowDropdown/RefreshButton/ShadowButton 组件本身)")
    output.append("=" * 65)
    output.append("")
    
    output.append("── 1. 当前统一组件使用情况 ──")
    output.append("")
    for cmp in ['ShadowDropdown', 'RefreshButton', 'ShadowButton']:
        usages = unified_counts.get(cmp, [])
        if usages:
            output.append(f"  {cmp}: {len(usages)} 个文件使用")
            for f, cnt in sorted(usages):
                output.append(f"    {f} ({cnt}处)")
        else:
            output.append(f"  {cmp}: 0 个文件使用")
    
    output.append("")
    output.append("── 2. 待统一模式统计 ──")
    output.append("")
    names = {
        'inline_button': '🔘 内联按钮 (Rectangle+MouseArea+cursorShape)',
        'inline_popup': '⬇️ 内联 Popup',
        'inline_combobox': '⬇️ 内联 ComboBox',
        'dialog': '🗔 Dialog/Window modal',
        'search_bar': '🔍 自定义搜索框 (TextInput)',
        'progress_bar': '📊 原生 ProgressBar'
    }
    for cat, cnt in sorted(category_counts.items(), key=lambda x: -x[1]):
        output.append(f"  {names.get(cat, cat)}: {cnt} 处")
    
    total = sum(category_counts.values()) if category_counts else 0
    output.append(f"\n  📊 总计: {total} 处待统一")
    
    output.append("")
    output.append("─" * 65)
    output.append("")
    output.append("── 3. 详情 ──")
    output.append("")
    
    for filename in sorted(all_results.keys()):
        r = all_results[filename]
        
        # Count patterns by type for this file
        file_cats = {}
        for p in r['patterns']:
            file_cats[p['type']] = file_cats.get(p['type'], 0) + 1
        cat_str = ', '.join(f"{t}={c}" for t, c in sorted(file_cats.items()))
        
        # Unified usage count
        uni_str = ''
        for cmp, cnt in sorted(r['unified_usage'].items()):
            if cnt > 0:
                uni_str += f' {cmp}={cnt}'
        
        output.append(f"\n{'─'*55}")
        output.append(f"📄 {filename} ({r['lines']} 行){f' | 已统一:{uni_str}' if uni_str else ''}")
        output.append(f"   未统一: {cat_str}" if cat_str else "   所有组件已统一 ✅")
        
        for p in r['patterns']:
            output.append(f"\n  [{p['type']}] L{p['line']}")
            if 'dim' in p:
                output.append(f"  尺寸: {p['dim']}")
            output.append(f"  {p['snippet'][:250]}")
    
    # Write to workspace (safe from git reset)
    outpath = os.path.join(OUTPUT_DIR, 'ui_unification_audit.txt')
    with open(outpath, 'w', encoding='utf-8') as f:
        f.write('\n'.join(output))
    
    print(f"\n✅ 审计报告已写入: {outpath}")
    print(f"\n分类统计:")
    for cat, cnt in sorted(category_counts.items(), key=lambda x: -x[1]):
        print(f"  {cat}: {cnt}")
    print(f"\n总文件: {len(all_results)}")

if __name__ == '__main__':
    main()
