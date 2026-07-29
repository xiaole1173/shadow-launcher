#!/usr/bin/env python3
"""Refactor installOptifine to use MergedInstallContext, remove standalone path, update helpers."""

import sys
sys.stdout.reconfigure(encoding="utf-8")

with open("D:/latest-code/cpp/src/backend/version_backend.cpp", "r", encoding="utf-8") as f:
    content = f.read()

# ============================================================
# 1. Replace installOptifine function
# ============================================================

old_install_optifine = '''void VersionBackend::installOptifine(const QString& mcVersion, const QString& optifineVersion,

        const QString& forgeVersion, const QString& installName,

        const QString& bmclType, const QString& bmclPatch) {

    auto* ml = createLoaderInstaller(installName);
    if (!ml) return;



    // FIX: Set installName so progress signals are not dropped


    ensureSession(installName);
    auto* ds = dlSession(installName);
    if (ds) ds->clearFailure();



    // Ensure vanilla MC is installed first

    if (!installedIds().contains(mcVersion) && !m_activeIds.contains(mcVersion)) {

        qDebug() << "[install] Vanilla MC" << mcVersion << "not installed for Optifine, downloading first...";

        ds->setMerged(true);

        ds->mcVersion = mcVersion;

        ds->loaderType = QStringLiteral("optifine");

        ds->loaderVer = optifineVersion;

        ds->bmclType = bmclType;

        ds->bmclPatch = bmclPatch;

        ds->hasPendingLoader = true;

        ds->pendingLoaderMc = mcVersion;

        ds->pendingLoaderType = QStringLiteral("optifine");

        ds->pendingLoaderVer = optifineVersion;

        ds->pendingLoaderName = installName;

        // Reset byte accumulators for merged install

        for (int i = 0; i < 3; i++) { ds->mcStepDone[i] = 0; ds->mcStepTotal[i] = 0; }

        ds->mcFileAdded.clear();



        // Build 5-step card: MC JSON + MC libs + MC assets + OptiFine JAR + Install

        rebuildSteps(installName, {

            tr("下载原版 JSON 文件"),

            tr("下载原版支持库文件"),

            tr("下载原版资源文件"),

            tr("下载 OptiFine 主文件"),

            tr("安装 OptiFine")

        }, {1.0, 8.0, 5.0, 3.0, 1.0},

         {true, true, true, true, false});  // step 4 (install) hidden until downloads done



        updateStep(installName, 0, QStringLiteral("active"), 0);

        ds->loadedStep = 1;



        setInstalling(true);

        setInstallPhase(tr("下载中..."));



        // Start MC and OptiFine JAR downloads in parallel

        ds->optifineJarParallel = true;

        installVersion(mcVersion);

        startOptifineJarParallel(installName, mcVersion, optifineVersion, bmclType, bmclPatch);

        return;

    }



    // MC already installed — 2-step OptiFine only
    ds->mcVersion = mcVersion;

    rebuildSteps(installName, {

        tr("下载 OptiFine 主文件"),

        tr("安装 OptiFine")

    }, {3.0, 1.0}, {true, true});

    updateStep(installName, 0, QStringLiteral("active"), 0);



    ml->setGameDir(m_gameDir);

    // Respect version isolation for mods/ output

    if (m_isolation && m_isolation->isVersionIsolated(installName)) {

        ml->setModsDir(m_isolation->getVersionGameDir(installName) + "/mods");

    } else {

        ml->setModsDir(QString());  // reset to default (gameDir/mods)

    }



    // Preflight: connectivity test (use manifestUrl, not the actual download URL — HEAD blocked by CDN)

    auto* sb = qobject_cast<ShadowBackend*>(parent());

    auto* coord = new DownloadCoordinator(this);

    int listSrc = sb ? sb->listDownloadSource() : 1;  // 0=镜像, 1=官方, 2=自动(双源)



    if (listSrc == 0) {

        // 仅镜像

        coord->addSource(QStringLiteral("bmclapi"), MirrorSource::bmclapi().manifestUrl);

    } else if (listSrc == 1) {

        // 仅官方

        coord->addSource(QStringLiteral("mojang"), MirrorSource::mojang().manifestUrl);

    } else {

        // 双源竞速（默认行为）

        coord->addSource(QStringLiteral("bmclapi"), MirrorSource::bmclapi().manifestUrl);

        coord->addSource(QStringLiteral("mojang"), MirrorSource::mojang().manifestUrl);

    }

    setInstallPhase(tr("连通性测试中..."));

    setInstalling(true);

'

# We'll use a simpler approach - find exact boundaries and replace
assert 'void VersionBackend::installOptifine(const QString& mcVersion, const QString& optifineVersion,' in content, "installOptifine start not found"

start_idx = content.index('void VersionBackend::installOptifine(const QString& mcVersion, const QString& optifineVersion,')
end_idx = content.index('\nvoid VersionBackend::finishOptifineMerged(', start_idx)

new_install_optifine = '''void VersionBackend::installOptifine(const QString& mcVersion, const QString& optifineVersion,

        const QString& forgeVersion, const QString& installName,

        const QString& bmclType, const QString& bmclPatch) {

    // Create MergedInstallContext for this install

    auto* ctx = createMergedContext(installName, mcVersion, "optifine", optifineVersion);
    if (!ctx) return;

    ctx->installer->setGameDir(m_gameDir);
    if (m_isolation && m_isolation->isVersionIsolated(installName)) {
        ctx->installer->setModsDir(m_isolation->getVersionGameDir(installName) + "/mods");
    } else {
        ctx->installer->setModsDir(QString());
    }

    ensureSession(installName);
    auto* ds = dlSession(installName);
    if (ds) ds->clearFailure();

    ds->setMerged(true);
    ds->mcVersion = mcVersion;
    ds->loaderType = QStringLiteral("optifine");
    ds->loaderVer = optifineVersion;
    ds->bmclType = bmclType;
    ds->bmclPatch = bmclPatch;

    // Reset byte accumulators
    for (int i = 0; i < 3; i++) { ds->mcStepDone[i] = 0; ds->mcStepTotal[i] = 0; }
    ds->mcFileAdded.clear();

    // Build 5-step card
    rebuildSteps(installName, {
        tr("下载原版 JSON 文件"),
        tr("下载原版支持库文件"),
        tr("下载原版资源文件"),
        tr("下载 OptiFine 主文件"),
        tr("安装 OptiFine")
    }, {1.0, 8.0, 5.0, 3.0, 1.0},
     {true, true, true, true, false});

    updateStep(installName, 0, QStringLiteral("active"), 0);
    ds->loadedStep = 1;
    setInstalling(true);
    setInstallPhase(tr("下载中..."));

    // Start MC and OptiFine JAR downloads in parallel
    ds->optifineJarParallel = true;
    installVersion(mcVersion);
    startOptifineJarParallel(installName, mcVersion, optifineVersion, bmclType, bmclPatch);
}'''

content = content[:start_idx] + new_install_optifine + content[end_idx:]

# ============================================================
# 2. Update onParallelOptifineDone to use merged context
# ============================================================

old_parallel_done = '''void VersionBackend::onParallelOptifineDone(const QString& installName, const QByteArray& jarData) {

    ensureSession(installName);

    auto* ds = dlSession(installName);



    // Guard against double invocation (MC completion + pending loader both trigger)

    // Guard against double invocation: if install was already triggered, skip
    if (ds->hasPendingLoader) ds->hasPendingLoader = false;
    if (ds->optifineInstallTriggered) return;



    // Store JAR data (in case MC is still downloading)

    if (!jarData.isEmpty()) {

        ds->optifineJarData = jarData;

    }

    ds->optifineJarDone = true;



    // Check if MC is also done

    if (!ds->mcDownloadDone) {

        emit logMessage(tr("OptiFine JAR 下载完成，等待 MC 下载..."));

        return;

    }



    // Both MC and OptiFine JAR are done

    emit logMessage(tr("[完成] MC 和 OptiFine 均下载完成，开始安装..."));

    QByteArray data = ds->optifineJarData.isEmpty() ? jarData : ds->optifineJarData;

    delegateOptifineInstall(ds->mcVersion, installName, data);

}'''

new_parallel_done = '''void VersionBackend::onParallelOptifineDone(const QString& installName, const QByteArray& jarData) {

    ensureSession(installName);

    auto* ds = dlSession(installName);



    // Guard against double invocation
    if (ds->optifineInstallTriggered) return;

    ds->optifineInstallTriggered = true;

    ds->optifineJarDone = true;

    if (!jarData.isEmpty()) ds->optifineJarData = jarData;



    // Use merged context for coordination
    auto* ctx = mergedContext(installName);
    if (ctx) {
        ctx->loaderJarReady = true;
        // Check if MC is also done
        if (!ctx->mcDownloadDone) {
            emit logMessage(tr("OptiFine JAR 下载完成，等待 MC 下载..."));
            return;
        }
    } else if (!ds->mcDownloadDone) {
        emit logMessage(tr("OptiFine JAR 下载完成，等待 MC 下载..."));
        return;
    }



    // Both MC and OptiFine JAR are done

    emit logMessage(tr("[完成] MC 和 OptiFine 均下载完成，开始安装..."));

    QByteArray data = ds->optifineJarData.isEmpty() ? jarData : ds->optifineJarData;

    delegateOptifineInstall(ds->mcVersion, installName, data);

}'''

assert old_parallel_done in content, "onParallelOptifineDone old text not found"
content = content.replace(old_parallel_done, new_parallel_done, 1)

# ============================================================
# 3. Remove finishOptifineMerged (no longer needed — MC done goes through onVersionDownloadFinished merged loop)
# ============================================================

if 'void VersionBackend::finishOptifineMerged' in content:
    start_fm = content.index('void VersionBackend::finishOptifineMerged')
    end_fm = content.index('\nvoid VersionBackend::delegateOptifineInstall(', start_fm)
    content = content[:start_fm] + content[end_fm:]

with open("D:/latest-code/cpp/src/backend/version_backend.cpp", "w", encoding="utf-8") as f:
    f.write(content)

print("installOptifine refactored successfully")
print(f"- Removed standalone 2-step path")
print(f"- onParallelOptifineDone uses mergedContext")
print(f"- finishOptifineMerged removed")
