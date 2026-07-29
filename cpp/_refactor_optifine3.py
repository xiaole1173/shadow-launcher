#!/usr/bin/env python3
"""Replace onParallelOptifineDone only, reading actual text from file."""
import sys
sys.stdout.reconfigure(encoding="utf-8")

with open("D:/latest-code/cpp/src/backend/version_backend.cpp", "r", encoding="utf-8") as f:
    content = f.read()

# 2. Replace onParallelOptifineDone - read actual text from file
idx = content.index('void VersionBackend::onParallelOptifineDone')
end_idx = content.index('\nvoid VersionBackend::installOptifineJar(', idx)

old_text = content[idx:end_idx]

new_text = (
    'void VersionBackend::onParallelOptifineDone(const QString& installName, const QByteArray& jarData) {\n'
    '\n'
    '    ensureSession(installName);\n'
    '    auto* ds = dlSession(installName);\n'
    '\n'
    '    // Guard against double invocation\n'
    '    if (ds->optifineInstallTriggered) return;\n'
    '    ds->optifineInstallTriggered = true;\n'
    '    ds->optifineJarDone = true;\n'
    '    if (!jarData.isEmpty()) ds->optifineJarData = jarData;\n'
    '\n'
    '    // Use merged context for coordination\n'
    '    auto* ctx = mergedContext(installName);\n'
    '    if (ctx) {\n'
    '        ctx->loaderJarReady = true;\n'
    '        if (!ctx->mcDownloadDone) {\n'
    '            emit logMessage(tr("OptiFine JAR 下载完成，等待 MC 下载..."));\n'
    '            return;\n'
    '        }\n'
    '    } else if (!ds->mcDownloadDone) {\n'
    '        emit logMessage(tr("OptiFine JAR 下载完成，等待 MC 下载..."));\n'
    '        return;\n'
    '    }\n'
    '\n'
    '    emit logMessage(tr("[完成] MC 和 OptiFine 均下载完成，开始安装..."));\n'
    '    QByteArray data = ds->optifineJarData.isEmpty() ? jarData : ds->optifineJarData;\n'
    '    delegateOptifineInstall(ds->mcVersion, installName, data);\n'
    '}\n'
)

# Remove trailing whitespace from old_text
old_text = old_text.rstrip() + '\n'
content = content.replace(old_text, new_text, 1)

with open("D:/latest-code/cpp/src/backend/version_backend.cpp", "w", encoding="utf-8") as f:
    f.write(content)

print("onParallelOptifineDone updated")
