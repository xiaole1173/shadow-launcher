#!/usr/bin/env python3
"""Insert pure MC branch into updateDownloadProgress()"""
with open('src/backend/version_backend.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Line 3517 = close of merged block, 3518-3520 = empty, 3521 = primary download comment
insert_code = (
    '    // ── Pure MC: route category progress into ds->steps via updateStep ──\n'
    '    if (mergedSessionId.isEmpty()) {\n'
    '        auto* pureDs = dlSession(versionId);\n'
    '        if (pureDs && !pureDs->isMerged() && pureDs->steps.size() >= 4) {\n'
    '            auto& st2 = m_dlStates[versionId];\n'
    '            bool verifying = (st2.phase == tr("\u6821\u9a8c\u4e2d..."));\n'
    '            if (!verifying) {\n'
    '                // Step 0 (JSON): mark completed once any bytes flow\n'
    '                if (st2.bytesDl > 0) {\n'
    '                    QVariantMap step0 = pureDs->steps[0].toMap();\n'
    '                    if (step0["status"].toString() != QStringLiteral("completed")) {\n'
    '                        updateStep(versionId, 0, QStringLiteral("completed"), 100, st2.catBytesDl[0], st2.catBytesTotal[0]);\n'
    '                    }\n'
    '                }\n'
    '                // Step 1 (libraries): category index 1\n'
    '                {\n'
    '                    QString st1;\n'
    '                    if (st2.catBytesTotal[1] <= 0) {\n'
    '                        st1 = st2.bytesDl > 0 ? QStringLiteral("completed") : QStringLiteral("pending");\n'
    '                    } else {\n'
    '                        st1 = (st2.catBytesDl[1] >= st2.catBytesTotal[1]) ? QStringLiteral("completed") : QStringLiteral("active");\n'
    '                    }\n'
    '                    int pct1 = st2.catBytesTotal[1] > 0 ? (int)(st2.catBytesDl[1] * 100 / st2.catBytesTotal[1]) : (st2.bytesDl > 0 ? 100 : 0);\n'
    '                    updateStep(versionId, 1, st1, pct1, st2.catBytesDl[1], st2.catBytesTotal[1]);\n'
    '                }\n'
    '                // Step 2 (assets): category index 2\n'
    '                {\n'
    '                    QString st2s;\n'
    '                    if (st2.catBytesTotal[2] <= 0) {\n'
    '                        st2s = st2.bytesDl > 0 ? QStringLiteral("completed") : QStringLiteral("pending");\n'
    '                    } else {\n'
    '                        st2s = (st2.catBytesDl[2] >= st2.catBytesTotal[2]) ? QStringLiteral("completed") : QStringLiteral("active");\n'
    '                    }\n'
    '                    int pct2 = st2.catBytesTotal[2] > 0 ? (int)(st2.catBytesDl[2] * 100 / st2.catBytesTotal[2]) : (st2.bytesDl > 0 ? 100 : 0);\n'
    '                    updateStep(versionId, 2, st2s, pct2, st2.catBytesDl[2], st2.catBytesTotal[2]);\n'
    '                }\n'
    '            }\n'
    '        }\n'
    '    }\n'
    '\n'
    '\n'
)

# Insert after line 3517 + 3 empty lines, before line 3521
new_lines = lines[:3518] + ['\n', '\n', '\n', insert_code] + lines[3521:]

with open('src/backend/version_backend.cpp', 'w', encoding='utf-8') as f:
    f.writelines(new_lines)

print('Edit applied successfully')
