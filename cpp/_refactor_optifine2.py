#!/usr/bin/env python3
"""Refactor installOptifine: use MergedInstallContext, remove standalone."""
import sys
sys.stdout.reconfigure(encoding="utf-8")

with open("D:/latest-code/cpp/src/backend/version_backend.cpp", "r", encoding="utf-8") as f:
    content = f.read()

# 1. Replace installOptifine function
start = content.index('void VersionBackend::installOptifine(const QString& mcVersion, const QString& optifineVersion,')
end = content.index('\nvoid VersionBackend::finishOptifineMerged(', start)

new_func = (
    'void VersionBackend::installOptifine(const QString& mcVersion, const QString& optifineVersion,\n'
    '\n'
    '        const QString& forgeVersion, const QString& installName,\n'
    '\n'
    '        const QString& bmclType, const QString& bmclPatch) {\n'
    '\n'
    '    // Create MergedInstallContext\n'
    '    auto* ctx = createMergedContext(installName, mcVersion, "optifine", optifineVersion);\n'
    '    if (!ctx) return;\n'
    '\n'
    '    ctx->installer->setGameDir(m_gameDir);\n'
    '    if (m_isolation && m_isolation->isVersionIsolated(installName)) {\n'
    '        ctx->installer->setModsDir(m_isolation->getVersionGameDir(installName) + "/mods");\n'
    '    } else {\n'
    '        ctx->installer->setModsDir(QString());\n'
    '    }\n'
    '\n'
    '    ensureSession(installName);\n'
    '    auto* ds = dlSession(installName);\n'
    '    if (ds) ds->clearFailure();\n'
    '\n'
    '    ds->setMerged(true);\n'
    '    ds->mcVersion = mcVersion;\n'
    '    ds->loaderType = QStringLiteral("optifine");\n'
    '    ds->loaderVer = optifineVersion;\n'
    '    ds->bmclType = bmclType;\n'
    '    ds->bmclPatch = bmclPatch;\n'
    '\n'
    '    for (int i = 0; i < 3; i++) { ds->mcStepDone[i] = 0; ds->mcStepTotal[i] = 0; }\n'
    '    ds->mcFileAdded.clear();\n'
    '\n'
    '    rebuildSteps(installName, {\n'
    '        tr("下载原版 JSON 文件"),\n'
    '        tr("下载原版支持库文件"),\n'
    '        tr("下载原版资源文件"),\n'
    '        tr("下载 OptiFine 主文件"),\n'
    '        tr("安装 OptiFine")\n'
    '    }, {1.0, 8.0, 5.0, 3.0, 1.0},\n'
    '     {true, true, true, true, false});\n'
    '\n'
    '    updateStep(installName, 0, QStringLiteral("active"), 0);\n'
    '    ds->loadedStep = 1;\n'
    '    setInstalling(true);\n'
    '    setInstallPhase(tr("下载中..."));\n'
    '\n'
    '    ds->optifineJarParallel = true;\n'
    '    installVersion(mcVersion);\n'
    '    startOptifineJarParallel(installName, mcVersion, optifineVersion, bmclType, bmclPatch);\n'
    '}\n'
)

content = content[:start] + new_func + content[end:]

# 2. Replace onParallelOptifineDone
old_par = (
    'void VersionBackend::onParallelOptifineDone(const QString& installName, const QByteArray& jarData) {\n'
    '\n'
    '    ensureSession(installName);\n'
    '\n'
    '    auto* ds = dlSession(installName);\n'
    '\n'
    '    // Guard against double invocation (MC completion + pending loader both trigger)\n'
    '    // Guard against double invocation: if install was already triggered, skip\n'
    '    if (ds->hasPendingLoader) ds->hasPendingLoader = false;\n'
    '    if (ds->optifineInstallTriggered) return;\n'
    '\n'
    '    // Store JAR data (in case MC is still downloading)\n'
    '\n'
    '    if (!jarData.isEmpty()) {\n'
    '\n'
    '        ds->optifineJarData = jarData;\n'
    '\n'
    '    }\n'
    '\n'
    '    ds->optifineJarDone = true;\n'
    '\n'
    '    // Check if MC is also done\n'
    '\n'
    '    if (!ds->mcDownloadDone) {\n'
    '\n'
    '        emit logMessage(tr("OptiFine JAR 下载完成，等待 MC 下载..."));\n'
    '\n'
    '        return;\n'
    '\n'
    '    }\n'
    '\n'
    '    // Both MC and OptiFine JAR are done\n'
    '\n'
    '    emit logMessage(tr("[完成] MC 和 OptiFine 均下载完成，开始安装..."));\n'
    '\n'
    '    QByteArray data = ds->optifineJarData.isEmpty() ? jarData : ds->optifineJarData;\n'
    '\n'
    '    delegateOptifineInstall(ds->mcVersion, installName, data);\n'
    '\n'
    '}\n'
)

new_par = (
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

if old_par in content:
    content = content.replace(old_par, new_par, 1)
    print("onParallelOptifineDone updated")
else:
    print("WARNING: onParallelOptifineDone old text not found!")

# 3. Remove finishOptifineMerged
fm = 'void VersionBackend::finishOptifineMerged'
if fm in content:
    fm_start = content.index(fm)
    fm_end = content.index('\nvoid VersionBackend::delegateOptifineInstall(', fm_start)
    content = content[:fm_start] + content[fm_end:]
    print("finishOptifineMerged removed")

with open("D:/latest-code/cpp/src/backend/version_backend.cpp", "w", encoding="utf-8") as f:
    f.write(content)

print("Done")
