#!/usr/bin/env python3
"""Replace Section 2 of doRebuildInstallCards with DownloadSession-based card building"""
with open('src/backend/version_backend.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Find Section 2 boundaries
section2_start = None
section2_end = None
for i, line in enumerate(lines):
    if line.strip() == '// 2. Version cards':
        section2_start = i
    if section2_start and i > section2_start and line.strip().startswith('// 3.'):
        section2_end = i
        break

if section2_start is None or section2_end is None:
    print('ERROR: Could not find Section 2 boundaries')
    exit(1)

print(f'Section 2: lines {section2_start+1}-{section2_end}')

# New Section 2 code
new_section2 = (
    '    // 2. Version cards — from DownloadSession for pure MC installs\n'
    '\n'
    '    for (auto sit = m_downloadSessions.constBegin(); sit != m_downloadSessions.constEnd(); ++sit) {\n'
    '\n'
    '        const QString& sid = sit.key();\n'
    '\n'
    '        if (seen.contains(sid)) continue;\n'
    '\n'
    '        auto* ds = dlSession(sid);\n'
    '\n'
    '        if (!ds) continue;\n'
    '\n'
    '        // Skip merged sessions (already built by Section 1)\n'
    '\n'
    '        if (ds->isMerged()) continue;\n'
    '\n'
    '        // Skip sessions with pending loader (shown as loader card in Section 1)\n'
    '\n'
    '        if (ds->hasPendingLoader && !ds->pendingLoaderName.isEmpty()) continue;\n'
    '\n'
    '\n'
    '\n'
    '        seen.insert(sid);\n'
    '\n'
    '\n'
    '\n'
    '        // Build card from DownloadSession data\n'
    '\n'
    '        InstallCard c = ds->toCard(sid, sid, QStringLiteral("version"));\n'
    '\n'
    '        // Ensure consistent name/type\n'
    '\n'
    '        c.name = sid;\n'
    '\n'
    '        c.type = QStringLiteral("version");\n'
    '\n'
    '        cards.append(c);\n'
    '\n'
    '    }\n'
    '\n'
    '\n'
)

# Replace from section2_start to section2_end
new_lines = lines[:section2_start] + [new_section2] + lines[section2_end:]

with open('src/backend/version_backend.cpp', 'w', encoding='utf-8') as f:
    f.writelines(new_lines)

print(f'Replaced {section2_end - section2_start} lines with new implementation')
