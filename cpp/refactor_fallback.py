"""Replace the merge-install fallback handler with a URL-list retry loop."""
path = r"D:\latest-code\cpp\src\backend\version_backend.cpp"

with open(path, 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Find the start of the fallback block and end
start_line = None
for i, line in enumerate(lines):
    if 'Phase 2: try official fallback' in line:
        start_line = i
        break

if start_line is None:
    print("ERROR: could not find Phase 2 marker")
    exit(1)

# Find end: the closing } of if (reply->error()) block, before "QByteArray data = reply->readAll()"
end_line = None
for i in range(start_line, len(lines)):
    if 'QByteArray data = reply->readAll();' in lines[i]:
        # Go backwards to find the closing } of if (reply->error())
        for j in range(i-1, start_line, -1):
            stripped = lines[j].strip()
            if stripped == '}' or stripped.startswith('}'):
                end_line = j
                break
        break

if end_line is None:
    print("ERROR: could not find end of error block")
    exit(1)

print(f"Replacing lines {start_line+1} through {end_line+1} ({end_line-start_line+1} lines)")

# Build the replacement code
replacement = """                            // Phase 2: build fallback URL list
                            // Priority: BMCLAPI old-format -> Official branch-aware -> Official old-format
                            QStringList fallbackUrls;
                            {
                                QString baseVer = mcVersion + QStringLiteral("-") + loaderVersion;
                                auto addFb = [&](const QString& base, const QString& ver) {
                                    fallbackUrls << QStringLiteral("%1/net/minecraftforge/forge/%2/forge-%2-installer.jar").arg(base, ver);
                                };
                                // 1. BMCLAPI old format ({mc}-{forge}-{mc}) — handles 1.7.10/1.8.9 edge cases
                                addFb(QStringLiteral("https://bmclapi2.bangbang93.com/maven"), baseVer + QStringLiteral("-") + mcVersion);
                                // 2. Official branch-aware format
                                QString branchVer = baseVer;
                                if (!forgeInstallerBranch.isEmpty())
                                    branchVer += QStringLiteral("-") + forgeInstallerBranch;
                                addFb(QStringLiteral("https://maven.minecraftforge.net"), branchVer);
                                // 3. Official old format
                                addFb(QStringLiteral("https://maven.minecraftforge.net"), baseVer + QStringLiteral("-") + mcVersion);
                            }

                            // Try each fallback in sequence
                            auto fbIdx = QSharedPointer<int>::create(0);
                            auto tryFb = QSharedPointer<std::function<void()>>::create();
                            *tryFb = [=]() {
                                if (*fbIdx >= fallbackUrls.size()) {
                                    // All fallbacks exhausted
                                    qWarning() << "[Coordinator] Loader download FAILED (all fallbacks exhausted)";
                                    emit logMessage(QStringLiteral(" %1 下载失败: 所有源均不可用").arg(loaderType));
                                    emit logMessage(tr("\\u26a0 %1 下载失败\\uff0c将以原版安装").arg(loaderType));

                                    nam->deleteLater();
                                    updateStep(installName, loaderDlStepIdx, QStringLiteral("failed"), 0, 0, 0);

                                    if (m_downloadSessions.contains(installName)) {
                                        ensureSession(installName);
                                        auto* ds = dlSession(installName);
                                        if (ds) {
                                            ds->loaderDownloadReady = true;
                                            ds->markFailed(QStringLiteral("所有源均不可用"));
                                            if (ds->mcDownloadDone) finishInstall(installName);
                                        }
                                    }
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
                                        if (delta > 0) {
                                            speedStateFb->first = recv;
                                            speedStateFb->second = nowMs;
                                        }
                                    });

                                connect(r, &QNetworkReply::finished, this,
                                    [=]() {
                                        r->deleteLater();
                                        if (r->error() != QNetworkReply::NoError) {
                                            qWarning() << "[Coordinator] Fallback failed:" << r->errorString();
                                            (*tryFb)();  // try next
                                            return;
                                        }

                                        QByteArray data = r->readAll();
                                        qDebug() << "[Coordinator] Fallback download complete:" << data.size() << "bytes";

                                        // Reject too-small responses
                                        if (data.size() < 102400) {
                                            qWarning() << "[Coordinator] Fallback response too small:" << data.size();
                                            (*tryFb)();  // try next
                                            return;
                                        }

                                        nam->deleteLater();

                                        if (!m_downloadSessions.contains(installName)) return;
                                        ensureSession(installName);
                                        auto* ds = dlSession(installName);
                                        ds->loaderDownloadData = data;
                                        updateStep(installName, loaderDlStepIdx, QStringLiteral("completed"), 100, data.size(), data.size());
                                        ds->loaderVerifyStep = (loaderDlStepIdx == 4) ? 5 : loaderDlStepIdx + 1;
                                        ds->loaderDownloadReady = true;

                                        if (ds->steps.size() > ds->loaderVerifyStep) {
                                            showStep(installName, ds->loaderVerifyStep);
                                            updateStep(installName, ds->loaderVerifyStep, QStringLiteral("active"), 0);
                                        }

                                        m_mlInstaller->setGameDir(m_gameDir);
                                        if (loaderType == QStringLiteral("neoforge")) {
                                            m_mlInstaller->installNeoForgeFromData(data, ds->mcVersion, ds->loaderVer, installName);
                                        } else {
                                            m_mlInstaller->installForgeFromData(data, ds->mcVersion, ds->loaderVer, installName);
                                        }
                                    });
                            };
                            (*tryFb)();
                            return;

"""

# Insert the replacement
new_lines = lines[:start_line] + replacement.splitlines(keepends=True) + lines[end_line+1:]

with open(path, 'w', encoding='utf-8') as f:
    f.writelines(new_lines)

print(f"Done. File now has {len(new_lines)} lines (was {len(lines)}).")
