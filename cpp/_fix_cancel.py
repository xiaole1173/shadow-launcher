#!/usr/bin/env python3
"""Update cancelModLoaderInstall to also cancel merged context installers."""

with open('src/backend/version_backend.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Find the cancelModLoaderInstall function
marker = 'void VersionBackend::cancelModLoaderInstall() {'
func_start = content.index(marker)

# Find the end: after the last setInstalling/emit logMessage
end_marker = '    emit logMessage(tr("安装已取消"));'
end_idx = content.index(end_marker, func_start) + len(end_marker)

old_func = content[func_start:end_idx]

# Add merged context cancellation before "setInstalling(false)"
set_installing_idx = old_func.rindex('    setInstalling(false);')

new_func = (
    old_func[:set_installing_idx] +
    '    // Cancel merged context installers\n'
    '    for (auto it = m_mergedContexts.begin(); it != m_mergedContexts.end(); ++it) {\n'
    '        if (it.value() && it.value()->installer) it.value()->installer->cancel();\n'
    '        if (it.value() && it.value()->mcDownloader) it.value()->mcDownloader->cancel();\n'
    '    }\n\n'
    '    // Mark merged sessions as cancelled\n'
    '    for (auto it = m_mergedContexts.begin(); it != m_mergedContexts.end(); ++it) {\n'
    '        if (auto* ds = dlSession(it.key())) ds->markFailed(tr("已取消"));\n'
    '    }\n\n'
    + old_func[set_installing_idx:]
)

content = content.replace(old_func, new_func, 1)

with open('src/backend/version_backend.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("cancelModLoaderInstall updated successfully")
