#!/usr/bin/env python3
"""Rewrite installModLoader: remove standalone branches, use MergedInstallContext."""

import re

with open('src/backend/version_backend.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Find the installModLoader function boundaries
start_marker = 'void VersionBackend::installModLoader(const QString& mcVersion, const QString& loaderType,'
end_marker = 'void VersionBackend::installOptifine(const QString& mcVersion, const QString& optifineVersion,'

start_idx = content.index(start_marker)
end_idx = content.index(end_marker)

# The new function body
new_func = '''void VersionBackend::installModLoader(const QString& mcVersion, const QString& loaderType,
                                       const QString& loaderVersion, const QString& installName,
                                       const QString& fabricApiVersion,
                                       const QString& fabricApiUrl,
                                       const QString& fabricApiSavePath,
                                       const QString& forgeInstallerSha1,
                                       const QString& forgeInstallerBranch) {
    // Build the full Forge Maven version: {mc}-{forge} or {mc}-{forge}-{branch}
    QString m_forgeMavenVer = mcVersion + QStringLiteral("-") + loaderVersion;
    if (!forgeInstallerBranch.isEmpty())
        m_forgeMavenVer += QStringLiteral("-") + forgeInstallerBranch;

    // Create MergedInstallContext for this install
    auto* ctx = createMergedContext(installName, mcVersion, loaderType, loaderVersion);
    if (!ctx) return;

    ensureSession(installName);
    auto* ds = dlSession(installName);
    if (ds) ds->clearFailure();

    // ── Merged install: always MC + loader ──
    qDebug() << "[install] Merged install: MC" << mcVersion << "+" << loaderType << loaderVersion;

    ds->setMerged(true);
    ds->mcVersion = mcVersion;
    ds->loaderType = loaderType;
    ds->loaderVer = loaderVersion;

    // Reset byte accumulators
    ds->mcBytesDl = 0; ds->mcBytesAll = 0;
    ds->mlBytesDl = 0; ds->mlBytesAll = 0; ds->mlBytesDone = 0; ds->mlFileTotal = 0;
    for (int i = 0; i < 3; i++) { ds->mcStepDone[i] = 0; ds->mcStepTotal[i] = 0; }
    ds->mcFileAdded.clear();

    // Build step list
    QString loaderLabel = QStringLiteral("Forge");
    if (loaderType == QStringLiteral("neoforge")) loaderLabel = QStringLiteral("NeoForge");
    else if (loaderType == QStringLiteral("fabric")) loaderLabel = QStringLiteral("Fabric");

    if (loaderType == QStringLiteral("fabric")) {
        QStringList stepNames = {
            tr("下载原版 JSON 文件"),
            tr("下载原版支持库文件"),
            tr("下载原版资源文件"),
            tr("校验游戏资源完整性"),
            tr("下载 Fabric 配置"),
            tr("下载 Fabric 依赖库"),
            tr("安装 Fabric")
        };
        QVector<qreal> weights = {3.0, 8.0, 5.0, 0.5, 0.1, 2.0, 0.5};
        QVector<bool> shows = {true, true, true, true, true, true, true};
        if (!fabricApiUrl.isEmpty()) {
            stepNames.append(tr("下载 Fabric API"));
            weights.append(0.05);
            shows.append(false);
        }
        rebuildSteps(installName, stepNames, weights, shows);
    } else {
        rebuildSteps(installName, {
            tr("下载原版 JSON 文件"),
            tr("下载原版支持库文件"),
            tr("下载原版资源文件"),
            tr("校验游戏资源完整性"),
            tr("下载 %1 主文件").arg(loaderLabel),
            tr("校验 %1 完整性").arg(loaderLabel),
            tr("安装 %1").arg(loaderLabel)
        }, {3.0, 8.0, 5.0, 0.5, 6.0, 0.5, 10.0},
         {true, true, true, true, true, true, true});
    }

    updateStep(installName, 0, QStringLiteral("active"), 0);
    updateCardFromSession(installName, installName, QStringLiteral("mod_loader"));
    ds->loadedStep = 1;
    setInstalling(true);

    // ── Start MC download ──
    installVersion(mcVersion);
    
    // ── Record ctx under mcVersion for onVersionDownloadFinished routing ──
    // (m_activeIds already checked inside installVersion)

    // ── Start loader in parallel ──
    if (loaderType == QStringLiteral("fabric")) {
        ctx->installer->setGameDir(m_gameDir);
        ctx->installer->setParallelMode(true);
        ctx->installer->installFabric(mcVersion, loaderVersion, installName);

        if (!fabricApiUrl.isEmpty()) {
            auto* ds2 = ensureSession(installName);
            ds2->fabricApiPending = true;
            ds2->fabricApiFinalPath = fabricApiSavePath;
            QString tempDir = QDir::tempPath() + QStringLiteral("/shadow-fabric-api");
            QDir().mkpath(tempDir);
            QString tempApiPath = tempDir + QStringLiteral("/") + QFileInfo(fabricApiSavePath).fileName();
            ds2->fabricApiSavePath = tempApiPath;

            showStep(installName, 7);
            updateStep(installName, 7, QStringLiteral("active"), 0);

            auto* apiNam = new QNetworkAccessManager(this);
            QUrl apiUrlObj(fabricApiUrl);
            QNetworkRequest apiReq(apiUrlObj);
            QNetworkReply* apiReply = apiNam->get(apiReq);

            connect(apiReply, &QNetworkReply::downloadProgress, this,
                    [this, installName](qint64 recv, qint64 total) {
                int pct = total > 0 ? (int)(recv * 100 / total) : 0;
                updateStep(installName, 7, QStringLiteral("active"), pct);
                auto* ds = dlSession(installName);
                if (ds) {
                    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                    qint64 delta = recv - ds->fabSpeedLastBytes;
                    qint64 timeDelta = nowMs - ds->fabSpeedLastMs;
                    if (delta > 0 && timeDelta >= 200) {
                        ds->fabSpeed = delta * 1000 / timeDelta;
                        ds->fabSpeedLastBytes = recv;
                        ds->fabSpeedLastMs = nowMs;
                    }
                }
            });

            connect(apiReply, &QNetworkReply::finished, this, [this, apiReply, apiNam, installName, tempApiPath]() {
                apiReply->deleteLater();
                apiNam->deleteLater();
                ensureSession(installName);
                auto* ds = dlSession(installName);
                ds->fabricApiPending = false;
                if (apiReply->error() != QNetworkReply::NoError) {
                    qWarning() << "[install] Fabric API download failed:" << apiReply->errorString();
                    updateStep(installName, 7, QStringLiteral("error"), 0);
                    setInstallPhase(tr("Fabric API 下载失败"));
                } else {
                    QByteArray data = apiReply->readAll();
                    QFile f(tempApiPath);
                    if (f.open(QIODevice::WriteOnly)) { f.write(data); f.close(); }
                    qDebug() << "[install] Fabric API downloaded to temp:" << tempApiPath << data.size() << "bytes";
                    updateStep(installName, 7, QStringLiteral("completed"), 100);
                    // Check if we can finalize
                    auto* mcCtx = mergedContext(installName);
                    if (mcCtx && mcCtx->mcDownloadDone && !mcCtx->bootstrapperDone) {
                        finishInstall(installName);
                    }
                }
            });
        }
        return;
    }

    // ── Forge/NeoForge: download installer JAR ──
    QString verArg = mcVersion + "-" + loaderVersion;
    QString fmv = m_forgeMavenVer;
    QString loaderDlUrl;
    if (loaderType == QStringLiteral("forge")) {
        loaderDlUrl = QStringLiteral("https://bmclapi2.bangbang93.com/maven/net/minecraftforge/forge/%1/forge-%1-installer.jar").arg(fmv);
    } else if (loaderType == QStringLiteral("neoforge")) {
        loaderDlUrl = QStringLiteral("https://bmclapi2.bangbang93.com/maven/net/neoforged/neoforge/%1/neoforge-%1-installer.jar").arg(loaderVersion);
    }

    if (!loaderDlUrl.isEmpty()) {
        auto* nam = new QNetworkAccessManager(this);
        int loaderDlStepIdx = 4;

        qDebug() << "[Coordinator] Loader download:" << loaderDlUrl;
        {
            QUrl qurl(loaderDlUrl);
            QNetworkRequest req(qurl);
            req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
            req.setTransferTimeout(300000);
            req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
            QNetworkReply* reply = nam->get(req);

            auto speedState = QSharedPointer<QPair<qint64,qint64>>::create(0,0);

            connect(reply, &QNetworkReply::downloadProgress, this,
                    [this, installName, loaderDlStepIdx, speedState](qint64 recv, qint64 total) {
                updateStep(installName, loaderDlStepIdx, QStringLiteral("active"),
                           total > 0 ? (int)(recv * 100 / total) : 0, recv, total);
                qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                qint64 delta = recv - speedState->first;
                qint64 timeDelta = nowMs - speedState->second;
                if (timeDelta >= 200 && speedState->second > 0 && delta > 0) {
                    qint64 instant = delta * 1000 / timeDelta;
                    ensureSession(installName);
                }
                speedState->first = recv;
                speedState->second = nowMs;
            });

            connect(reply, &QNetworkReply::finished, this,
                    [this, nam, reply, installName, loaderType, loaderVersion, mcVersion, forgeInstallerBranch, loaderDlStepIdx, ctx]() {
                reply->deleteLater();

                auto handleLoaderData = [this, nam, installName, loaderType, mcVersion, loaderVersion, forgeInstallerBranch, loaderDlStepIdx, ctx](const QByteArray& data) {
                    if (data.size() < 102400) {
                        qWarning() << "[Coordinator] Loader download too small:" << data.size();
                        nam->deleteLater();
                        updateStep(installName, loaderDlStepIdx, QStringLiteral("failed"), 0, data.size(), 0);
                        ctx->failed = true;
                        ctx->errorMessage = tr("下载文件异常");
                        if (ctx->mcDownloadDone) finishInstall(installName);
                        return;
                    }

                    nam->deleteLater();
                    if (!m_downloadSessions.contains(installName)) return;

                    ensureSession(installName);
                    auto* ds = dlSession(installName);
                    ds->loaderDownloadData = data;

                    updateStep(installName, loaderDlStepIdx, QStringLiteral("completed"), 100, data.size(), data.size());
                    int verifyStep = loaderDlStepIdx + 1;
                    if (ds->steps.size() > verifyStep) {
                        showStep(installName, verifyStep);
                        updateStep(installName, verifyStep, QStringLiteral("active"), 0);
                    }

                    ctx->installer->setGameDir(m_gameDir);
                    ctx->installer->setForgeBranch(forgeInstallerBranch);
                    ctx->loaderJarReady = true;

                    // Wait for MC download if not done yet
                    if (ctx->mcDownloadDone) {
                        proceedToLoaderInstall(installName);
                    }
                };

                if (reply->error() != QNetworkReply::NoError) {
                    // Build fallback URLs
                    QStringList fallbackUrls;
                    if (loaderType == QStringLiteral("forge")) {
                        QString baseVer = mcVersion + QStringLiteral("-") + loaderVersion;
                        auto addFb = [&](const QString& base, const QString& ver) {
                            fallbackUrls << QStringLiteral("%1/net/minecraftforge/forge/%2/forge-%2-installer.jar").arg(base, ver);
                        };
                        addFb(QStringLiteral("https://bmclapi2.bangbang93.com/maven"), baseVer + QStringLiteral("-") + mcVersion);
                        QString branchVer = baseVer;
                        if (!forgeInstallerBranch.isEmpty()) branchVer += QStringLiteral("-") + forgeInstallerBranch;
                        addFb(QStringLiteral("https://maven.minecraftforge.net"), branchVer);
                        addFb(QStringLiteral("https://maven.minecraftforge.net"), baseVer + QStringLiteral("-") + mcVersion);
                    } else if (loaderType == QStringLiteral("neoforge")) {
                        fallbackUrls << QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/neoforge/%1/neoforge-%1-installer.jar").arg(loaderVersion);
                    }

                    auto fbIdx = QSharedPointer<int>::create(0);
                    auto tryFb = QSharedPointer<std::function<void()>>::create();
                    *tryFb = [=]() {
                        if (*fbIdx >= fallbackUrls.size()) {
                            qWarning() << "[Coordinator] Loader download FAILED (all fallbacks)";
                            emit logMessage(QStringLiteral(" %1 下载失败: 所有源均不可用").arg(loaderType));
                            emit logMessage(tr("\\u26a0 %1 下载失败，将以原版安装").arg(loaderType));
                            nam->deleteLater();
                            updateStep(installName, loaderDlStepIdx, QStringLiteral("failed"), 0, 0, 0);
                            ctx->failed = true;
                            ctx->errorMessage = tr("所有源均不可用");
                            if (ctx->mcDownloadDone) finishInstall(installName);
                            return;
                        }

                        QString url = fallbackUrls[(*fbIdx)++];
                        qWarning() << "[Coordinator] Trying fallback:" << url;
                        QUrl qurlFb(url);
                        QNetworkRequest reqFb(qurlFb);
                        reqFb.setRawHeader("User-Agent", "ShadowLauncher/1.0");
                        reqFb.setTransferTimeout(300000);
                        QNetworkReply* r = nam->get(reqFb);

                        auto speedStateFb = QSharedPointer<QPair<qint64,qint64>>::create(0, 0);
                        connect(r, &QNetworkReply::downloadProgress, this,
                            [this, installName, loaderDlStepIdx, speedStateFb](qint64 recv, qint64 total) {
                                updateStep(installName, loaderDlStepIdx, QStringLiteral("active"),
                                           total > 0 ? (int)(recv * 100 / total) : 0, recv, total);
                                qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                                qint64 delta = recv - speedStateFb->first;
                                if (delta > 0) { speedStateFb->first = recv; speedStateFb->second = nowMs; }
                            });

                        connect(r, &QNetworkReply::finished, this,
                            [=]() {
                                r->deleteLater();
                                if (r->error() != QNetworkReply::NoError) { (*tryFb)(); return; }
                                QByteArray data = r->readAll();
                                if (data.size() < 102400) { (*tryFb)(); return; }
                                handleLoaderData(data);
                            });
                    };
                    (*tryFb)();
                    return;
                }

                QByteArray data = reply->readAll();
                handleLoaderData(data);
            });
        }
    }
}
'''

# Replace
new_content = content[:start_idx] + new_func + content[end_idx:]

with open('src/backend/version_backend.cpp', 'w', encoding='utf-8') as f:
    f.write(new_content)

print("installModLoader rewritten successfully")
print(f"Replaced {end_idx - start_idx} chars with {len(new_func)} chars")
