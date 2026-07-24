#!/usr/bin/env python3
"""Insert pure MC step completion marking in onVersionDownloadFinished success path"""
with open('src/backend/version_backend.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Insert before line 2337 (refreshInstalled();)
insert_code = (
    '\n'
    '        // ── Pure MC: mark verify step as completed ──\n'
    '        auto* pureFinishDs = dlSession(finishedId);\n'
    '        if (pureFinishDs && !pureFinishDs->isMerged()) {\n'
    '            int verifyIdx = 3;\n'
    '            if (verifyIdx < pureFinishDs->steps.size())\n'
    '                updateStep(finishedId, verifyIdx, QStringLiteral("completed"), 100);\n'
    '        }\n'
    '\n'
)

# Line 2336 is empty, 2337 is refreshInstalled()
# We want to insert BEFORE refreshInstalled()
new_lines = lines[:2336] + [insert_code] + lines[2337:]

with open('src/backend/version_backend.cpp', 'w', encoding='utf-8') as f:
    f.writelines(new_lines)

print('Edit applied successfully')
