"""Scan all QML files for onClicked handlers missing toast feedback."""
import os, re, glob

qml_root = 'shadow_launcher/ui/qml'
files = glob.glob(os.path.join(qml_root, '*.qml'))

missing = []

for fpath in files:
    fname = os.path.basename(fpath)
    with open(fpath, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()
        lines = content.split('\n')

    # Find blocks that contain "onClicked:" 
    for i, line in enumerate(lines):
        if 'onClicked:' not in line:
            continue
        # Get surrounding context (5 lines before, 2 after)
        start = max(0, i-5)
        end = min(len(lines), i+3)
        ctx = '\n'.join(lines[start:end])
        
        # Check if this onClicked already has toast
        has_toast = 'toastManager.show' in ctx or 'showToast(' in ctx
        
        # Check if this is a meaningful action (not just setting a property)
        # Find the label/text associated with this button
        label = ''
        for j in range(max(0, i-10), i):
            if 'text:' in lines[j]:
                m = re.search(r'text:\s*"([^"]+)"', lines[j])
                if m: label = m.group(1)
        
        if label and not has_toast:
            # Check if it's an actual user action (calls backend.* or does something)
            action_match = re.search(r'onClicked:\s*\{([^}]+)\}', ctx, re.DOTALL)
            if action_match:
                action = action_match.group(1).strip()
                if 'backend.' in action or action.strip():
                    missing.append((fname, i+1, label, action[:80]))

for fname, line, label, action in missing:
    print(f'{fname}:{line} | {label} | {action}')
    print()

print(f'\nTotal: {len(missing)} buttons without toast')
