// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "shadow_backend.h"
#include "../core/http_client.h"
#include "../core/resource_fetch_engine.h"
#include "../core/mod_manager.h"
#include "../core/update_manager.h"
#include "../core/icon_cache.h"
#include "../core/modpack_importer.h"
#include "../core/modpack/modpack_exporter.h"
#include "../core/local_mod_manager.h"
#include "../multiplayer/multiplayer_manager.h"
#include "../multiplayer/relay_crypto.h"
#include "../utils/secure_wipe.h"
#include "../multiplayer/encrypted_addr.h"  // kWorker offset constants
#include "../core/version_downloader.h"
#include "../core/geoip_service.h"
#include "../core/mc_language.h"
#include <QApplication>
#include <QFileDialog>
#include <QTimer>
#include <QTimer>
#include <QDirIterator>
#include <QFile>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QProcess>
#include "../core/version_isolation.h"
#include "../utils/logger.h"
#include <QElapsedTimer>
#include <QMutex>
#include <QThread>
#include <QtConcurrent>
#include <QRegularExpression>
#include <functional>
#include "account_backend.h"
#include "yggdrasil_backend.h"
#include "app_backend.h"
#include "check_backend.h"
#include "userdata_backend.h"
#include "launch_backend.h"
#include "resource_backend.h"
#include "settings_backend.h"
#include "version_backend.h"
#include "../utils/temp_tracker.h"
#include "stats_backend.h"
#include "java_backend.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGuiApplication>
#include <QDir>
#include <QDirIterator>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <memory>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QProcess>
#include <QRegularExpression>
#include <QTranslator>
#include <QUrl>

// libwebp: in-process webp→PNG decoding
#include <webp/decode.h>
#include <webp/encode.h>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QEventLoop>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dpapi.h>
#endif

namespace ShadowLauncher {

ShadowBackend::ShadowBackend(QObject* parent)
    : QObject(parent)
{
    QElapsedTimer bt; bt.start();
    auto bp = [&bt](const char* label) {
        qCInfo(logApp) << QStringLiteral("[后端启动 +%1ms] %2").arg(bt.elapsed()).arg(label);
    };

    // Create all 7 sub-backends
    m_app = new AppBackend(this);
    bp("AppBackend");
    m_account = new AccountBackend(this);
    bp("AccountBackend");
    m_yggdrasil = new YggdrasilBackend(this);
    bp("YggdrasilBackend");
    m_settings = new SettingsBackend(this);
    bp("SettingsBackend");
    m_check = new CheckBackend(this);
    bp("CheckBackend");
    m_version = new VersionBackend(this);
    bp("VersionBackend");
    m_launch = new LaunchBackend(this);
    bp("LaunchBackend");
    m_resource = new ResourceBackend(this);
    bp("ResourceBackend");
    m_stats = new StatsBackend(m_app->gameDir(), m_settings->isolation(), this);
    // Forward stats signals to backend's QML property
    connect(m_stats, &StatsBackend::statsChanged,     this, &ShadowBackend::statsChanged);
    connect(m_stats, &StatsBackend::loadingChanged,    this, &ShadowBackend::statsLoadingChanged);
    bp("StatsBackend");

    m_java = new JavaBackend(this);
    bp("JavaBackend");

    // ── Java 一键安装完成后：自动刷新两处 Java 状态 ──
    // 1) SettingsBackend 重新扫描系统 Java（设置-Java 列表更新）
    // 2) JavaRuntimeInstaller 重新前置检测（关于页一键安装卡片更新）
    connect(m_java, &JavaBackend::javaInstallFinished, this, [this](bool ok, const QString&) {
        if (!ok) return;  // 安装失败不需要刷新
        // 重新扫描设置-Java（findAllJava 现在含 java_cache，新装的会出现在列表）
        m_settings->scanJavaInstallations();
        // 刷新一键安装卡片的前置检测（scanSystemJavas 异步，完成后 emit systemJavaScanFinished）
        m_java->scanSystemJavas();
        qCInfo(logJava) << QStringLiteral("[Java安装] 安装完成，已触发 Java 列表与前置检测刷新");
    });

    m_userData = new UserDataBackend(this);
    bp("UserDataBackend");

    // 用户数据导出/导入日志接入主日志（诊断用）
    connect(m_userData, &UserDataBackend::logMessage,
            this, &ShadowBackend::logMessage);

    // ── Icon cache (3 separate caches: mod / shader / rp, each max 100) ──
    QString iconBase = m_app->dataDir() + "/icons";
    m_modIconCache = new IconCache(iconBase + "/mod", 100, this);
    m_shaderIconCache = new IconCache(iconBase + "/shader", 100, this);
    m_rpIconCache = new IconCache(iconBase + "/rp", 100, this);

    // ── 资源拉取引擎（司南）：统一列表 API + 图标拉取（并发/缓存/缩略图）──
    m_fetchEngine = new ResourceFetchEngine(m_app->dataDir() + "/cache/res", this);
    connect(m_fetchEngine, &ResourceFetchEngine::iconReady,
            this, [this](const QString& url, const QString& path) {
        emit iconReady(url, path);
    });
    if (m_resource)
        m_resource->setFetchEngine(m_fetchEngine);
    m_multiplayer = new MultiplayerManager(this);
    m_localMods = new LocalModManager(this);
    m_modpackImporter = new ModpackImporter(this);
    m_modpackExporter = new ModpackExporter(this);
    {
        auto* exporter = qobject_cast<ModpackExporter*>(m_modpackExporter);
        if (exporter)
            exporter->setGameDir(m_app->gameDir());   // 初始化（否则 m_gameDir 为空，导出全路径失效）
    }
    {
        auto* importer = qobject_cast<ModpackImporter*>(m_modpackImporter);
        if (importer) {
            importer->setVersionBackend(m_version);
            importer->setGameDir(m_app->gameDir());
            importer->setIsolation(m_settings->isolation());
        }
    }
    syncPlayerName();
    bp("IconCache");

    // ── Sync game directories: ALL backends use the same path ──
    m_version->setGameDir(m_app->gameDir());
    m_settings->setMinecraftDir(m_app->gameDir());
    m_settings->setIsolationGameDir(m_app->gameDir());
    m_localMods->setGameDir(m_app->gameDir());
    m_launch->setGameDir(m_app->gameDir());
    m_launch->setAccount(m_account);
    m_version->setIsolation(m_settings->isolation());

    // ── GeoIP service (auto-language detection) ──
    m_geoIp = new GeoIpService(this);
    // Pass initial settings
    m_version->setAutoLangMode(m_settings->autoLangMode());
    m_version->setDetectedRegion(m_geoIp->cachedRegion());
    // React to setting changes
    connect(m_settings, &SettingsBackend::autoLangModeChanged, this, [this]() {
        m_version->setAutoLangMode(m_settings->autoLangMode());
    });
    // When GeoIP detects region, update VersionBackend and retroactively apply
    connect(m_geoIp, &GeoIpService::regionDetected, this, [this](const QString& region) {
        m_version->setDetectedRegion(region);
        // If mode=2 (IP region), retroactively write options.txt for all installed versions
        if (m_settings->autoLangMode() == 2) {
            const auto ids = m_version->installedIds();
            for (const QString& id : ids) {
                QString versionGameDir = m_settings->getVersionGameDir(id);
                QString mcLang = mc_language::regionToMinecraftLang(region);
                mc_language::writeOptionsTxt(versionGameDir, mcLang);
            }
        }
    });
    // Background pre-detect (3s delay so it doesn't slow startup)
    QTimer::singleShot(3000, m_geoIp, &GeoIpService::detectRegion);

    bp("Signal wiring...");
    connect(m_account, &AccountBackend::accountChanged,
            this, &ShadowBackend::accountChanged);
    connect(m_account, &AccountBackend::accountChanged,
            this, &ShadowBackend::syncPlayerName);
    connect(m_account, &AccountBackend::skinReady,
            this, &ShadowBackend::skinReady);
    connect(m_account, &AccountBackend::offlineSkinReady,
            this, &ShadowBackend::offlineSkinReady);
    connect(m_account, &AccountBackend::offlineHistoryChanged,
            this, &ShadowBackend::offlineHistoryChanged);
    connect(m_account, &AccountBackend::logMessage,
            this, &ShadowBackend::logMessage);
    connect(m_account, &AccountBackend::microsoftLoginProgress,
            this, &ShadowBackend::microsoftLoginProgress);
    connect(m_account, &AccountBackend::microsoftLoginSuccess,
            this, &ShadowBackend::microsoftLoginSuccess);
    connect(m_account, &AccountBackend::microsoftLoginFailed,
            this, &ShadowBackend::microsoftLoginFailed);
    // Persist username on every login
    connect(m_account, &AccountBackend::accountChanged,
            this, [this]() {
                QString u = m_account->username();
                if (!u.isEmpty()) {
                    QSettings s(QCoreApplication::organizationName(),
                                QCoreApplication::applicationName());
                    s.setValue(QStringLiteral("account/lastUsername"), u);
                }
            });

    // ── Signal forwarding: SettingsBackend → ShadowBackend ──
    connect(m_settings, &SettingsBackend::javaPathChanged,
            this, &ShadowBackend::javaPathChanged);
    connect(m_settings, &SettingsBackend::javaScanFinished,
            this, &ShadowBackend::javaScanFinished);
    connect(m_settings, &SettingsBackend::memorySettingsChanged,
            this, &ShadowBackend::memorySettingsChanged);
    connect(m_settings, &SettingsBackend::generalSettingsChanged,
            this, &ShadowBackend::generalSettingsChanged);
    connect(m_settings, &SettingsBackend::isolationChanged,
            this, &ShadowBackend::isolationChanged);
    connect(m_settings, &SettingsBackend::embeddedLoginChanged,
            this, &ShadowBackend::embeddedLoginChanged);
    connect(m_settings, &SettingsBackend::customBgChanged,
            this, &ShadowBackend::customBgChanged);
    connect(m_settings, &SettingsBackend::logMessage,
            this, &ShadowBackend::logMessage);

    // ── Embedded login: sync SettingsBackend ↔ AccountBackend ──
    m_account->setEmbeddedLoginEnabled(m_settings->embeddedLoginEnabled());
    connect(m_settings, &SettingsBackend::embeddedLoginChanged, this, [this]() {
        m_account->setEmbeddedLoginEnabled(m_settings->embeddedLoginEnabled());
    });

    // ── Signal forwarding: CheckBackend → ShadowBackend ──
    connect(m_check, &CheckBackend::logMessage,
            this, &ShadowBackend::logMessage);

    // ── Signal forwarding: VersionBackend → ShadowBackend ──
    connect(m_version, &VersionBackend::versionListReady,
            this, &ShadowBackend::versionListReady);
    connect(m_version, &VersionBackend::installedVersionsChanged,
            this, [this]() {
                // 已安装列表就绪时恢复上次选中版本——只恢复「真实存在」的版本（installedIds）。
                // 旧逻辑挂在 versionListReady 且用 versionIds（manifest 在线清单），
                // manifest 有但未安装的版本会被误选中（如 26.2 vs 实际安装的 26.2-forge-65.1.0）。
                const QStringList installed = m_version->installedIds();
                const QString last = m_settings->lastSelectedVersion();
                if (!last.isEmpty() && installed.contains(last)) {
                    m_version->setSelectedVersion(last);
                    qCInfo(logLaunch) << QStringLiteral("恢复上次选中版本 版本=%1").arg(last);
                    return;
                }
                // 自愈：当前选中版本已不存在（被删除/文件夹被手动删掉）→ 自动切换到可用版本
                const QString cur = m_version->selectedVersion();
                if (!cur.isEmpty() && !installed.contains(cur)) {
                    m_version->ensureSelectedVersionValid();
                    qCInfo(logLaunch) << QStringLiteral("选中版本 %1 已不存在，已自动切换").arg(cur);
                }
            });
    connect(m_version, &VersionBackend::installedVersionsChanged,
            this, &ShadowBackend::installedVersionsChanged);
    connect(m_version, &VersionBackend::installedVersionsChanged,
            this, &ShadowBackend::activeVersionNamesChanged);
        connect(m_version, &VersionBackend::installStateChanged,
            this, &ShadowBackend::activeVersionNamesChanged);
    connect(m_version, &VersionBackend::selectedVersionChanged,
            this, &ShadowBackend::selectedVersionChanged);
    connect(m_version, &VersionBackend::selectedVersionChanged, this, [this]() {
        QString vid = m_version->selectedVersion();
        // Persist last selected version (saved on exit, restored on next launch)
        m_settings->setLastSelectedVersion(vid);
        if (vid.isEmpty()) {
            m_currentVersionSummary.clear();
            emit currentVersionSummaryChanged();
            return;
        }

        // 异步计算：游戏目录/版本目录/assets 全量递归遍历在 worker 线程，
        // 避免模组上百后「选择版本」卡死 UI（选中即回显，大小稍后刷新）
        const QString gameDir = getVersionGameDir(vid);
        const QString versionDir = m_app->gameDir() + QStringLiteral("/versions/") + vid;
        const QString assetsPath = m_app->gameDir() + QStringLiteral("/assets/objects");
        QtConcurrent::run([this, vid, gameDir, versionDir, assetsPath]() {
            qint64 totalSize = 0;

            // Count game directory (worlds, mods, saves, screenshots, etc.)
            if (QDir(gameDir).exists()) {
                QDirIterator it(gameDir, QDir::Files, QDirIterator::Subdirectories);
                while (it.hasNext()) { it.next(); totalSize += it.fileInfo().size(); }
            }
            // Add version files (jar, json, libraries) — but NOT game/ subdir (already counted)
            if (QDir(versionDir).exists()) {
                QDirIterator it(versionDir, QDir::Files, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    QString fp = it.next();
                    // Skip game/ subdirectory for isolated versions (already counted via gameDir)
                    if (fp.contains(QStringLiteral("/game/")) || fp.contains(QStringLiteral("\\game\\")))
                        continue;
                    totalSize += it.fileInfo().size();
                }
            }
            // Count shared assets (objects/) for a realistic total
            if (QDir(assetsPath).exists()) {
                QDirIterator ait(assetsPath, QDir::Files, QDirIterator::Subdirectories);
                while (ait.hasNext()) { ait.next(); totalSize += ait.fileInfo().size(); }
            }

            QVariantMap summary;
            if (totalSize < 1024 * 1024)
                summary[QStringLiteral("sizeDisplay")] = QString::number(totalSize / 1024.0, 'f', 1) + QStringLiteral(" KB");
            else if (totalSize < 1024LL * 1024 * 1024)
                summary[QStringLiteral("sizeDisplay")] = QString::number(totalSize / (1024.0 * 1024), 'f', 1) + QStringLiteral(" MB");
            else
                summary[QStringLiteral("sizeDisplay")] = QString::number(totalSize / (1024.0 * 1024 * 1024), 'f', 2) + QStringLiteral(" GB");

            summary[QStringLiteral("modCount")] = 0;
            QMetaObject::invokeMethod(this, [this, vid, summary]() {
                // 防过期：选择已切换则丢弃旧结果
                if (m_version->selectedVersion() != vid) return;
                m_currentVersionSummary = summary;
                emit currentVersionSummaryChanged();
            }, Qt::QueuedConnection);
        });
    });
    connect(m_version, &VersionBackend::installStateChanged,
            this, &ShadowBackend::installStateChanged);
        // installPhaseChanged signal with different signature:
    // VersionBackend emits installPhaseChanged(const QString&), ShadowBackend emits installPhaseChanged()
    connect(m_version, &VersionBackend::installPhaseChanged,
            this, [this](const QString&) {
                emit installPhaseChanged();
                emit verifyRunningChanged();
            });
    connect(m_version, &VersionBackend::repairRunningChanged,
            this, &ShadowBackend::repairRunningChanged);
    connect(m_version, &VersionBackend::installFinished,
            this, &ShadowBackend::installFinished);
    connect(m_version, &VersionBackend::installComplete,
            this, &ShadowBackend::installComplete);
    connect(m_version, &VersionBackend::logMessage,
            this, &ShadowBackend::logMessage);
    connect(m_version, &VersionBackend::toastMessage,
            this, &ShadowBackend::toastMessage);
    connect(m_version, &VersionBackend::verifyStarted,
            this, &ShadowBackend::verifyStarted);
    connect(m_version, &VersionBackend::verifyProgress,
            this, [this](int checked, int total) {
                emit verifyProgress(checked, total);
                emit verifyCheckedChanged();
                emit verifyTotalChanged();
            });
    connect(m_version, &VersionBackend::verifyFinished,
            this, [this](bool allPassed) {
        setVerifyResult(allPassed ? tr("所有文件校验通过") : tr("存在损坏/缺失文件，请修复后重试"), allPassed);
        emit verifyFinished(allPassed);
    });
                connect(m_version, &VersionBackend::verifyFailedFiles,
            this, [this](const QStringList& failedFiles) {
                // Generate error report
                if (!failedFiles.isEmpty()) {
                    QDir reportsDir(m_gameDir + QStringLiteral("/verify_reports"));
                    if (!reportsDir.exists()) reportsDir.mkpath(QStringLiteral("."));
                    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
                    m_verifyReportPath = reportsDir.filePath(
                        QStringLiteral("verify_failed_%1.txt").arg(timestamp));
                    QFile report(m_verifyReportPath);
                    if (report.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        QTextStream out(&report);
                        out << tr("Shadow Launcher — 版本完整性校验报告\n");
                        out << QStringLiteral("======================================\n");
                        out << tr("时间: ") << QDateTime::currentDateTime().toString(Qt::ISODate) << QStringLiteral("\n");
                        out << tr("版本: ") << m_version->selectedVersion() << QStringLiteral("\n");
                        out << tr("异常文件数: ") << failedFiles.size() << QStringLiteral("\n");
                        out << QStringLiteral("--------------------------------------\n");
                        for (int i = 0; i < failedFiles.size(); ++i) {
                            out << (i + 1) << QStringLiteral(". ") << failedFiles[i] << QStringLiteral("\n");
                        }
                        out << QStringLiteral("--------------------------------------\n");
                        out << tr("请使用「一键修复」重新下载损坏/缺失的文件。\n");
                    }
                } else {
                    m_verifyReportPath.clear();
                }
                emit verifyFailedFiles(failedFiles);
            });
    connect(m_version, &VersionBackend::downloadQueueChanged,
            this, &ShadowBackend::downloadQueueChanged);
    connect(m_version, &VersionBackend::downloadQueueFull,
            this, &ShadowBackend::downloadQueueFull);

    // ── Signal forwarding: LaunchBackend → ShadowBackend ──
    connect(m_launch, &LaunchBackend::launchProgressChanged,
            this, &ShadowBackend::launchProgressChanged);
    connect(m_launch, &LaunchBackend::launchStateChanged,
            this, &ShadowBackend::launchStateChanged);
    connect(m_launch, &LaunchBackend::minecraftStarted,
            this, &ShadowBackend::minecraftStarted);
    connect(m_launch, &LaunchBackend::minecraftStopped,
            this, &ShadowBackend::minecraftStopped);
    connect(m_launch, &LaunchBackend::crashDetected,
            this, [this](const QVariantMap& report) {
                m_lastCrash = report;
                emit crashDetected(report);
            });
    connect(m_launch, &LaunchBackend::crashAnalysisStarted,
            this, &ShadowBackend::crashAnalysisStarted);
    connect(m_launch, &LaunchBackend::crashAnalysisReady,
            this, &ShadowBackend::crashAnalysisReady);
    connect(m_launch, &LaunchBackend::isRunningChanged,
            this, &ShadowBackend::isRunningChanged);
    connect(m_launch, &LaunchBackend::runningCountChanged,
            this, &ShadowBackend::runningCountChanged);
    connect(m_launch, &LaunchBackend::logMessage,
            this, &ShadowBackend::logMessage);
    connect(m_launch, &LaunchBackend::launchCheckProgress,
            this, &ShadowBackend::launchCheckProgress);
    connect(m_launch, &LaunchBackend::launchCheckFailed,
            this, &ShadowBackend::launchCheckFailed);
    connect(m_launch, &LaunchBackend::launchCheckMissingFiles,
            this, &ShadowBackend::launchCheckMissingFiles);
    connect(m_launch, &LaunchBackend::launchCheckWarning,
            this, &ShadowBackend::launchCheckWarning);

    // ── Load persisted login mode ──
    {
        QSettings s(QCoreApplication::organizationName(),
                    QCoreApplication::applicationName());
        m_lastLoginMode = s.value(QStringLiteral("account/lastLoginMode"), 1).toInt();
        QString lastUser = s.value(QStringLiteral("account/lastUsername")).toString();
        if (!lastUser.isEmpty()) {
            // Auto-restore login on startup — differentiate mode
            QTimer::singleShot(100, this, [this, lastUser]() {
                if (m_lastLoginMode == 0) {
                    // Microsoft (online) — session already restored in AccountBackend ctor via loadMicrosoftSession()
                    qCInfo(logApp) << QStringLiteral("[启动] 恢复正版登录: ") << lastUser;
                } else if (m_lastLoginMode == 1) {
                    // Offline mode
                    qCInfo(logApp) << QStringLiteral("[启动] 恢复离线登录: ") << lastUser;
                    m_account->offlineLogin(lastUser);
                }
                // m_lastLoginMode == 2 (外置登录): YggdrasilBackend 的 loadSession() 已自动恢复
            });
        }

        // 通知 QML 恢复登录模式（包括外置登录）
        QTimer::singleShot(200, this, [this]() {
            emit loginModeChanged();
        });
    }

    // ── Signal forwarding: ResourceBackend → ShadowBackend ──
    connect(m_resource, &ResourceBackend::downloadStateChanged,
            this, [this]() {
                bool downloading = m_resource->isDownloading();
                if (!downloading) {
                    m_resourceDlProgress = 0;
                    m_resourceDlTotal = 0;
                    m_resourceDlSpeed = 0;
                    m_resourceDlFile.clear();
                    emit resourceDownloadProgress(0, 0, QString());
                    if (m_version) m_version->removeResourceCard(QStringLiteral("resource"));
                } else {
                    m_resourceDlProgress = 0;
                    m_resourceDlTotal = 0;
                    m_resourceDlFile.clear();
                    if (m_version) m_version->addResourceCard(QStringLiteral("resource"), tr("资源包下载"));
                }
                qCInfo(logLaunch) << QStringLiteral("[资源包下载] 状态变更 下载中=%1").arg(m_resource->isDownloading());
                emit resourceDownloadStateChanged();
            });
    connect(m_resource, &ResourceBackend::downloadProgressChanged,
            this, [this](int completed, int total, const QString& fileName) {
                m_resourceDlProgress = completed;
                m_resourceDlTotal = total;
                m_resourceDlSpeed = m_resource->dlSpeed();
                m_resourceDlFile = fileName;
                qCInfo(logLaunch) << QStringLiteral("[资源包下载] 进度 %1/%2 %3").arg(completed).arg(total).arg(fileName);
                emit resourceDownloadProgress(completed, total, fileName);
                if (m_version && total > 0) {
                    m_version->updateResourceCard(QStringLiteral("resource"),
                        (qreal)completed / total, fileName);
                }
            });
    connect(m_resource, &ResourceBackend::downloadFinished,
            this, [this](const QString&, bool success, const QString&) {
                qCInfo(logLaunch) << QStringLiteral("[资源包下载] 下载完成 成功=%1").arg(success);
                emit resourceDownloadDone(success);
                if (m_version) {
                    if (success) m_version->completeResourceCard(QStringLiteral("resource"));
                    else m_version->failResourceCard(QStringLiteral("resource"), tr("资源包下载失败"));
                }
            });
    connect(m_resource, &ResourceBackend::searchResultsReady,
            this, &ShadowBackend::searchResultsReady);
    connect(m_resource, &ResourceBackend::modSearchResultsReady,
            this, &ShadowBackend::modSearchResultsReady);
    connect(m_resource, &ResourceBackend::shaderSearchResultsReady,
            this, &ShadowBackend::shaderSearchResultsReady);
    connect(m_resource, &ResourceBackend::resourcepackSearchCompleted,
            this, [this](const QVariantList& results, int totalHits) {
                qDebug().noquote() << "[BACKEND] RP searchCompleted SIGNAL emitted, hits=" << results.size() << "total=" << totalHits;
                emit resourcepackSearchCompleted(results, totalHits);
            });
    connect(m_resource, &ResourceBackend::resourcepackSearchFailed,
            this, &ShadowBackend::resourcepackSearchFailed);
    connect(m_resource, &ResourceBackend::resourcepackDownloadFinished,
            this, &ShadowBackend::resourcepackDownloadFinished);
    connect(m_resource, &ResourceBackend::resourcepackVersionsLoaded,
            this, &ShadowBackend::resourcepackVersionsLoaded);
    connect(m_resource, &ResourceBackend::resourcepackVersionsPartial,
            this, &ShadowBackend::resourcepackVersionsPartial);
    connect(m_resource, &ResourceBackend::resourcepackVersionsProgress,
            this, &ShadowBackend::resourcepackVersionsProgress);
    connect(m_resource, &ResourceBackend::modVersionsLoaded,
            this, &ShadowBackend::modVersionsLoaded);
    connect(m_resource, &ResourceBackend::modVersionsPartial,
            this, &ShadowBackend::modVersionsPartial);
    connect(m_resource, &ResourceBackend::modVersionsProgress,
            this, &ShadowBackend::modVersionsProgress);
    connect(m_resource, &ResourceBackend::shaderVersionsLoaded,
            this, &ShadowBackend::shaderVersionsLoaded);
    connect(m_resource, &ResourceBackend::shaderVersionsPartial,
            this, &ShadowBackend::shaderVersionsPartial);
    connect(m_resource, &ResourceBackend::shaderVersionsProgress,
            this, &ShadowBackend::shaderVersionsProgress);
    connect(m_resource, &ResourceBackend::logMessage,
            this, &ShadowBackend::logMessage);

    // ── Mod file download forwarding ──
    connect(m_resource, &ResourceBackend::modFileDownloadStarted,
            this, [this](int dlId, const QString& fileName, qint64 fileSize, const QString& displayName) {
                Q_UNUSED(fileSize);
                QString cardId = QStringLiteral("mod:%1").arg(dlId);
                m_modDownloadCards.insert(dlId, cardId);
                if (m_version) m_version->addResourceCard(cardId, displayName.isEmpty() ? fileName : displayName);
            });
    connect(m_resource, &ResourceBackend::modFileDownloadProgress,
            this, [this](int dlId, qint64 received, qint64 total, qint64 speed) {
                if (m_modDownloadCards.contains(dlId) && m_version && total > 0) {
                    m_version->updateResourceCard(m_modDownloadCards[dlId],
                        (qreal)received / total, QString(), speed);
                }
            });
    connect(m_resource, &ResourceBackend::modFileDownloadFinished,
            this, [this](int dlId, bool success, const QString& filePath, const QString& displayName) {
                if (m_modDownloadCards.contains(dlId)) {
                    const QString cardId = m_modDownloadCards[dlId];
                    if (m_version) {
                        if (success) {
                            // 下载完成：卡片定格绿色完成态（不立即移除，QML 端保留展示）
                            m_version->completeResourceCard(cardId);
                        } else {
                            // 下载器回调 ok=false（如 SHA1 校验失败分支已走 Failed 信号，此处兜底）
                            m_version->failResourceCard(cardId, tr("下载失败"));
                        }
                    }
                    m_modDownloadCards.remove(dlId);
                }
                // ── 整合包下载完成 → 自动转入导入流程（用户输入版本名注册）──
                if (m_packDownloads.contains(dlId)) {
                    const PackDownloadInfo info = m_packDownloads.take(dlId);
                    m_packDownloading = false;   // 下载阶段结束（无论成败）
                    if (success && m_modpackImporter) {
                        auto* importer = qobject_cast<ModpackImporter*>(m_modpackImporter);
                        qCInfo(logApp) << QStringLiteral("[整合包] 下载完成，自动导入: %1 版本名=%2")
                            .arg(info.zipPath, info.versionName);
                        if (importer) {
                            // 图标随 startImport 传入（任务侧重置/落盘），不再走 setPackIcon 前置设置
                            importer->startImport(info.zipPath, info.versionName, false, info.iconUrl);
                        }
                    } else if (!success) {
                        qCInfo(logApp) << QStringLiteral("[整合包] 下载失败，不导入: %1").arg(info.zipPath);
                    }
                }
                // 透传信号给 QML（成功 Toast / 详情页错误弹窗）
                emit modFileDownloadFinished(dlId, success, filePath, displayName);
            });
    connect(m_resource, &ResourceBackend::modFileDownloadFailed,
            this, [this](int dlId, const QString& errorDetail, const QString& displayName) {
                if (m_modDownloadCards.contains(dlId)) {
                    const QString cardId = m_modDownloadCards[dlId];
                    if (m_version) m_version->failResourceCard(cardId, errorDetail);
                    m_modDownloadCards.remove(dlId);
                }
                // 整合包下载失败：移除待导入记录（不进入导入流程）
                if (m_packDownloads.contains(dlId)) {
                    m_packDownloads.remove(dlId);
                    m_packDownloading = false;
                    qCInfo(logApp) << QStringLiteral("[整合包] 下载失败，取消自动导入 dlId=%1").arg(dlId);
                }
                // 透传信号给 QML（失败 Toast / 详情页错误弹窗）
                emit modFileDownloadFailed(dlId, errorDetail, displayName);
            });
    // 整合包搜索完成透传（QML 回填列表）
    connect(m_resource, &ResourceBackend::modpackSearchResultsReady,
            this, &ShadowBackend::modpackSearchResultsReady);
    // 数据包搜索完成透传（QML 回填列表）
    connect(m_resource, &ResourceBackend::datapackSearchResultsReady,
            this, &ShadowBackend::datapackSearchResultsReady);
    // CF 前置依赖解析结果透传（QML 回填依赖卡片）
    connect(m_resource, &ResourceBackend::cfDependenciesResolved,
            this, &ShadowBackend::cfDependenciesResolved);

    // ── Signal forwarding: AppBackend → ShadowBackend ──
    connect(m_app, &AppBackend::gameDirChanged,
            this, &ShadowBackend::gameDirChanged);
    connect(m_app, &AppBackend::themeChanged,
            this, &ShadowBackend::themeChanged);
    connect(m_app, &AppBackend::logMessage,
            this, &ShadowBackend::logMessage);

    // ── Update manager ──
    m_updateManager = new UpdateManager(this);
    m_updateManager->setRepo(QStringLiteral("xiaole1173"), QStringLiteral("shadow-launcher"));
    m_updateManager->setCurrentVersion(appVersion());
    m_updateManager->setQtVersion(QStringLiteral(SHADOW_QT_VERSION));
    m_updateManager->setResourceEpoch(SHADOW_RESOURCE_EPOCH);
    connect(m_updateManager, &UpdateManager::stateChanged,
            this, &ShadowBackend::updateStateChanged);
    connect(m_updateManager, &UpdateManager::stateChanged,
            this, &ShadowBackend::updateCheckingChanged);
    connect(m_updateManager, &UpdateManager::toastMessage,
            this, &ShadowBackend::toastMessage);
    connect(m_updateManager, &UpdateManager::downloadProgress,
            this, [this](qint64 r, qint64 t) { emit updateDownloadProgress(r, t); });
    // Resume paused download from previous session
    m_updateManager->resumePausedDownload();
    // Check for post-update changelog (delay for QML init)
    QTimer::singleShot(500, this, [this] { checkChangelog(); });
    // Load persisted Java runtime settings (restored from previous session)
    loadJavaRuntimeSettings();
    // Auto-check on startup (silent)
    QTimer::singleShot(0, m_updateManager, &UpdateManager::checkSilent);
    bp("UpdateManager");

    bp("Constructor done");

    // ── Clean up orphaned temp dirs from previous (crashed) sessions ──
    qCInfo(logApp) << QStringLiteral("[追踪] 检查残留临时目录...");
    TempTracker::cleanupOrphans();

    // Sync m_currentLang from saved settings (prevents switchLanguage early-return bug)
    const QStringList codes = { QStringLiteral("zh_CN"), QStringLiteral("zh_HK"), QStringLiteral("zh_TW") };
    int savedIdx = m_settings->languageIndex();
    if (savedIdx >= 0 && savedIdx < codes.size()) {
        m_currentLang = codes[savedIdx];
    }
    // Ensure language.txt is initialized (used by QML to recover ComboBox state)
    writeLanguageFile(savedIdx);
}

// ============================================================
// Account property getters
// ============================================================

QString ShadowBackend::username() const {
    return m_account->username();
}

QString ShadowBackend::offlineUsername() const {
    return m_account->offlineUsername();
}

bool ShadowBackend::isOnline() const {
    return m_account->isOnline();
}

QString ShadowBackend::accountUuid() const {
    return m_account->accountUuid();
}

QString ShadowBackend::offlineUuid() const {
    return m_account->offlineUuid();
}

QString ShadowBackend::skinPath() const {
    return m_account->skinPath();
}

QString ShadowBackend::offlineSkinPath() const {
    return m_account->offlineSkinPath();
}

QStringList ShadowBackend::offlineUsernames() const {
    return m_account->offlineUsernames();
}

// ============================================================
// Settings property getters
// ============================================================

QString ShadowBackend::javaPath() const {
    return m_settings->javaPath();
}

QString ShadowBackend::javaVersion() const {
    return m_settings->javaVersion();
}

int ShadowBackend::javaMajor() const {
    return m_settings->javaMajor();
}

bool ShadowBackend::javaInstalled() const {
    return m_settings->isJavaReady();
}

int ShadowBackend::minMemoryMb() const {
    return m_settings->minMemoryMB();
}

int ShadowBackend::maxMemoryMb() const {
    return m_settings->maxMemoryMB();
}


bool ShadowBackend::isolationEnabled() const {
    return m_isolationEnabled;
}

int ShadowBackend::fileDownloadSource() const { return m_settings->fileDownloadSource(); }
void ShadowBackend::setFileDownloadSource(int v) { m_settings->setFileDownloadSource(v); emit downloadSettingsChanged(); }
int ShadowBackend::listDownloadSource() const { return m_settings->listDownloadSource(); }
void ShadowBackend::setListDownloadSource(int v) { m_settings->setListDownloadSource(v); emit downloadSettingsChanged(); }
int ShadowBackend::maxDownloadThreads() const { return m_settings->maxDownloadThreads(); }
void ShadowBackend::setMaxDownloadThreads(int v) { m_settings->setMaxDownloadThreads(v); emit downloadSettingsChanged(); }
double ShadowBackend::downloadSpeedLimitMB() const { return m_settings->downloadSpeedLimitMB(); }
void ShadowBackend::setDownloadSpeedLimitMB(double v) { m_settings->setDownloadSpeedLimitMB(v); emit downloadSettingsChanged(); }

bool ShadowBackend::embeddedLoginEnabled() const {
    return m_settings->embeddedLoginEnabled();
}

void ShadowBackend::setEmbeddedLoginEnabled(bool v) {
    m_settings->setEmbeddedLoginEnabled(v);
}

// ── Custom background ──
QString ShadowBackend::customBgPath() const { return m_settings->customBgPath(); }
void ShadowBackend::setCustomBgPath(const QString& url) {
    // On clear, delete the cached copy
    if (url.isEmpty() && !m_settings->customBgPath().isEmpty()) {
        QString oldPath = QUrl(m_settings->customBgPath()).toLocalFile();
        if (!oldPath.isEmpty() && QFile::exists(oldPath)) {
            QFile::remove(oldPath);
        }
    }
    m_settings->setCustomBgPath(url);
}
qreal ShadowBackend::sidebarOpacity() const { return m_settings->sidebarOpacity(); }
void ShadowBackend::setSidebarOpacity(qreal v) { m_settings->setSidebarOpacity(v); }
qreal ShadowBackend::contentOpacity() const { return m_settings->contentOpacity(); }
void ShadowBackend::setContentOpacity(qreal v) { m_settings->setContentOpacity(v); }

qreal ShadowBackend::cropX() const { return m_settings->cropX(); }
void ShadowBackend::setCropX(qreal v) { m_settings->setCropX(v); }
qreal ShadowBackend::cropY() const { return m_settings->cropY(); }
void ShadowBackend::setCropY(qreal v) { m_settings->setCropY(v); }
void ShadowBackend::updateCrop(qreal x, qreal y) {
    m_settings->setCropX(x);
    m_settings->setCropY(y);
}

QString ShadowBackend::pickBackgroundImage() {
    QString path = QFileDialog::getOpenFileName(nullptr,
        QString::fromUtf8("选择背景图片"), QString(),
        QString::fromUtf8("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*)"));
    if (path.isEmpty()) return QString();

    // Copy to app data directory so the file doesn't go missing
    QDir bgDir(m_app->dataDir() + "/custom_bg");
    if (!bgDir.exists()) bgDir.mkpath(".");
    // Remove old backgrounds
    for (const QString& old : bgDir.entryList(QDir::Files)) {
        QFile::remove(bgDir.filePath(old));
    }
    QFileInfo fi(path);
    QString ext = fi.suffix().isEmpty() ? QStringLiteral("png") : fi.suffix();
    QString destPath = bgDir.filePath(QStringLiteral("bg.") + ext);
    if (!QFile::copy(path, destPath)) {
        // Fallback: use original path
        qWarning() << "[pickBackgroundImage] failed to copy" << path << "to" << destPath;
        QString url = QUrl::fromLocalFile(path).toString();
        m_settings->setCustomBgPath(url);
        return url;
    }
    QString url = QUrl::fromLocalFile(destPath).toString();
    m_settings->setCustomBgPath(url);
    return url;
}

QVariantList ShadowBackend::availableJavaList() const {
    return m_settings->availableJavaList();
}

int ShadowBackend::lastLoginMode() const {
    return m_lastLoginMode;
}

void ShadowBackend::setLastLoginMode(int mode) {
    if (m_lastLoginMode != mode) {
        m_lastLoginMode = mode;
        // Persist
        QSettings s(QCoreApplication::organizationName(),
                    QCoreApplication::applicationName());
        s.setValue(QStringLiteral("account/lastLoginMode"), mode);
        emit loginModeChanged();
    }
}

// ============================================================
// Version property getters
// ============================================================

QString ShadowBackend::selectedVersion() const {
    return m_version->selectedVersion();
}

QStringList ShadowBackend::versionIds() const {
    return m_version->versionIds();
}

QVariantList ShadowBackend::versionList() const {
    return m_version->versionInfoList();
}

QStringList ShadowBackend::installedVersions() const {
    return m_version->installedIds();
}

QStringList ShadowBackend::activeVersionNames() const {
    return m_version->activeVersionNames();
}

bool ShadowBackend::isInstalling() const {
    return m_version->isInstalling();
}




bool ShadowBackend::verifyRunning() const {
    return m_version && m_version->isVerifyRunning();
}
bool ShadowBackend::repairRunning() const {
    return m_version && m_version->isRepairRunning();
}





QObject* ShadowBackend::installCardsModel() const {
    return m_version ? m_version->installCardsModel() : nullptr;
}

int ShadowBackend::verifyChecked() const {
    return m_version ? m_version->verifyChecked() : 0;
}

int ShadowBackend::verifyTotal() const {
    return m_version ? m_version->verifyTotal() : 0;
}

QString ShadowBackend::installVersionId() const {
    return m_version->installVersionId();
}

QString ShadowBackend::installPhase() const {
    return m_version->installPhase();
}




QVariantList ShadowBackend::downloadQueue() const {
    return m_version ? m_version->downloadQueue() : QVariantList{};
}

QVariantList ShadowBackend::activeDownloads() const {
    return m_version ? m_version->activeDownloads() : QVariantList{};
}

QStringList ShadowBackend::releaseVersions() const {
    QStringList list;
    auto versions = m_version->cachedMcVersions();
    for (const auto& v : versions) {
        if (v.type == QStringLiteral("release")) list.append(v.id);
    }
    return list;
}

static bool isAprilFoolVersion(const McVersion& v)
{
    // "special" type from Mojang manifest
    if (v.type == QStringLiteral("special"))
        return true;

    // Specific April Fools version IDs
    static const QSet<QString> foolIds = {
        QStringLiteral("20w14infinite"),
        QStringLiteral("20w14\u221E"),          // 20w14∞
        QStringLiteral("3d shareware v1.34"),
        QStringLiteral("1.rv-pre1"),
        QStringLiteral("15w14a"),
        QStringLiteral("2.0"),
        QStringLiteral("22w13oneblockatatime"),
        QStringLiteral("23w13a_or_b"),
        QStringLiteral("24w14potato"),
        QStringLiteral("25w14craftmine"),
        QStringLiteral("26w14a")
    };
    if (foolIds.contains(v.id))
        return true;

    // Any snapshot released on April 1st (auto-detection)
    if (v.releaseTime.isValid()) {
        QDate d = v.releaseTime.date();
        if (d.month() == 4 && d.day() == 1)
            return true;
    }

    return false;
}

QStringList ShadowBackend::snapshotVersions() const {
    QStringList list;
    auto versions = m_version->cachedMcVersions();
    for (const auto& v : versions) {
        if (v.type == QStringLiteral("snapshot") && !isAprilFoolVersion(v))
            list.append(v.id);
    }
    return list;
}

QStringList ShadowBackend::oldVersions() const {
    QStringList list;
    auto versions = m_version->cachedMcVersions();
    for (const auto& v : versions) {
        if (v.type != QStringLiteral("release") && v.type != QStringLiteral("snapshot")
            && !isAprilFoolVersion(v))
            list.append(v.id);
    }
    return list;
}

QStringList ShadowBackend::aprilFoolVersions() const {
    QStringList list;
    auto versions = m_version->cachedMcVersions();
    for (const auto& v : versions) {
        if (isAprilFoolVersion(v)) list.append(v.id);
    }
    return list;
}

void ShadowBackend::refreshVersionDetails()
{
    // 两阶段异步：① 设置扫描状态让 QML 显示加载指示器
    //              ② 下一事件循环再执行实际扫描（给 QML 渲染时间）
    QTimer::singleShot(0, this, [this]() {
        if (m_isScanningVersions) return;
        m_isScanningVersions = true;
        emit scanningChanged();

        // 扫描体放到 worker 线程（大版本目录遍历/尺寸统计可能耗时数百 ms，避免卡 UI）
        const QString gameDirPath = m_app->gameDir();
        QtConcurrent::run([this, gameDirPath]() {
            QVariantList local;
            QDir gameDir(gameDirPath);
        QString versionsPath = gameDir.absoluteFilePath(QStringLiteral("versions"));
        QDir versionsDir(versionsPath);
        if (versionsDir.exists()) {

    // Map of known MC versions to their types (from cached manifest)
    QMap<QString, QString> knownTypes;
    auto cached = m_version->cachedMcVersions();
    for (const auto& v : cached) {
        knownTypes[v.id] = v.type;
    }

    const QStringList entries = versionsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    // Build a lookup for release times
    QMap<QString, QDateTime> releaseTimes;
    for (const auto& v : cached) {
        if (v.releaseTime.isValid())
            releaseTimes[v.id] = v.releaseTime;
    }
    for (const QString& versionId : entries) {
        QString verPath = versionsDir.filePath(versionId);

        // === Find version JSON (flexible static helper) ===
        QString jsonPath = VersionBackend::findVersionJson(verPath, versionId);
        if (jsonPath.isEmpty()) continue;

        // Parse JSON to get the real version id
        QFile jf(jsonPath);
        QByteArray jsonBytes;
        QJsonObject verJson;
        if (jf.open(QIODevice::ReadOnly)) {
            jsonBytes = jf.readAll();
            jf.close();
            QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
            if (doc.isObject()) verJson = doc.object();
        }
        QString jsonText = QString::fromUtf8(jsonBytes);

        // === Find the main JAR (flexible static helper) ===
        QString jarPath = VersionBackend::findVersionJar(verPath, versionId);
        // If no jar found, that's OK — loader versions inherit from vanilla

        QVariantMap detail;
        detail[QStringLiteral("id")] = versionId;

        // ── Determine base MC version from JSON id ──
        QString baseMcVersion = versionId;
        if (!verJson.isEmpty() && verJson.contains(QStringLiteral("id"))) {
            baseMcVersion = verJson.value(QStringLiteral("id")).toString();
        }

        // ── 从版本 JSON 内容检测加载器类型 ──
        QString loaderType = tr("原版");
        QString loaderVersion;

        // Try to detect loader from versionId pattern: "XX.X-neoforge-XX.X" etc.
        static const QRegularExpression loaderIdPattern(R"(^(.+?)-(neoforge|forge|fabric|quilt)-(.+)$)");
        QRegularExpressionMatch loaderMatch = loaderIdPattern.match(versionId);
        if (loaderMatch.hasMatch()) {
            baseMcVersion = loaderMatch.captured(1);
            loaderVersion = loaderMatch.captured(3);
        }

        // 基于 JSON 内容的加载器检测
        // This is more reliable than filesystem markers — a NeoForge version with
        // a mods/ dir but without neoforge/ marker would be misidentified as Forge.
        if (!jsonText.isEmpty()) {
            // Order matters: OptiFine/LiteLoader are standalone; NeoForge must be
            // checked BEFORE Forge (Forge check explicitly excludes "net.neoforge").
            if (jsonText.contains(QStringLiteral("optifine"), Qt::CaseInsensitive)) {
                loaderType = QStringLiteral("OptiFine");
                static const QRegularExpression ofVerRe(QStringLiteral("HD_U_([^\":/]+)"));
                QRegularExpressionMatch ofM = ofVerRe.match(jsonText);
                if (ofM.hasMatch())
                    loaderVersion = ofM.captured(1);
            } else if (jsonText.contains(QStringLiteral("liteloader"), Qt::CaseInsensitive)) {
                loaderType = QStringLiteral("LiteLoader");
            } else if (jsonText.contains(QStringLiteral("net.fabricmc:fabric-loader"))
                       || jsonText.contains(QStringLiteral("org.quiltmc:quilt-loader"))) {
                loaderType = jsonText.contains(QStringLiteral("org.quiltmc:quilt-loader"))
                    ? QStringLiteral("Quilt") : QStringLiteral("Fabric");
                static const QRegularExpression fabricVerRe(
                    QStringLiteral("(?:net\\.fabricmc:fabric-loader|org\\.quiltmc:quilt-loader):([\\d\\.]+(?:\\+build\\.\\d+)?)"));
                QRegularExpressionMatch fM = fabricVerRe.match(jsonText);
                if (fM.hasMatch())
                    loaderVersion = fM.captured(1);
            } else if (jsonText.contains(QStringLiteral("net.neoforge"))) {
                loaderType = QStringLiteral("NeoForge");
                // 查找 --fml.forgeVersion 或 --fml.neoForgeVersion
                static const QRegularExpression neoVerRe1(QStringLiteral("\"forgeVersion\"\\s*,\\s*\"([^\"]+)\""));
                QRegularExpressionMatch nM1 = neoVerRe1.match(jsonText);
                if (nM1.hasMatch()) {
                    loaderVersion = nM1.captured(1);
                } else {
                    static const QRegularExpression neoVerRe2(QStringLiteral("\"neoForgeVersion\"\\s*,\\s*\"([^\"]+)\""));
                    QRegularExpressionMatch nM2 = neoVerRe2.match(jsonText);
                    if (nM2.hasMatch())
                        loaderVersion = nM2.captured(1);
                }
            } else if (jsonText.contains(QStringLiteral("minecraftforge"))
                       && !jsonText.contains(QStringLiteral("net.neoforge"))) {
                loaderType = QStringLiteral("Forge");
                // 依次尝试 forge:MCVER-BUILD、net.minecraftforge:minecraftforge:VER、fmlloader
                static const QRegularExpression forgeVerRe1(
                    QStringLiteral("forge:[\\d\\.]+(?:_pre\\d*)?-([\\d\\.]+)"));
                QRegularExpressionMatch fgM = forgeVerRe1.match(jsonText);
                if (fgM.hasMatch()) {
                    loaderVersion = fgM.captured(1);
                } else {
                    static const QRegularExpression forgeVerRe2(
                        QStringLiteral("net\\.minecraftforge:minecraftforge:([\\d\\.]+)"));
                    fgM = forgeVerRe2.match(jsonText);
                    if (fgM.hasMatch()) {
                        loaderVersion = fgM.captured(1);
                    } else {
                        static const QRegularExpression forgeVerRe3(
                            QStringLiteral("net\\.minecraftforge:fmlloader:[\\d\\.]+-([\\d\\.]+)"));
                        fgM = forgeVerRe3.match(jsonText);
                        if (fgM.hasMatch())
                            loaderVersion = fgM.captured(1);
                    }
                }
            }
        }

        // Fallback: versionId pattern when JSON detection didn't match
        if (loaderType == tr("原版") && loaderMatch.hasMatch()) {
            QString key = loaderMatch.captured(2);
            if (key == QStringLiteral("neoforge")) loaderType = QStringLiteral("NeoForge");
            else if (key == QStringLiteral("forge")) loaderType = QStringLiteral("Forge");
            else if (key == QStringLiteral("fabric")) loaderType = QStringLiteral("Fabric");
            else if (key == QStringLiteral("quilt")) loaderType = QStringLiteral("Quilt");
        }
        detail[QStringLiteral("loaderType")] = loaderType;
        detail[QStringLiteral("loaderVersion")] = loaderVersion;

        // ── Version type: use base MC version for manifest lookup ──
        QString vtype = knownTypes.value(baseMcVersion, QString());
        if (vtype.isEmpty()) {
            // Not in manifest — run heuristic ONLY for vanilla versions
            if (loaderType == tr("原版") && baseMcVersion.contains(QRegularExpression(QStringLiteral("\\dw\\d|alpha|beta|inf"))))
                vtype = QStringLiteral("old");
            else
                vtype = QStringLiteral("release");
        } else {
            // Map Mojang raw types to simplified display types
            if (vtype == QStringLiteral("snapshot")) {
                vtype = QStringLiteral("snapshot");
            } else if (vtype == QStringLiteral("release")) {
                vtype = QStringLiteral("release");
            } else {
                vtype = QStringLiteral("old");  // old_alpha, old_beta, pending, etc.
            }
        }
        detail[QStringLiteral("versionType")] = vtype;

        // Release time (Unix ms) for sorting — use base MC version for loader versions
        QDateTime rt = releaseTimes.value(baseMcVersion);
        if (!rt.isValid()) rt = releaseTimes.value(versionId);
        detail[QStringLiteral("releaseTimeMs")] = rt.isValid() ? rt.toMSecsSinceEpoch() : 0;

        // 快速估算大小（避免递归遍历整个目录）
        qint64 totalSize = 0;
        QFileInfo jarFi(jarPath);
        if (jarFi.exists()) totalSize += jarFi.size();
        QFileInfo jsonFi(jsonPath);
        if (jsonFi.exists()) totalSize += jsonFi.size();
        // 只统计顶层子目录里的直接文件（不递归子子目录）
        static const QStringList topDirs = {
            QStringLiteral("mods"), QStringLiteral("resourcepacks"),
            QStringLiteral("shaderpacks"), QStringLiteral("saves")
        };
        for (const QString& d : topDirs) {
            QDir sub(verPath + QStringLiteral("/") + d);
            if (sub.exists()) {
                const auto files = sub.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
                for (const QFileInfo& fi : files)
                    totalSize += fi.size();
            }
        }
        detail[QStringLiteral("sizeBytes")] = totalSize;

        // Count mods
        int modCount = 0;
        QString modsDir = verPath + QStringLiteral("/mods");
        if (QDir(modsDir).exists()) {
            QDirIterator modIt(modsDir, QStringList() << QStringLiteral("*.jar"), QDir::Files);
            while (modIt.hasNext()) { modIt.next(); modCount++; }
        }
        detail[QStringLiteral("modCount")] = modCount;
        {
            // 整合包标记：isModpack=true 时读取标记内图标 URL（下载页传入），版本选择直接用真实图标
            const QString markerPath = verPath + QStringLiteral("/.shadow_modpack");
            const bool isModpack = QFileInfo::exists(markerPath);
            detail[QStringLiteral("isModpack")] = isModpack;
            if (isModpack) {
                const QString localIcon = verPath + QStringLiteral("/modpack_icon.png");
                // 本地图标优先（导入时已解码落盘）；无则退回标记里的 URL
                detail[QStringLiteral("modpackIconPath")] =
                    QFileInfo::exists(localIcon) ? QUrl::fromLocalFile(localIcon).toString() : QString();
                QFile mf(markerPath);
                if (mf.open(QIODevice::ReadOnly)) {
                    const QJsonDocument md = QJsonDocument::fromJson(mf.readAll());
                    mf.close();
                    if (md.isObject())
                        detail[QStringLiteral("modpackIcon")] =
                            md.object().value(QStringLiteral("icon")).toString();
                }
            }
        }
        detail[QStringLiteral("jsonPath")] = jsonPath;
        detail[QStringLiteral("jarPath")] = jarPath;

        local.append(detail);
    }

        }  // end if (versionsDir.exists())
        QMetaObject::invokeMethod(this, [this, local]() {
            m_versionDetails = local;
            emit versionDetailsReady();
            emit logMessage(tr("已扫描 %1 个已安装版本").arg(m_versionDetails.size()));
            m_isScanningVersions = false;
            emit scanningChanged();
        }, Qt::QueuedConnection);
        });  // end QtConcurrent::run lambda
    });  // end outer QTimer::singleShot lambda
}

QVariantMap ShadowBackend::systemMemoryInfo() const {
    return m_settings->getMemoryStatus();
}

// ============================================================
// Launch property getters
// ============================================================

bool ShadowBackend::isLaunching() const {
    return m_launch->isLaunching();
}

int ShadowBackend::launchProgress() const {
    return m_launch->launchProgress();
}

QString ShadowBackend::launchStatus() const {
    return m_launch->launchStatus();
}

bool ShadowBackend::isRunning() const {
    return m_launch->isRunning();
}


int ShadowBackend::runningCount() const {
    return m_launch->runningCount();
}

// ============================================================
// Resource property getters
// ============================================================

bool ShadowBackend::isResourceDownloading() const {
    return m_resource->isDownloading();
}

// ============================================================
// App property getters
// ============================================================

QString ShadowBackend::gameDir() const {
    return m_app->gameDir();
}

QString ShadowBackend::appDataDir() const {
    return m_app->dataDir();
}

QString ShadowBackend::theme() const {
    return m_app->theme();
}

QString ShadowBackend::appVersion() const {
    return m_app->appVersion();
}

bool ShadowBackend::devMode() const {
    return m_app->devMode();
}

static QString agreementConsentPath()
{
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("agreement_consent.txt"));
}

bool ShadowBackend::agreementAccepted() const
{
    // Read consent from file next to the executable
    QFile f(agreementConsentPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QString content = QString::fromUtf8(f.readAll()).trimmed();
    QStringList parts = content.split('|');
    if (parts.size() < 2)
        return false;
    return parts[0] == AppBackend::AGREEMENT_VERSION && parts[1] == QStringLiteral("true");
}

void ShadowBackend::setMarkAgreed(bool v)
{
    if (!v) return;
    // Write consent file: VERSION|true
    QFile f(agreementConsentPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QString line = QString::fromLatin1(AppBackend::AGREEMENT_VERSION)
                       + QStringLiteral("|true\n");
        f.write(line.toUtf8());
        f.close();
    }
    emit agreementAcceptedChanged();
}

static QString readQrcFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QStringLiteral("<p>无法加载协议文件</p>");
    return QString::fromUtf8(f.readAll());
}

QString ShadowBackend::betaAgreementHtml() const
{
    return readQrcFile(QStringLiteral(":/qt/qml/ShadowLauncher/agreements/qml/agreements/beta_agreement.html"));
}

QString ShadowBackend::privacyAgreementHtml() const
{
    return readQrcFile(QStringLiteral(":/qt/qml/ShadowLauncher/agreements/qml/agreements/privacy_policy.html"));
}

QString ShadowBackend::termsAgreementHtml() const
{
    return readQrcFile(QStringLiteral(":/qt/qml/ShadowLauncher/agreements/qml/agreements/terms_of_service.html"));
}

// ============================================================
// Q_INVOKABLE methods — Account
// ============================================================

void ShadowBackend::offlineLogin(const QString& username) {
    m_account->offlineLogin(username);
}

bool ShadowBackend::isOfflineRestricted() const {
    // 地区由 GeoIpService 异步检测（ip-api.com，24h 缓存）；启动后 3s 触发
    const QString region = m_geoIp ? m_geoIp->cachedRegion().toUpper().trimmed() : QString();
    // 只有明确检测为 CN（中国大陆）才放行；
    // 未检测到地区（检测失败/被屏蔽/断网）同样受限——防止绕过合规限制
    if (region == QStringLiteral("CN")) return false;
    // 非中国大陆（国外/港澳台）或未检测到地区，且未正版登录 → 限制
    return !m_account->isLoggedIn();
}

void ShadowBackend::updateOfflineSkin(const QString& username) {
    m_account->updateOfflineSkin(username);
}

void ShadowBackend::removeOfflineUsername(const QString& username) {
    m_account->removeOfflineUsername(username);
}

void ShadowBackend::microsoftLogin() {
    m_account->microsoftLogin();
}

void ShadowBackend::cancelMicrosoftLogin() {
    m_account->cancelMicrosoftLogin();
}

void ShadowBackend::logout() {
    m_account->logout();
}

// ============================================================
// Q_INVOKABLE methods — Settings / Java
// ============================================================

QVariantList ShadowBackend::scanJavaInstallations() {
    emit logMessage(tr("正在扫描 Java 环境..."));
    QVariantList list = m_settings->scanJavaInstallations();
    qCInfo(logJava) << QStringLiteral("Java扫描完成 检出=%1").arg(list.size());
    return list;
}

QString ShadowBackend::autoSelectJava() {
    return m_settings->autoSelectJava();
}

QString ShadowBackend::detectJava() {
    return m_settings->autoSelectJava();
}

QString ShadowBackend::jvmArgs() const {
    return m_app->jvmArgs();
}

void ShadowBackend::setJvmArgs(const QString& args) {
    m_app->setJvmArgs(args);
    emit logMessage(tr("[JVM] GC参数已更新: %1").arg(args));
    saveJvmArgs();
    emit jvmArgsChanged();
}

QString ShadowBackend::gameArgs() const {
    return m_app->gameArgs();
}

void ShadowBackend::setGameArgs(const QString& args) {
    m_app->setGameArgs(args);
    emit logMessage(tr("[GAME] 游戏附加参数已更新: %1").arg(args));
    saveGameArgs();
    emit gameArgsChanged();
}

bool ShadowBackend::highPerfGpu() const {
    return m_app->highPerfGpu();
}

void ShadowBackend::setHighPerfGpu(bool v) {
    m_app->setHighPerfGpu(v);
    emit logMessage(tr("[GPU] 高性能显卡模式: %1").arg(v ? QStringLiteral("开启") : QStringLiteral("关闭")));
    saveHighPerfGpu();
    emit highPerfGpuChanged();
}


// ── Persistence helpers (delegated to SettingsBackend-style QSettings) ──
void ShadowBackend::saveJvmArgs() {
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    s.setValue(QStringLiteral("java/jvmArgs"), m_app->jvmArgs());
}

void ShadowBackend::saveGameArgs() {
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    s.setValue(QStringLiteral("java/gameArgs"), m_app->gameArgs());
}

void ShadowBackend::saveHighPerfGpu() {
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    s.setValue(QStringLiteral("java/highPerfGpu"), m_app->highPerfGpu());
}


void ShadowBackend::loadJavaRuntimeSettings() {
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    m_app->setJvmArgs(s.value(QStringLiteral("java/jvmArgs"), QString()).toString());
    m_app->setGameArgs(s.value(QStringLiteral("java/gameArgs"), QString()).toString());
    m_app->setHighPerfGpu(s.value(QStringLiteral("java/highPerfGpu"), false).toBool());
}

QString ShadowBackend::browseJava() {
    emit logMessage(tr("用户手动选择 Java 环境..."));
    return m_settings->browseJava();
}

void ShadowBackend::selectJavaByIndex(int index) {
    qCInfo(logJava) << QStringLiteral("选择Java index=%1").arg(index);
    m_settings->selectJavaByIndex(index);
}

QVariantMap ShadowBackend::getMemoryStatus() {
    return m_settings->getMemoryStatus();
}

void ShadowBackend::setMinMemory(int mb) {
    m_settings->setMinMemory(mb);
}

void ShadowBackend::setMaxMemory(int mb) {
    m_settings->setMaxMemory(mb);
}

void ShadowBackend::setIsolationEnabled(bool enabled) {
    m_isolationEnabled = enabled;
    m_settings->setIsolationEnabled(enabled);
}

QString ShadowBackend::getVersionGameDir(const QString& versionId) const {
    return m_settings->getVersionGameDir(versionId);
}

void ShadowBackend::migrateVersionToIsolated(const QString& versionId) {
    m_settings->migrateVersionToIsolated(versionId);
}

QObject* ShadowBackend::modManager() const
{
    return m_resource ? m_resource->modManager() : nullptr;
}

QObject* ShadowBackend::multiplayer() const
{
    return m_multiplayer;
}

QString ShadowBackend::resolveIconUrl(const QString &url)
{
    return m_fetchEngine ? m_fetchEngine->iconLocalPath(url) : url;
}

void ShadowBackend::cacheIconBatchAsync(const QStringList &urls)
{
    if (m_fetchEngine) m_fetchEngine->prefetchIcons(urls);
}

QString ShadowBackend::resolveShaderIconUrl(const QString &url)
{
    return m_fetchEngine ? m_fetchEngine->iconLocalPath(url) : url;
}

void ShadowBackend::cacheShaderIconBatchAsync(const QStringList &urls)
{
    if (m_fetchEngine) m_fetchEngine->prefetchIcons(urls);
}

QString ShadowBackend::resolveRpIconUrl(const QString &url)
{
    return m_fetchEngine ? m_fetchEngine->iconLocalPath(url) : url;
}

void ShadowBackend::cacheRpIconBatchAsync(const QStringList &urls)
{
    if (m_fetchEngine) m_fetchEngine->prefetchIcons(urls);
}

// ── Helper: get the game directory for a given version ID ──
QString ShadowBackend::gameDirForVersion(const QString& versionId) const {
    if (versionId.isEmpty()) return m_app->gameDir();
    return getVersionGameDir(versionId);
}

void ShadowBackend::syncPlayerName()
{
    if (!m_multiplayer) return;

    // Priority: Microsoft name > offline name > default "Steve"
    if (m_account->isOnline() && !m_account->username().isEmpty()) {
        m_multiplayer->setPlayerName(m_account->username());
    } else if (!m_account->offlineUsername().isEmpty()) {
        m_multiplayer->setPlayerName(m_account->offlineUsername());
    }
    // else: keep default "Steve" from MultiplayerManager constructor
}

bool ShadowBackend::openGameDir(const QString& versionId) {
    QString dir = gameDirForVersion(versionId);
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    emit logMessage(tr("已打开游戏目录") + (versionId.isEmpty() ? QString() : QStringLiteral(" (") + versionId + QStringLiteral(")")));
    return true;
}

bool ShadowBackend::openLatestLog(const QString& versionId) {
    QString gameDir = gameDirForVersion(versionId);
    QString latestLog = gameDir + QStringLiteral("/logs/latest.log");
    if (QFileInfo::exists(latestLog)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(latestLog));
        emit logMessage(tr("已打开最新日志"));
        return true;
    }
    return false;
}

bool ShadowBackend::isVersionInstalled(const QString& versionId) const {
    if (versionId.isEmpty()) return false;
    return QFileInfo::exists(m_app->gameDir() + QStringLiteral("/versions/") + versionId
                             + QStringLiteral("/") + versionId + QStringLiteral(".json"));
}

bool ShadowBackend::openLogsFolder(const QString& versionId) {
    if (!isVersionInstalled(versionId)) {
        qCWarning(logMod) << "[openLogsFolder] 版本不存在，拒绝打开/创建:" << versionId;
        return false;
    }
    QString logsDir = gameDirForVersion(versionId) + QStringLiteral("/logs");
    QDir().mkpath(logsDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(logsDir));
    emit logMessage(tr("已打开日志目录"));
    return true;
}

bool ShadowBackend::openLauncherLogsFolder() {
    QString logsDir = QCoreApplication::applicationDirPath() + QStringLiteral("/logs");
    QDir().mkpath(logsDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(logsDir));
    emit logMessage(tr("已打开启动器日志目录"));
    return true;
}

bool ShadowBackend::openCrashLog(const QString& versionId) {
    QString gameDir = gameDirForVersion(versionId);
    QString crashDir = gameDir + QStringLiteral("/crash-reports");
    QDir cd(crashDir);
    if (cd.exists()) {
        QStringList filters; filters << QStringLiteral("*.txt");
        QFileInfoList entries = cd.entryInfoList(filters, QDir::Files, QDir::Time);
        if (!entries.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(entries.first().absoluteFilePath()));
            emit logMessage(tr("已打开崩溃日志"));
            return true;
        }
    }
    // Fallback: check versions/{versionId}/game/crash-reports/
    QString altCrashDir = m_app->gameDir() + QStringLiteral("/versions/") + versionId + QStringLiteral("/game/crash-reports");
    QDir acd(altCrashDir);
    if (acd.exists()) {
        QStringList filters; filters << QStringLiteral("*.txt");
        QFileInfoList entries = acd.entryInfoList(filters, QDir::Files, QDir::Time);
        if (!entries.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(entries.first().absoluteFilePath()));
            emit logMessage(tr("已打开崩溃日志 (隔离目录)"));
            return true;
        }
    }
    return false;
}

void ShadowBackend::analyzeCrashNow() {
    if (m_launch) m_launch->analyzeCrashNow();
}

QString ShadowBackend::exportCrashLogs(const QString& destDir) {
    if (!m_launch) return {};
    return m_launch->exportCrashLogs(destDir);
}

QString ShadowBackend::exportLaunchScript(const QString& versionId, const QString& javaPath,
                                          int maxMemoryMB, const QString& jvmArgs,
                                          const QString& gameArgs, bool highPerfGpu) {
    if (!m_launch) return {};
    // 对齐启动流程：Java 按版本需求匹配（不能直接用全局 javaPath——26.2-forge 需要
    // Java 25，全局 Java 17 会导致 UnsupportedClassVersionError class file 69.0）
    QString resolvedJava = javaPath;
    if (m_settings) {
        const int requiredMajor = requiredJavaMajor(versionId);
        int maxMajor = 0;
        if (requiredMajor == 17) maxMajor = 21;
        const QString manualJava = m_settings->javaPath();
        if (!manualJava.isEmpty() && QFileInfo::exists(manualJava)
            && m_settings->javaMajor() >= requiredMajor
            && (maxMajor <= 0 || m_settings->javaMajor() <= maxMajor)) {
            resolvedJava = manualJava;
        } else {
            const QString matched = m_settings->findJavaForVersion(requiredMajor, maxMajor);
            if (!matched.isEmpty()) resolvedJava = matched;
        }
    }
    if (resolvedJava.isEmpty()) return QString();
    // 对齐启动流程：注入账号信息 + 版本隔离目录
    if (m_account) {
        // 离线用 offlineUsername，在线用 username（对齐 launch() 内 auth 注入）
        const bool online = m_account->isOnline();
        const QString name = online ? m_account->username() : m_account->offlineUsername();
        const QString uuid = online ? m_account->accountUuid() : m_account->offlineUuid();
        m_launch->setAuthInfo(name, uuid, online ? m_account->mcToken() : QString(), online);
    }
    if (m_settings)
        m_launch->setVersionGameDir(m_settings->getVersionGameDir(versionId));
    return m_launch->exportLaunchScript(versionId, resolvedJava, maxMemoryMB,
                                        jvmArgs, gameArgs, highPerfGpu);
}

bool ShadowBackend::saveTextFile(const QString& path, const QString& content) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    f.write(content.toUtf8());
    f.close();
    return true;
}

void ShadowBackend::openPath(const QString& path) {
    if (m_launch) m_launch->openPath(path);
}

void ShadowBackend::cleanupCrashArtifacts() {
    if (m_launch) m_launch->cleanupCrashArtifacts();
}

bool ShadowBackend::openSavesFolder(const QString& versionId) {
    if (!isVersionInstalled(versionId)) {
        qCWarning(logMod) << "[openSavesFolder] 版本不存在，拒绝打开/创建:" << versionId;
        return false;
    }
    QString savesDir = gameDirForVersion(versionId) + QStringLiteral("/saves");
    QDir().mkpath(savesDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(savesDir));
    emit logMessage(tr("已打开存档文件夹"));
    return true;
}

bool ShadowBackend::openScreenshotsFolder(const QString& versionId) {
    if (!isVersionInstalled(versionId)) {
        qCWarning(logMod) << "[openScreenshotsFolder] 版本不存在，拒绝打开/创建:" << versionId;
        return false;
    }
    QString screenshotsDir = gameDirForVersion(versionId) + QStringLiteral("/screenshots");
    QDir().mkpath(screenshotsDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(screenshotsDir));
    emit logMessage(tr("已打开截图文件夹"));
    return true;
}

bool ShadowBackend::openModsFolder(const QString& versionId) {
    if (!isVersionInstalled(versionId)) {
        qCWarning(logMod) << "[openModsFolder] 版本不存在，拒绝打开/创建:" << versionId;
        return false;
    }
    QString modsDir = gameDirForVersion(versionId) + QStringLiteral("/mods");
    QDir().mkpath(modsDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(modsDir));
    emit logMessage(tr("已打开 Mod 文件夹"));
    return true;
}

bool ShadowBackend::openResourcePacksFolder(const QString& versionId) {
    if (!isVersionInstalled(versionId)) {
        qCWarning(logMod) << "[openResourcePacksFolder] 版本不存在，拒绝打开/创建:" << versionId;
        return false;
    }
    QString rpDir = gameDirForVersion(versionId) + QStringLiteral("/resourcepacks");
    QDir().mkpath(rpDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(rpDir));
    emit logMessage(tr("已打开资源包文件夹"));
    return true;
}

bool ShadowBackend::openShaderPacksFolder(const QString& versionId) {
    QString spDir = gameDirForVersion(versionId) + QStringLiteral("/shaderpacks");
    QDir().mkpath(spDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(spDir));
    emit logMessage(tr("已打开光影包文件夹"));
    return true;
}

// ── Mod/local file operations ──

QVariantList ShadowBackend::listMods(const QString& versionId) const
{
    if (!m_localMods) return {};
    return m_localMods->scanMods(versionId);
}

QVariantList ShadowBackend::listResourcePacks(const QString& versionId) const
{
    if (!m_localMods) return {};
    return m_localMods->scanResourcePacks(versionId);
}

// ── 异步列表加载：parseJar / 目录遍历在 worker 线程，完成后回主线程发信号 ──
void ShadowBackend::listModsAsync(const QString& versionId)
{
    if (!m_localMods) return;
    LocalModManager* lmm = m_localMods;
    QtConcurrent::run([this, lmm, versionId]() {
        const QVariantList result = lmm->scanMods(versionId);
        QMetaObject::invokeMethod(this, [this, versionId, result]() {
            emit modsListReady(versionId, result);
        }, Qt::QueuedConnection);
    });
}

void ShadowBackend::listResourcePacksAsync(const QString& versionId)
{
    if (!m_localMods) return;
    LocalModManager* lmm = m_localMods;
    QtConcurrent::run([this, lmm, versionId]() {
        const QVariantList result = lmm->scanResourcePacks(versionId);
        QMetaObject::invokeMethod(this, [this, versionId, result]() {
            emit resourcePacksListReady(versionId, result);
        }, Qt::QueuedConnection);
    });
}

static qint64 computeDirSize(const QString& path)
{
    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (it.fileInfo().isFile())
            total += it.fileInfo().size();
    }
    return total;
}

// ── 辅助：格式化文件大小为可读字符串 ──
static QString formatSizeDisplay(qint64 bytes)
{
    if (bytes < 1024) return QString::number(bytes) + QStringLiteral(" B");
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
    if (bytes < 1024LL * 1024 * 1024)
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + QStringLiteral(" GB");
}

QVariantList ShadowBackend::listSaves(const QString& versionId) const
{
    QString savesDir = gameDirForVersion(versionId) + QStringLiteral("/saves");
    QDir dir(savesDir);
    QVariantList result;
    if (dir.exists()) {
        const auto dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const auto& fi : dirs) {
            QVariantMap m;
            m[QStringLiteral("name")] = fi.fileName();

            // Size (reuse the cached result from fi if available — QFileInfo caches)
            qint64 sizeBytes = computeDirSize(fi.absoluteFilePath());
            m[QStringLiteral("sizeBytes")] = sizeBytes;
            m[QStringLiteral("sizeDisplay")] = formatSizeDisplay(sizeBytes);

            // Icon
            QString iconPath = fi.absoluteFilePath() + QStringLiteral("/icon.png");
            if (QFile::exists(iconPath))
                m[QStringLiteral("iconPath")] = QUrl::fromLocalFile(iconPath).toString();
            else
                m[QStringLiteral("iconPath")] = QString();

            result.append(m);
        }
    }
    return result;
}


void ShadowBackend::listSavesAsync(const QString& versionId)
{
    const QString savesDir = gameDirForVersion(versionId) + QStringLiteral("/saves");
    QtConcurrent::run([this, versionId, savesDir]() {
        QVariantList result;
        QDir dir(savesDir);
        if (dir.exists()) {
            const auto dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            for (const auto& fi : dirs) {
                QVariantMap m;
                m[QStringLiteral("name")] = fi.fileName();
                qint64 sizeBytes = 0;
                QDirIterator it(fi.absoluteFilePath(), QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                                QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    it.next();
                    if (it.fileInfo().isFile()) sizeBytes += it.fileInfo().size();
                }
                m[QStringLiteral("sizeBytes")] = sizeBytes;
                m[QStringLiteral("sizeDisplay")] = formatSizeDisplay(sizeBytes);
                QString iconPath = fi.absoluteFilePath() + QStringLiteral("/icon.png");
                m[QStringLiteral("iconPath")] = QFile::exists(iconPath)
                    ? QUrl::fromLocalFile(iconPath).toString() : QString();
                result.append(m);
            }
        }
        QMetaObject::invokeMethod(this, [this, versionId, result]() {
            emit savesListReady(versionId, result);
        }, Qt::QueuedConnection);
    });
}

// ── 辅助：递归计算目录大小 ──
void ShadowBackend::deleteSave(const QString& saveName, const QString& versionId)
{
    if (saveName.isEmpty()) return;
    QString savePath = gameDirForVersion(versionId) + QStringLiteral("/saves/") + saveName;
    QDir dir(savePath);
    if (!dir.exists()) {
        emit logMessage(QStringLiteral("存档不存在: ") + saveName);
        return;
    }
    if (dir.removeRecursively()) {
        emit logMessage(QStringLiteral("已删除存档: ") + saveName);
    } else {
        emit logMessage(QStringLiteral("删除存档失败: ") + saveName);
    }
}

void ShadowBackend::deleteMod(const QString& filename, const QString& versionId)
{
    if (!m_localMods) return;
    if (m_localMods->deleteMod(filename, versionId))
        emit logMessage(QStringLiteral("已删除 Mod: ") + filename);
}

void ShadowBackend::setModEnabled(const QString& filename, const QString& versionId, bool enabled)
{
    if (!m_localMods) return;
    if (m_localMods->setModEnabled(filename, versionId, enabled))
        emit logMessage(QStringLiteral("%1 Mod: %2").arg(enabled ? QStringLiteral("已启用") : QStringLiteral("已禁用"), filename));
}

void ShadowBackend::deleteResourcePack(const QString& filename, const QString& versionId)
{
    if (!m_localMods) return;
    if (m_localMods->deleteResourcePack(filename, versionId))
        emit logMessage(QStringLiteral("已删除资源包: ") + filename);
}

bool ShadowBackend::importMod(const QString& filePath, const QString& versionId)
{
    if (!m_localMods) return false;
    return m_localMods->importMod(filePath, versionId);
}

bool ShadowBackend::importResourcePack(const QString& filePath, const QString& versionId)
{
    if (!m_localMods) return false;
    return m_localMods->importResourcePack(filePath, versionId);
}

bool ShadowBackend::openVersionDir(const QString& versionId) {
    return m_settings->openVersionDir(versionId);
}

void ShadowBackend::deleteVersion(const QString& versionId) {
    m_settings->deleteVersion(versionId);
    // Refresh the installed list (deleted version disappears)
    m_version->refreshInstalled();
    // If the deleted version was selected, reset to unselected
    if (m_version->selectedVersion() == versionId) {
        m_version->setSelectedVersion(QString());
        // Inform QML so it can explicitly re-read the value
        emit selectedVersionClearedAfterDelete();
    }
}

// ============================================================
// Q_INVOKABLE methods — Version
// ============================================================

void ShadowBackend::refreshVersionList() {
    m_version->refreshVersionList();
}

void ShadowBackend::refreshInstalled() {
    m_version->refreshInstalled();
}

void ShadowBackend::refreshInstalledList() {
    QTimer::singleShot(0, this, [this]() {
        if (m_isScanningVersions) return;
        m_isScanningVersions = true;
        emit scanningChanged();
        QTimer::singleShot(0, this, [this]() {
            m_version->refreshInstalled();
            m_isScanningVersions = false;
            emit scanningChanged();
        });
    });
}

void ShadowBackend::installVersion(const QString& versionId) {
    m_version->installVersion(versionId);
}

void ShadowBackend::cancelVersionInstall(const QString& versionId) {
    // ── 资源文件下载卡片（mod:N）：取消下载并移除卡片 ──
    if (versionId.startsWith(QStringLiteral("mod:"))) {
        bool ok = false;
        const int dlId = versionId.mid(4).toInt(&ok);
        if (ok) {
            m_resource->cancelModFileDownload(dlId);
            if (m_packDownloads.contains(dlId)) {
                m_packDownloads.remove(dlId);
                m_packDownloading = false;   // 取消整合包下载 → 释放单任务占位
            }
            if (m_modDownloadCards.contains(dlId)) {
                if (m_version) m_version->removeResourceCard(m_modDownloadCards[dlId]);
                m_modDownloadCards.remove(dlId);
            }
        }
        return;
    }
    if (m_version) m_version->cancelVersionInstall(versionId);
}

void ShadowBackend::dismissCard(const QString& installId) {
    if (m_version) m_version->dismissCard(installId);
}

void ShadowBackend::cancelQueuedDownload(const QString& versionId) {
    if (m_version) m_version->cancelQueuedDownload(versionId);
}

void ShadowBackend::setSelectedVersion(const QString& versionId) {
    m_version->setSelectedVersion(versionId);
    m_settings->setLastLaunchedVersion(versionId);
}

// ============================================================
// Q_INVOKABLE methods — Launch
// ============================================================

void ShadowBackend::launch(const QString& versionId, bool online) {
    // Prepare server.properties for multiplayer (disable online-mode)
    if (m_multiplayer) {
        m_multiplayer->prepareServerProperties(m_app->gameDir(), versionId);
    }

    // 外置登录模式：从 yggdrasil backend 获取用户名和 token
    // 三种登录方式严格分离（用户要求）：0=正版(微软账号)、1=离线(离线名)、2=外置(ygg)
    QString username;
    auto *ygg = static_cast<YggdrasilBackend*>(m_yggdrasil);
    if (m_lastLoginMode == 2 && ygg && ygg->loggedIn()) {
        username = ygg->username();
        // 通知 launch backend 使用外置登录
        m_launch->setYggdrasilMode(ygg->apiRoot(), ygg->accessToken());
    } else if (m_lastLoginMode == 1) {
        // 离线登录：用离线用户名（m_account->username() 是微软正版账号名，离线时为空）
        username = m_account->offlineUsername();
        m_launch->clearYggdrasilMode();
    } else {
        // 正版登录：微软账号用户名
        username = m_account->username();
        m_launch->clearYggdrasilMode();
    }

    // Check per-version memory override
    int verMode = m_settings->versionMemoryMode(versionId);
    int maxMemory;
    if (verMode == 1) {
        // Per-version: auto
        maxMemory = m_launch->getAutoMemoryForVersion(versionId);
    } else if (verMode == 2) {
        // Per-version: manual
        maxMemory = m_settings->versionMemoryManualMB(versionId);
    } else {
        // Per-version: follow global (0 or unset)
        maxMemory = m_settings->autoMemoryEnabled()
            ? m_launch->getAutoMemoryForVersion(versionId)
            : m_settings->maxMemoryMB();
    }
    qCInfo(logApp) << QStringLiteral("启动内存: %1MB (verMode=%2, auto=%3)").arg(maxMemory).arg(verMode).arg(m_settings->autoMemoryEnabled());
    QString jvmArgs = resolvedJvmArgs(versionId);
    QString gameArgs = resolvedGameArgs(versionId);

    // 外置登录：自动进入服务器
    auto *yggServer = static_cast<YggdrasilBackend*>(m_yggdrasil);
    if (m_lastLoginMode == 2 && yggServer && yggServer->loggedIn()
            && yggServer->autoJoinServer() && !yggServer->serverAddress().isEmpty()) {
        QString addr = yggServer->serverAddress();
        // 二次验证（防御性编程，理论上 setter 已过滤）
        if (!YggdrasilBackend::isValidServerAddress(addr)) {
            qCWarning(logApp) << QStringLiteral("[服务端] 服务器地址无效，跳过自动连接: %1").arg(addr);
        } else {
            QStringList parts = addr.split(QLatin1Char(':'));
            QString host = parts.value(0);
            int port = 25565;
            if (parts.size() >= 2) {
                bool ok = false;
                int parsed = parts.value(1).toInt(&ok);
                if (ok && parsed > 0 && parsed <= 65535)
                    port = parsed;
            }
            gameArgs += QStringLiteral(" --server %1 --port %2").arg(host).arg(port);
            qCInfo(logApp) << QStringLiteral("[服务端] 自动连接: %1:%2").arg(host).arg(port);
        }
    }

    bool highPerfGpu = resolvedHighPerfGpu(versionId);
    m_launchVersion = versionId;
    m_launchUsername = username;

    // Determine required Java version from version JSON
    int requiredMajor = requiredJavaMajor(versionId);
    
    // Find best matching Java: prefer manual selection, fallback to auto-match
    QString javaPath;
    QString manualJava = m_settings->javaPath();

    // 兼容上限：老版本 Forge/Mixin 不支持太新的 Java 类格式
    //   8（LWJGL2 / pre-1.13）：精确 8（Java 9+ 模块系统问题，已有 needExactJava8 处理）
    //   17（1.17-1.20.4）：上限 21（Forge 47.x 的 Mixin 不认识 >Java 21 的类格式，
    //       默认 Java 25 时 25>=17 会被旧逻辑放行 → Mixin 崩溃）
    //   21+（1.20.5+）：新 Mixin，无上限
    int maxMajor = 0;
    if (requiredMajor == 17) maxMajor = 21;

    if (!manualJava.isEmpty() && QFileInfo::exists(manualJava)) {
        // Check manually configured Java version against version requirement
        int manualMajor = m_settings->javaMajor();

        // LWJGL 2 versions (pre-1.13, requiredMajor==8) need Java 8 specifically:
        // Java 9+ has module system issues and LWJGL 2 compatibility problems.
        // Even with --add-opens flags, old versions work best with Java 8.
        bool needExactJava8 = (requiredMajor == 8 && manualMajor > 8);

        if (needExactJava8) {
            emit logMessage(tr("[提示] 此版本依赖 LWJGL 2，建议使用 Java 8，尝试自动匹配..."));
            javaPath = m_settings->findJavaForVersion(requiredMajor);
            if (!javaPath.isEmpty()) {
                emit logMessage(tr("[完成] 已自动降级 Java 8: %1").arg(javaPath));
            } else {
                // Java 8 not found — fall back to manual Java as best-effort
                emit logMessage(tr("[警告] 未找到 Java 8，降级使用手动配置的 Java %1").arg(manualMajor));
                javaPath = manualJava;
            }
        } else if (manualMajor >= requiredMajor
                   && (maxMajor <= 0 || manualMajor <= maxMajor)) {
            javaPath = manualJava;
            emit logMessage(tr("[完成] 使用设置的 Java %1: %2").arg(manualMajor).arg(javaPath));
        } else {
            emit logMessage(tr("[提示] 设置的 Java %1 (%2) 不满足版本要求 (需要 %3%4)，尝试自动匹配...")
                                .arg(manualMajor).arg(manualJava).arg(requiredMajor)
                                .arg(maxMajor > 0 ? tr("~%1").arg(maxMajor) : tr("+")));
            javaPath = m_settings->findJavaForVersion(requiredMajor, maxMajor);
            if (!javaPath.isEmpty()) {
                emit logMessage(tr("[完成] 已自动匹配 Java %1: %2").arg(requiredMajor).arg(javaPath));
            } else if (!manualJava.isEmpty()) {
                // 区间内无匹配 → 回退用户默认（尽力而为，可能不兼容）
                javaPath = manualJava;
                emit logMessage(tr("[警告] 未找到兼容区间内的 Java，使用设置的 Java %1（可能存在兼容问题）")
                                    .arg(manualMajor));
            }
        }
    } else {
        javaPath = m_settings->findJavaForVersion(requiredMajor, maxMajor);
        if (!javaPath.isEmpty()) {
            emit logMessage(tr("[完成] 已自动匹配 Java %1: %2").arg(requiredMajor).arg(javaPath));
        }
    }

    if (javaPath.isEmpty()) {
        // ── 2026-08-08：Java 缺失 → 自动下载安装后继续启动 ──
        // 有扫描进行中的情况：等扫描完再判断（避免误判缺失）
        if (m_settings->isJavaScanning()) {
            emit logMessage(QStringLiteral("[JAVA] Java scan still in progress, please wait and try again"));
            emit launchBlocked(tr("Java 扫描进行中，请稍后再试"));
            return;
        }
        emit logMessage(tr("[提示] 此版本需要 Java %1，但系统中未找到匹配的 Java，开始自动安装...")
                            .arg(requiredMajor));
        autoInstallJavaThenLaunch(versionId, requiredMajor, online);
        return;
    }

    // Pass auth info based on mode
    if (online) {
        auto *ygg2 = static_cast<YggdrasilBackend*>(m_yggdrasil);
        if (m_lastLoginMode == 2 && ygg2 && ygg2->loggedIn()) {
            // 外置登录
            qCInfo(logLaunch) << QStringLiteral("[认证] 外置登录模式启动 玩家名=%1 uuid=%2").arg(ygg2->username(), ygg2->uuid());
            m_launch->setAuthInfo(ygg2->username(), ygg2->uuid(),
                                  ygg2->accessToken(), true);
        } else {
            qCInfo(logLaunch) << QStringLiteral("[认证] 在线模式启动 玩家名=%1 uuid=%2").arg(m_account->username(), m_account->accountUuid());
            m_launch->setAuthInfo(m_account->username(), m_account->accountUuid(),
                                  m_account->mcToken(), true);
        }
    } else {
        qCInfo(logLaunch) << QStringLiteral("[认证] 离线模式启动 玩家名=%1 uuid=%2").arg(m_account->offlineUsername(), m_account->offlineUuid());
        m_launch->setAuthInfo(m_account->offlineUsername(), m_account->offlineUuid(), QString(), false);
    }

    proceedLaunch(versionId, online, javaPath, maxMemory, jvmArgs, gameArgs, highPerfGpu);
}

void ShadowBackend::proceedLaunch(const QString& versionId, bool online, const QString& javaPath,
                                  int maxMemory, const QString& jvmArgs, const QString& gameArgs,
                                  bool highPerfGpu)
{
    m_launch->setAutoLangMode(m_settings->autoLangMode());
    m_launch->setDetectedRegion(m_geoIp ? m_geoIp->cachedRegion() : QString());
    m_launch->setVersionGameDir(m_settings->getVersionGameDir(versionId));
    m_launch->launch(versionId, m_launchUsername, javaPath, maxMemory, jvmArgs, gameArgs, highPerfGpu,
                     m_settings->windowWidth(), m_settings->windowHeight());
}

void ShadowBackend::autoInstallJavaThenLaunch(const QString& versionId, int requiredMajor, bool online)
{
    if (m_launchJavaInstalling) {
        emit launchBlocked(tr("Java 正在安装中，请稍候"));
        return;
    }
    // 版本归一化：Tuna 镜像无 JRE 16（404），Java 17 完全兼容 1.17（class 格式兼容）
    if (requiredMajor == 16)
        requiredMajor = 17;
    m_launchJavaInstalling = true;
    m_launchJavaRequiredMajor = requiredMajor;
    m_launchJavaInstallVersion = versionId;
    m_launchJavaInstallOnline = online;

    // 快照当前启动参数（安装完成后续用，避免重入 launch() 重新解析）
    // 内存计算与 launch() 完全一致（per-version override → 全局）
    const int verMode = m_settings->versionMemoryMode(versionId);
    if (verMode == 1) {
        m_launchJavaPendingMemory = m_launch->getAutoMemoryForVersion(versionId);
    } else if (verMode == 2) {
        m_launchJavaPendingMemory = m_settings->versionMemoryManualMB(versionId);
    } else {
        m_launchJavaPendingMemory = m_settings->autoMemoryEnabled()
            ? m_launch->getAutoMemoryForVersion(versionId)
            : m_settings->maxMemoryMB();
    }
    m_launchJavaPendingJvmArgs = resolvedJvmArgs(versionId);
    m_launchJavaPendingGameArgs = resolvedGameArgs(versionId);
    m_launchJavaPendingHighPerfGpu = resolvedHighPerfGpu(versionId);

    emit launchCheckWarning(tr("正在自动下载 Java %1...").arg(requiredMajor));
    emit launchCheckProgress(tr("正在自动下载 Java %1...").arg(requiredMajor));
    qCInfo(logLaunch) << QStringLiteral("[JAVA] 启动自动安装 Java %1 (版本 %2)").arg(requiredMajor).arg(versionId);

    // 进度轮询：JavaRuntimeInstaller 下载进度属性 → launchCheckProgress（QML toast 显示）
    QTimer* poll = new QTimer(this);
    poll->setInterval(250);
    connect(poll, &QTimer::timeout, this, [this, poll, requiredMajor]() {
        if (!m_launchJavaInstalling) { poll->stop(); poll->deleteLater(); return; }
        const int pct = m_java->javaDownloadPercent();
        const QString status = m_java->javaInstallStatus();
        if (pct > 0)
            emit launchCheckProgress(tr("正在下载 Java %1 (%2%)...").arg(requiredMajor).arg(pct));
        else if (!status.isEmpty())
            emit launchCheckProgress(status);
    });
    poll->start();

    m_java->installJavaForLaunch(requiredMajor,
        [this, poll, versionId, requiredMajor, online](bool ok, const QString& err, const QString& javaExe) {
            poll->stop();
            poll->deleteLater();
            if (!m_launchJavaInstalling) {
                // 已被 cancelLaunch 中止（已发取消提示），忽略迟到回调
                return;
            }
            m_launchJavaInstalling = false;
            if (!ok) {
                qCWarning(logLaunch) << QStringLiteral("[JAVA] 自动安装 Java %1 失败: %2").arg(requiredMajor).arg(err);
                emit logMessage(tr("[失败] Java %1 自动安装失败: %2").arg(requiredMajor).arg(err));
                emit launchBlocked(tr("Java %1 自动安装失败: %2").arg(requiredMajor).arg(err));
                return;
            }
            qCInfo(logLaunch) << QStringLiteral("[JAVA] 自动安装 Java %1 完成: %2").arg(requiredMajor).arg(javaExe);
            emit logMessage(tr("[完成] Java %1 已自动安装: %2").arg(requiredMajor).arg(javaExe));
            emit launchCheckProgress(tr("Java %1 下载完成，正在刷新 Java 列表...").arg(requiredMajor));

            // 刷新 Java 列表（java_cache 新装的会进入 scanJavaInstallations）——异步，
            // 不阻塞启动；findJavaForVersion 用已知的 javaExe 路径兜底（onDone 已给）
            m_settings->scanJavaInstallations();
            m_java->scanSystemJavas();

            // 用安装回调返回的 java.exe 直接启动（scanJavaInstallations 异步刷新列表，
            // 立即 findJavaForVersion 可能拿旧缓存；安装路径已知，无需再查）
            const QString newJava = javaExe;
            if (newJava.isEmpty() || !QFileInfo::exists(newJava)) {
                emit logMessage(tr("[失败] Java %1 安装完成但可执行文件缺失: %2")
                                    .arg(requiredMajor).arg(newJava));
                emit launchBlocked(tr("Java %1 安装完成，但未找到可执行文件").arg(requiredMajor));
                return;
            }
            emit logMessage(tr("[完成] 使用自动安装的 Java %1 启动").arg(newJava));
            emit launchCheckProgress(tr("Java 就绪，正在启动..."));
            // 刷新列表后重放 auth 注入 + 启动（内存参数沿用快照）
            proceedLaunch(versionId, online, newJava,
                          m_launchJavaPendingMemory, m_launchJavaPendingJvmArgs,
                          m_launchJavaPendingGameArgs, m_launchJavaPendingHighPerfGpu);
        });
}

void ShadowBackend::cancelLaunch() {
    // 自动安装 Java 进行中：中止安装（JavaRuntimeInstaller 支持取消）
    if (m_launchJavaInstalling) {
        m_launchJavaInstalling = false;
        m_java->cancelJavaInstall();
        emit launchBlocked(tr("已取消 Java 自动安装"));
    }
    m_launch->cancelLaunch();
}

int ShadowBackend::requiredJavaMajor(const QString& versionId)
{
    // Walk version JSON + inheritsFrom chain to find javaVersion.majorVersion
    // This handles Forge/NeoForge/Fabric installs where javaVersion may be in the base MC JSON
    QString gameDir = m_app->gameDir();
    QString currentId = versionId;
    QStringList seen;

    while (!currentId.isEmpty() && !seen.contains(currentId)) {
        seen.append(currentId);
        QString jsonPath = gameDir + QStringLiteral("/versions/") + currentId
                         + QStringLiteral("/") + currentId + QStringLiteral(".json");
        QFile f(jsonPath);
        if (!f.open(QIODevice::ReadOnly)) break;

        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        QJsonObject json = doc.object();

        // Check javaVersion field first
        QJsonObject javaVer = json[QStringLiteral("javaVersion")].toObject();
        if (!javaVer.isEmpty()) {
            int major = javaVer[QStringLiteral("majorVersion")].toInt(0);
            if (major > 0) {
                qCInfo(logLaunch) << QStringLiteral("[JAVA] Version %1 inherits %2 → requires Java %3")
                                    .arg(versionId, currentId).arg(major);
                return major;
            }
        }

        // Walk up inheritsFrom chain
        currentId = json[QStringLiteral("inheritsFrom")].toString();
    }

    // Fallback: parse MC version from versionId and infer Java requirement
    // Examples: "1.12.2-forge-14.23.5.2860" → "1.12.2" → Java 8
    //           "26.1.1-forge-63.0.2" → "26.1.1" → no known mapping → try base version
    // Parse MC version: everything before first "-" that follows a digit
    QString mcVer;
    // Strip loader suffix: "X.Y.Z-forge-..." → "X.Y.Z"
    static QRegularExpression mcVerRe(QStringLiteral("^(\\d+\\.\\d+(?:\\.\\d+)?)"));
    QRegularExpressionMatch match = mcVerRe.match(versionId);
    if (match.hasMatch()) {
        mcVer = match.captured(1);
    }
    
    int javaMajor = inferJavaByMcVersion(mcVer);
    qCInfo(logLaunch) << QStringLiteral("[JAVA] Version %1 → MC %2 → inferred Java %3 (no javaVersion in chain)")
                        .arg(versionId, mcVer.isEmpty() ? QStringLiteral("unknown") : mcVer).arg(javaMajor);
    return javaMajor;
}

int ShadowBackend::inferJavaByMcVersion(const QString& mcVersion)
{
    // Fallback: infer Java major version from MC version string.
    // Used when version JSON + inheritsFrom chain has no javaVersion field.
    //
    // Source: Minecraft Wiki + ModReady version table
    //   MC < 1.17                 → Java 8
    //   MC 1.17~1.17.1            → Java 16
    //   MC 1.18~1.20.4            → Java 17
    //   MC 1.20.5~1.21.5          → Java 21
    //   MC 22.x+ (post-rename)    → Java 25 (java-runtime-beta)
    //
    if (mcVersion.isEmpty()) return 8;

    // Parse major.minor[.revision] from version string
    QStringList parts = mcVersion.split(QStringLiteral("."));
    if (parts.size() < 2) return 8;
    
    int major = parts[0].toInt();
    int minor = parts[1].toInt();
    int rev = (parts.size() >= 3) ? parts[2].split(QStringLiteral("-"))[0].toInt() : 0;
    
    // ── Post-rename era: "22.x.x" ~ "26.x.x" ──
    // Mojang renamed 1.22+ to 22.x (e.g. 26.1 = Tiny Takeover, 26.2 = Chaos Cubed)
    // These all use java-runtime-beta → Java 25
    if (major >= 22) return 25;
    
    // ── Classic era: "1.X.Y" ──
    if (major == 1) {
        // 1.21.x (regardless of rev) → Java 21
        if (minor >= 22) return 25;   // 1.22+ (if it exists) → same as post-rename
        if (minor >= 21) return 21;
        // 1.20.x: 1.20.5+ → Java 21, 1.20.0~1.20.4 → Java 17
        if (minor >= 20) return (rev >= 5) ? 21 : 17;
        if (minor == 18 || minor == 19) return 17;
        if (minor == 17) return 16;
        return 8;  // 1.16.x and below
    }
    
    // Unknown version scheme → conservative
    return 21;
}

void ShadowBackend::killGameProcess() {
    m_launch->killGameProcess();
}

void ShadowBackend::killMinecraft() {
    m_launch->killGameProcess();
}

void ShadowBackend::killGameByPid(qint64 pid) {
    m_launch->killGameByPid(pid);
}

QVariantList ShadowBackend::runningGames() {
    return m_launch->runningGames();
}

int ShadowBackend::getAutoMemory() {
    return m_launch->getAutoMemory();
}

bool ShadowBackend::autoMemoryEnabled() const {
    return m_settings->autoMemoryEnabled();
}

void ShadowBackend::setAutoMemoryEnabled(bool enabled) {
    m_settings->setAutoMemoryEnabled(enabled);
}

int ShadowBackend::versionMemoryMode(const QString& versionId) const {
    return m_settings->versionMemoryMode(versionId);
}

void ShadowBackend::setVersionMemoryMode(const QString& versionId, int mode) {
    m_settings->setVersionMemoryMode(versionId, mode);
}

int ShadowBackend::resolvedMemoryMB(const QString& versionId) {
    int verMode = m_settings->versionMemoryMode(versionId);
    if (verMode == 1) return m_launch->getAutoMemory();
    if (verMode == 2) return m_settings->versionMemoryManualMB(versionId);
    return m_settings->autoMemoryEnabled()
        ? m_launch->getAutoMemory()
        : m_settings->maxMemoryMB();
}

int ShadowBackend::versionMemoryManualMB(const QString& versionId) const {
    return m_settings->versionMemoryManualMB(versionId);
}

void ShadowBackend::setVersionMemoryManualMB(const QString& versionId, int mb) {
    m_settings->setVersionMemoryManualMB(versionId, mb);
}

// ── Per-version Java/launch overrides ──

int ShadowBackend::versionJavaMode(const QString& versionId) const {
    return m_settings->versionJavaMode(versionId);
}
void ShadowBackend::setVersionJavaMode(const QString& versionId, int mode) {
    m_settings->setVersionJavaMode(versionId, mode);
}

int ShadowBackend::versionJvmArgsMode(const QString& versionId) const {
    return m_settings->versionJvmArgsMode(versionId);
}
void ShadowBackend::setVersionJvmArgsMode(const QString& versionId, int mode) {
    m_settings->setVersionJvmArgsMode(versionId, mode);
}
QString ShadowBackend::versionJvmArgs(const QString& versionId) const {
    return m_settings->versionJvmArgs(versionId);
}
void ShadowBackend::setVersionJvmArgs(const QString& versionId, const QString& args) {
    m_settings->setVersionJvmArgs(versionId, args);
    emit versionLaunchSettingsChanged(versionId);
}
QString ShadowBackend::resolvedJvmArgs(const QString& versionId) const {
    if (versionJvmArgsMode(versionId) == 1)
        return versionJvmArgs(versionId);
    return m_app->jvmArgs();
}

int ShadowBackend::versionGameArgsMode(const QString& versionId) const {
    return m_settings->versionGameArgsMode(versionId);
}
void ShadowBackend::setVersionGameArgsMode(const QString& versionId, int mode) {
    m_settings->setVersionGameArgsMode(versionId, mode);
}
QString ShadowBackend::versionGameArgs(const QString& versionId) const {
    return m_settings->versionGameArgs(versionId);
}
void ShadowBackend::setVersionGameArgs(const QString& versionId, const QString& args) {
    m_settings->setVersionGameArgs(versionId, args);
    emit versionLaunchSettingsChanged(versionId);
}
QString ShadowBackend::resolvedGameArgs(const QString& versionId) const {
    if (versionGameArgsMode(versionId) == 1)
        return versionGameArgs(versionId);
    return m_app->gameArgs();
}

int ShadowBackend::versionHighPerfGpuMode(const QString& versionId) const {
    return m_settings->versionHighPerfGpuMode(versionId);
}
void ShadowBackend::setVersionHighPerfGpuMode(const QString& versionId, int mode) {
    m_settings->setVersionHighPerfGpuMode(versionId, mode);
}
bool ShadowBackend::versionHighPerfGpu(const QString& versionId) const {
    return m_settings->versionHighPerfGpu(versionId);
}
void ShadowBackend::setVersionHighPerfGpu(const QString& versionId, bool v) {
    m_settings->setVersionHighPerfGpu(versionId, v);
    emit versionLaunchSettingsChanged(versionId);
}
bool ShadowBackend::resolvedHighPerfGpu(const QString& versionId) const {
    if (versionHighPerfGpuMode(versionId) == 1)
        return versionHighPerfGpu(versionId);
    return m_app->highPerfGpu();
}

// ============================================================
// Q_INVOKABLE methods — Resource
// ============================================================

QVariantList ShadowBackend::getPopularMods(const QString& loader) {
    return m_resource->getPopularMods(loader);
}

QVariantList ShadowBackend::getShaderList() {
    return m_resource->getShaderList();
}

void ShadowBackend::searchMods(const QString& query, const QString& loader) {
    m_resource->searchMods(query, loader);
}

void ShadowBackend::searchModsEx(const QString& query, const QString& loader,
    const QString& category, const QString& gameVersion,
    const QString& environment, const QString& license,
    int offset, int limit, const QString& source) {
    QStringList versions;
    if (!gameVersion.isEmpty())
        versions << gameVersion;
    m_resource->searchModsEx(query, loader, category, versions,
                             environment, license, offset, limit, source);
}

QVariantMap ShadowBackend::getModCategories() {
    return m_resource->getModCategories();
}

QVariantList ShadowBackend::cfCategories(int classId) const
{
    return m_resource ? m_resource->cfCategories(classId) : QVariantList();
}

void ShadowBackend::searchShadersEx(const QString& query, const QStringList& gameVersions,
    const QStringList& categories, const QStringList& performance,
    const QStringList& loader, int offset, int limit, const QString& source) {
    m_resource->searchShadersEx(query, gameVersions, categories, performance, loader, offset, limit, source);
}

// 翻页预取：只预热缓存，不产生聚合信号
void ShadowBackend::prefetchModsEx(const QString& query, const QString& loader,
    const QString& category, const QStringList& gameVersions,
    int offset, int limit, const QString& source) {
    m_resource->prefetchModsEx(query, loader, category, gameVersions, offset, limit, source);
}

void ShadowBackend::prefetchShadersEx(const QString& query, const QStringList& gameVersions,
    const QStringList& categories, int offset, int limit, const QString& source) {
    m_resource->prefetchShadersEx(query, gameVersions, categories, offset, limit, source);
}

void ShadowBackend::prefetchResourcepacks(const QString& query, const QString& gameVersion,
    int offset, const QStringList& categories, const QString& source) {
    m_resource->prefetchResourcepacks(query, gameVersion, categories, offset, 20, source);
}

void ShadowBackend::downloadMod(const QString& slug, const QString& gameVersion, const QString& minecraftDir) {
    m_resource->downloadMod(slug, gameVersion, minecraftDir);
}

void ShadowBackend::downloadShader(const QString& slug, const QString& gameVersion, const QString& minecraftDir) {
    m_resource->downloadShader(slug, gameVersion, minecraftDir);
}

void ShadowBackend::searchResourcepacks(const QString& query, const QString& gameVersion, int offset, const QStringList& categories, const QString& source) {
    m_resource->searchResourcepacks(query, gameVersion, offset, categories, source);
}

void ShadowBackend::downloadResourcepack(const QString& slug, const QString& gameVersion, const QString& minecraftDir) {
    m_resource->downloadResourcepack(slug, gameVersion, minecraftDir);
}

void ShadowBackend::fetchResourcepackVersions(const QStringList& slugs) {
    m_resource->fetchResourcepackVersions(slugs);
}

void ShadowBackend::fetchModVersions(const QStringList& slugs) {
    m_resource->fetchModVersions(slugs);
}

void ShadowBackend::fetchShaderVersions(const QStringList& slugs) {
    m_resource->fetchShaderVersions(slugs);
}

// CurseForge 详情页版本转发（复用 modVersionsPartial 等信号）
void ShadowBackend::fetchModVersionsCf(const QString& modId, const QString& gameVersion, const QString& loader) {
    m_resource->fetchModVersionsCf(modId, gameVersion, loader);
}

void ShadowBackend::fetchShaderVersionsCf(const QString& modId, const QString& gameVersion, const QString& loader) {
    m_resource->fetchShaderVersionsCf(modId, gameVersion, loader);
}

void ShadowBackend::fetchResourcepackVersionsCf(const QString& modId, const QString& gameVersion, const QString& loader) {
    m_resource->fetchResourcepackVersionsCf(modId, gameVersion, loader);
}

void ShadowBackend::resolveCfDependencies(const QString& modId, const QVariantList& deps) {
    m_resource->resolveCfDependencies(modId, deps);
}

void ShadowBackend::fetchCfDependencies(const QString& modId) {
    m_resource->fetchCfDependencies(modId);
}

// ── 整合包：双源搜索 / 详情版本 / 下载→自动导入 ──
void ShadowBackend::searchModpacksEx(const QString& query, const QString& loader,
    const QString& category, const QStringList& gameVersions,
    int offset, int limit, const QString& source) {
    m_resource->searchModpacksEx(query, loader, category, gameVersions, offset, limit, source);
}

void ShadowBackend::fetchModpackVersions(const QString& slug, const QString& gameVersion, const QString& loader) {
    m_resource->fetchModpackVersions(slug, gameVersion, loader);
}

void ShadowBackend::prefetchModpacks(const QString& query, const QString& loader,
    const QString& category, const QStringList& gameVersions,
    int offset, int limit, const QString& source) {
    m_resource->prefetchModpacks(query, loader, category, gameVersions, offset, limit, source);
}

// ── 数据包：双源搜索 / 翻页预取 ──
void ShadowBackend::searchDatapacksEx(const QString& query, const QString& category,
    const QStringList& gameVersions, const QString& sort,
    int offset, int limit, const QString& source) {
    m_resource->searchDatapacksEx(query, category, gameVersions, sort, offset, limit, source);
}

void ShadowBackend::prefetchDatapacks(const QString& query, const QString& category,
    const QStringList& gameVersions, const QString& sort,
    int offset, int limit, const QString& source) {
    m_resource->prefetchDatapacks(query, category, gameVersions, sort, offset, limit, source);
}

int ShadowBackend::downloadModpack(const QString& url, const QString& filename, qint64 size,
                                   const QString& sha1, const QString& versionName, const QString& actualName,
                                   const QString& iconUrl)
{
    if (url.isEmpty() || versionName.isEmpty()) return -1;
    // 单任务限制：下载中或导入中都不允许再下载第二个整合包（QML 已前置弹窗，此处兜底）
    if (modpackBusy()) {
        qCInfo(logApp) << QStringLiteral("[整合包] 已有整合包任务（下载/导入）进行中，拒绝新的下载");
        return -1;
    }
    // 下载目录：{gameDir}/downloads/（不存在则创建）
    QString dlDir = m_gameDir + QStringLiteral("/downloads");
    QDir().mkpath(dlDir);
    QString savePath = dlDir + QLatin1Char('/') + filename;
    if (QFileInfo::exists(savePath)) {
        // 已存在同名文件（上次下载/导入残留）：加时间戳避免覆盖冲突
        savePath = dlDir + QLatin1Char('/')
                 + QFileInfo(filename).completeBaseName()
                 + QStringLiteral("-%1.").arg(QDateTime::currentMSecsSinceEpoch())
                 + QFileInfo(filename).suffix();
    }
    // 卡片标题：整合包：【用户输入名】（【实际名】）——「+」仅为需求示意连接符，不展示给用户
    const QString displayName = tr("整合包：%1（%2）").arg(versionName, actualName.isEmpty() ? filename : actualName);
    const int dlId = m_resource->downloadModFile(url, savePath, displayName, size, sha1);
    if (dlId >= 0) {
        PackDownloadInfo info;
        info.zipPath = savePath;
        info.versionName = versionName;
        info.iconUrl = iconUrl;
        m_packDownloads.insert(dlId, info);
        m_packDownloading = true;
        qCInfo(logApp) << QStringLiteral("[整合包] 下载任务已添加 id=%1 → %2 (版本名=%3)")
            .arg(dlId).arg(savePath, versionName);
    }
    return dlId;
}

bool ShadowBackend::modpackBusy() const
{
    if (m_packDownloading) return true;
    auto* imp = qobject_cast<ModpackImporter*>(m_modpackImporter);
    return imp && imp->isBusy();
}

// ── Mod file download proxy ──
int ShadowBackend::downloadModFile(const QString& url, const QString& savePath,
                                    const QString& displayName, qint64 expectedSize,
                                    const QString& sha1, qint64 receivedOffset, int resumeId) {
    return m_resource->downloadModFile(url, savePath, displayName, expectedSize, sha1, receivedOffset, resumeId);
}
void ShadowBackend::cancelModFileDownload(int downloadId) {
    if (m_packDownloads.contains(downloadId)) {
        m_packDownloads.remove(downloadId);
        m_packDownloading = false;   // 取消整合包下载 → 释放单任务占位
    }
    m_resource->cancelModFileDownload(downloadId);
}

void ShadowBackend::pauseModFileDownload(int downloadId) {
    m_resource->pauseModFileDownload(downloadId);
}

void ShadowBackend::resumeModFileDownload(int downloadId) {
    m_resource->resumeModFileDownload(downloadId);
}

void ShadowBackend::retryModFileDownload(int downloadId) {
    m_resource->retryModFileDownload(downloadId);
}

// ═══ Wardrobe (衣帽间) ═══

void ShadowBackend::initCapeCache() {
    if (!m_capeCache.isEmpty()) return;

    struct CapeEntry { QString name; QString file; };
    QList<CapeEntry> capes = {
        {QStringLiteral("15th Anniversary"), QStringLiteral("15th_anniversary.png")},
        {QStringLiteral("Builder"), QStringLiteral("builder.png")},
        {QStringLiteral("Cherry Blossom"), QStringLiteral("cherry_blossom.png")},
        {QStringLiteral("Common"), QStringLiteral("common.png")},
        {QStringLiteral("Copper"), QStringLiteral("copper.png")},
        {QStringLiteral("Follower's"), QStringLiteral("followers.png")},
        {QStringLiteral("Founder's"), QStringLiteral("founders.png")},
        {QStringLiteral("Home"), QStringLiteral("home.png")},
        {QStringLiteral("MCC 15th Year"), QStringLiteral("mcc_15th_year.png")},
        {QStringLiteral("Menace"), QStringLiteral("menace.png")},
        {QStringLiteral("Migrator"), QStringLiteral("migrator.png")},
        {QStringLiteral("MineCon 2011"), QStringLiteral("minecon_2011.png")},
        {QStringLiteral("MineCon 2012"), QStringLiteral("minecon_2012.png")},
        {QStringLiteral("MineCon 2013"), QStringLiteral("minecon_2013.png")},
        {QStringLiteral("MineCon 2015"), QStringLiteral("minecon_2015.png")},
        {QStringLiteral("MineCon 2016"), QStringLiteral("minecon_2016.png")},
        {QStringLiteral("MC Experience"), QStringLiteral("minecraft_experience.png")},
        {QStringLiteral("Mojang"), QStringLiteral("mojang.png")},
        {QStringLiteral("Mojang Studios"), QStringLiteral("mojang_studios.png")},
        {QStringLiteral("Pan"), QStringLiteral("pan.png")},
        {QStringLiteral("Purple Heart"), QStringLiteral("purple_heart.png")},
        {QStringLiteral("Mapmaker"), QStringLiteral("realms_mapmaker.png")},
        {QStringLiteral("Translator CN"), QStringLiteral("translator_chinese.png")},
        {QStringLiteral("Vanilla"), QStringLiteral("vanilla.png")},
        {QStringLiteral("Yearn"), QStringLiteral("yearn.png")},
        {QStringLiteral("Zombie Horse"), QStringLiteral("zombie_horse.png")},
    };

    for (const auto& entry : capes) {
        QVariantMap m;
        m[QStringLiteral("name")] = entry.name;
        m[QStringLiteral("preview")] = QStringLiteral("qrc:/qt/qml/ShadowLauncher/resources/capes/") + entry.file;
        m[QStringLiteral("url")] = entry.name;
        m_capeCache.append(m);
    }
}

QVariantList ShadowBackend::availableCapes() const {
    const_cast<ShadowBackend*>(this)->initCapeCache();
    return m_capeCache;
}

QString ShadowBackend::loginType() const {
    return m_lastLoginMode == 0 ? QStringLiteral("microsoft") : QStringLiteral("offline");
}

void ShadowBackend::browseSkin() {
    QString file = QFileDialog::getOpenFileName(nullptr, tr("选择皮肤图片"), QString(),
        tr("PNG 图片 (*.png)"));
    if (file.isEmpty()) return;

    // Validate PNG dimensions (64x64 or 64x32)
    QImage img(file);
    if (img.isNull()) {
        emit wardrobeError(tr("无法读取图片"));
        return;
    }
    int w = img.width();
    int h = img.height();
    bool valid = (w == 64 && h == 64) || (w == 64 && h == 32);
    if (!valid) {
        emit wardrobeError(tr("皮肤尺寸必须为 64x64 或 64x32，当前为 %1x%2").arg(w).arg(h));
        return;
    }

    m_selectedSkinPath = file;
    emit logMessage(tr("已选择皮肤: %1 (%2x%3)").arg(file).arg(w).arg(h));
    emit skinChanged();
}

void ShadowBackend::uploadSkin(const QString& skinPath, int modelType) {
    if (skinPath.isEmpty()) return;

    if (m_lastLoginMode != 0 || !m_account || m_account->mcToken().isEmpty()) {
        // Offline mode: just save locally
        qCInfo(logApp) << QStringLiteral("衣帽间: 离线模式，皮肤保存到本地");
        emit logMessage(tr("离线模式：皮肤已保存到本地"));
        emit wardrobeBusyChanged();
        return;
    }

    m_wardrobeBusy = true;
    emit wardrobeBusyChanged();

    // Read PNG and convert to base64 data URL
    QFile f(skinPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit wardrobeError(tr("无法读取皮肤文件"));
        m_wardrobeBusy = false;
        emit wardrobeBusyChanged();
        return;
    }
    QByteArray pngData = f.readAll();
    f.close();
    QString base64 = QString::fromLatin1(pngData.toBase64());
    QString dataUrl = QStringLiteral("data:image/png;base64,") + base64;

    const char* variant = (modelType == 2) ? "slim" : "classic";  // 0=auto,1=Steve,2=Alex(slim)

    QJsonObject body;
    body[QStringLiteral("variant")] = QString::fromLatin1(variant);
    body[QStringLiteral("url")] = dataUrl;
    QJsonDocument doc(body);
    QByteArray json = doc.toJson(QJsonDocument::Compact);

    QNetworkRequest req(QUrl(QStringLiteral("https://api.minecraftservices.com/minecraft/profile/skins")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Authorization", ("Bearer " + m_account->mcToken()).toUtf8());

    auto* net = new QNetworkAccessManager(this);
    QNetworkReply* reply = net->post(req, json);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_wardrobeBusy = false;
        emit wardrobeBusyChanged();

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 200 || status == 204) {
            qCInfo(logApp) << QStringLiteral("衣帽间: 皮肤上传成功");
            emit logMessage(tr("皮肤已同步到 Mojang 服务器"));
        } else {
            QByteArray body = reply->readAll();
            qCWarning(logApp) << QStringLiteral("衣帽间: 皮肤上传失败 status=%1 body=%2").arg(status).arg(QString::fromUtf8(body));
            emit wardrobeError(tr("上传失败 (HTTP %1)").arg(status));
        }
    });
}

void ShadowBackend::saveWardrobeSettings(const QString& skinPath, const QString& capeId, int modelType) {
    qCInfo(logApp) << QStringLiteral("衣帽间保存 skin=%1 cape=%2 model=%3").arg(skinPath, capeId).arg(modelType);

    if (m_lastLoginMode == 0 && m_account && !m_account->mcToken().isEmpty()) {
        // Online: upload to Mojang
        if (!skinPath.isEmpty()) {
            uploadSkin(skinPath, modelType);
        }
        if (!capeId.isEmpty()) {
            emit logMessage(tr("披风选择: %1 (需在 Minecraft 官网设置)").arg(capeId));
        }
    } else {
        // Offline: copy skin to launcher data dir
        if (!skinPath.isEmpty()) {
            QString skinDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/skins";
            QDir().mkpath(skinDir);
            QString dest = skinDir + "/custom_skin.png";
            if (QFile::copy(skinPath, dest)) {
                emit logMessage(tr("离线皮肤已保存: %1").arg(dest));
            } else {
                emit wardrobeError(tr("皮肤保存失败"));
            }
        }
        emit logMessage(tr("离线模式: 皮肤将在下次启动时生效"));
    }
}

// ────────────────────────────────────────────────────────────
// Save Skin To File (user-chosen destination via file dialog)
// ────────────────────────────────────────────────────────────

void ShadowBackend::saveSkinToFile()
{
    if (!m_account) {
        emit wardrobeError(tr("未就绪"));
        return;
    }

    // Online mode: fetch fresh full skin from Mojang API
    if (m_account->isOnline()) {
        qCInfo(logApp) << QStringLiteral("在线保存皮肤: 正在从Mojang获取最新皮肤");
        qCInfo(logApp) << QStringLiteral("正在获取最新皮肤...");

        QNetworkRequest req(QUrl(QStringLiteral("https://api.minecraftservices.com/minecraft/profile")));
        req.setRawHeader("Authorization",
            QStringLiteral("Bearer %1").arg(m_account->mcToken()).toUtf8());

        QNetworkReply* profileReply = HttpClient::instance().getRaw(req);
        connect(profileReply, &QNetworkReply::finished, this, [this, profileReply]() {
            profileReply->deleteLater();

            if (profileReply->error() != QNetworkReply::NoError) {
                emit wardrobeError(tr("获取皮肤信息失败: %1").arg(profileReply->errorString()));
                return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(profileReply->readAll());
            QJsonArray skins = doc.object()[QStringLiteral("skins")].toArray();

            QString skinUrl;
            for (const QJsonValue& sv : skins) {
                QJsonObject s = sv.toObject();
                if (s[QStringLiteral("state")].toString() == QStringLiteral("ACTIVE")) {
                    skinUrl = s[QStringLiteral("url")].toString();
                    break;
                }
            }

            if (skinUrl.isEmpty()) {
                emit wardrobeError(tr("未找到活跃皮肤"));
                return;
            }

            qCInfo(logApp) << QStringLiteral("正在下载皮肤纹理: ") << skinUrl;
            QNetworkRequest skinReq(skinUrl);
            QNetworkReply* skinReply = HttpClient::instance().getRaw(skinReq);
            connect(skinReply, &QNetworkReply::finished, this, [this, skinReply]() {
                skinReply->deleteLater();

                if (skinReply->error() != QNetworkReply::NoError) {
                    emit wardrobeError(tr("皮肤纹理下载失败: %1").arg(skinReply->errorString()));
                    return;
                }

                QByteArray skinData = skinReply->readAll();

                QString dest = QFileDialog::getSaveFileName(nullptr, tr("保存皮肤文件"),
                    QStringLiteral("skin.png"), tr("PNG 图片 (*.png)"));
                if (dest.isEmpty()) return;

                QFile file(dest);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(skinData);
                    file.close();
                    qCInfo(logApp) << QStringLiteral("皮肤已在线保存到: ") << dest;
                    emit wardrobeError(tr("皮肤已保存到: %1").arg(dest));
                } else {
                    emit wardrobeError(tr("保存失败: %1").arg(dest));
                }
            });
        });
        return;
    }

    // Offline mode: save local cached skin (fix file:/// prefix bug)
    QString srcPath;
    srcPath = m_account->offlineSkinPath();
    if (srcPath.isEmpty())
        srcPath = m_selectedSkinPath;

    // Resolve file:/// prefix before any QFile operations
    if (srcPath.startsWith(QStringLiteral("file:///")))
        srcPath = srcPath.mid(8);

    if (srcPath.isEmpty() || !QFile::exists(srcPath)) {
        emit wardrobeError(tr("没有可保存的皮肤"));
        return;
    }

    QString dest = QFileDialog::getSaveFileName(nullptr, tr("保存皮肤文件"),
        QStringLiteral("skin.png"), tr("PNG 图片 (*.png)"));
    if (dest.isEmpty()) return;

    if (QFile::copy(srcPath, dest)) {
        emit wardrobeError(tr("皮肤已保存到: %1").arg(dest));
    } else {
        // Try reading + writing if copy fails (e.g. across drives)
        QFile inFile(srcPath);
        if (inFile.open(QIODevice::ReadOnly)) {
            QByteArray data = inFile.readAll();
            inFile.close();
            QFile outFile(dest);
            if (outFile.open(QIODevice::WriteOnly)) {
                outFile.write(data);
                outFile.close();
                emit wardrobeError(tr("皮肤已保存到: %1").arg(dest));
                return;
            }
        }
        emit wardrobeError(tr("保存失败: %1").arg(dest));
    }
}

// ── Icon cache: async download webp → ffmpeg convert to PNG → cache locally ──
// Qt 6.5.3 on this machine lacks qwebp.dll plugin, so we pre-convert webp to PNG
void ShadowBackend::cacheIconAsync(const QString& webpUrl) {
    if (webpUrl.isEmpty()) return;

    qCDebug(logApp) << "[ICON] cacheIconAsync called for:" << webpUrl.left(100);

    // Cache dir: <appdir>/icons/
    static QString s_cacheDir;
    if (s_cacheDir.isEmpty()) {
        s_cacheDir = QCoreApplication::applicationDirPath() + QStringLiteral("/icons/");
        QDir().mkpath(s_cacheDir);
        qCDebug(logApp) << "[ICON] cache dir:" << s_cacheDir;
    }

    // Derive cache filename from URL hash
    QByteArray hash = QCryptographicHash::hash(webpUrl.toUtf8(), QCryptographicHash::Sha1).toHex().left(16);
    QString pngPath = s_cacheDir + QString::fromLatin1(hash) + QStringLiteral(".png");

    // Return cached version immediately if exists
    if (QFile::exists(pngPath)) {
        qCDebug(logApp) << "[ICON] cached exists:" << pngPath;
        emit iconCached(webpUrl, QUrl::fromLocalFile(pngPath).toString());
        return;
    }

    qCDebug(logApp) << "[ICON] downloading:" << webpUrl.left(120);

    // Async download + convert
    auto* mgr = new QNetworkAccessManager(this);
    QUrl qurl(webpUrl);
    QNetworkRequest req(qurl);
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = mgr->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, mgr, webpUrl, pngPath]() {
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(logApp) << QStringLiteral("[图标] 下载失败 url=%1 错误=%2").arg(webpUrl.left(100), reply->errorString());
            reply->deleteLater();
            mgr->deleteLater();
            emit iconCached(webpUrl, {});
            return;
        }

        // Decode webp → QImage → save PNG (libwebp, zero subprocess overhead)
        QByteArray webpData = reply->readAll();
        reply->deleteLater();
        mgr->deleteLater();

        QImage img;
        int w = 0, h = 0;
        uint8_t* rgba = WebPDecodeRGBA(
            reinterpret_cast<const uint8_t*>(webpData.constData()),
            webpData.size(), &w, &h);

        if (rgba && w > 0 && h > 0) {
            // WebP decoded successfully
            img = QImage(rgba, w, h, QImage::Format_RGBA8888,
                         [](void* p) { WebPFree(p); }, rgba);
        } else {
            // Not webp? Try loading directly (e.g., already PNG)
            WebPFree(rgba);  // safe to free null
            img = QImage::fromData(webpData);
            if (img.isNull()) {
                qCWarning(logApp) << QStringLiteral("[图标] 解码失败 url=%1").arg(webpUrl.left(100));
                emit iconCached(webpUrl, {});
                return;
            }
        }

        // Save PNG to disk cache for future instant loads
        if (img.save(pngPath, "PNG")) {
            qCDebug(logApp) << "[ICON] cached (libwebp):" << webpUrl.left(100) << "→" << pngPath
                     << "(" << img.width() << "x" << img.height() << ")";
            emit iconCached(webpUrl, QUrl::fromLocalFile(pngPath).toString());
        } else {
            qCWarning(logApp) << QStringLiteral("[图标] PNG保存失败 路径=%1").arg(pngPath);
            emit iconCached(webpUrl, {});
        }
    });
}

QString ShadowBackend::cachedIconPath(const QString& webpUrl) const {
    if (webpUrl.isEmpty()) return {};
    QByteArray hash = QCryptographicHash::hash(webpUrl.toUtf8(), QCryptographicHash::Sha1).toHex().left(16);
    QString pngPath = QCoreApplication::applicationDirPath() + QStringLiteral("/icons/") 
                    + QString::fromLatin1(hash) + QStringLiteral(".png");
    if (QFile::exists(pngPath)) {
        return QUrl::fromLocalFile(pngPath).toString();
    }
    return {};
}

// ============================================================
// fetchResourcepackVersions
// Q_INVOKABLE methods — App
// ============================================================

void ShadowBackend::setTheme(const QString& theme) {
    m_app->setTheme(theme);
}


// ============================================================
// Q_INVOKABLE methods — Version management
// ============================================================

void ShadowBackend::verifyVersion(const QString& versionId) {
    m_version->verifyVersion(versionId);
}

void ShadowBackend::cleanCorruptVersion(const QString& versionId) {
    m_version->cleanCorruptVersion(versionId);
}

bool ShadowBackend::renameVersion(const QString& oldId, const QString& newId) {
    return m_version->renameVersion(oldId, newId);
}

bool ShadowBackend::cloneVersion(const QString& sourceId, const QString& newId) {
    return m_version->cloneVersion(sourceId, newId);
}

bool ShadowBackend::cloneVersion(const QString& sourceId) {
    // Auto-generate name: "1.21.6-fabric-0.19.3 (\u526f\u672c)"
    QString newId = sourceId + QString::fromUtf8(" (\u526f\u672c)");
    return m_version->cloneVersion(sourceId, newId);
}

bool ShadowBackend::renameVersion(const QString& oldId) {
    // Single-param: called from QML with just oldId — QML shows rename dialog
    qDebug() << "[renameVersion] single-param stub called for" << oldId << "– QML should show dialog";
    return false;
}

void ShadowBackend::migrateVersion(const QString& versionId) {
    qDebug() << "[migrateVersion]" << versionId;
    m_settings->migrateVersionToIsolated(versionId);
}

QString ShadowBackend::copyVersionPath(const QString& versionId) {
    return m_version->copyVersionPath(versionId);
}

void ShadowBackend::repairVersion(const QString& versionId) {
    m_version->repairVersion(versionId);
}

QString ShadowBackend::verifyResultText() const { return m_verifyResultText; }
bool ShadowBackend::verifyResultOk() const { return m_verifyResultOk; }
void ShadowBackend::setVerifyResult(const QString& text, bool ok) {
    m_verifyResultText = text;
    m_verifyResultOk = ok;
    emit verifyResultTextChanged();
}

void ShadowBackend::cancelVerify() {
    if (m_version) m_version->cancelVerify();
}

void ShadowBackend::openVerifyReport() {
    if (!m_verifyReportPath.isEmpty() && QFile::exists(m_verifyReportPath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_verifyReportPath));
    }
}

// ============================================================
// Q_INVOKABLE methods — Check
// ============================================================

QVariantMap ShadowBackend::checkAll(const QString& versionId) {
    return m_check->checkAll(versionId,
                             m_settings->javaPath(),
                             m_settings->maxMemoryMB(),
                             m_app->gameDir());
}

// ============================================================
// Q_INVOKABLE methods — Clipboard
// ============================================================

void ShadowBackend::copyToClipboard(const QString& text)
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(text);
    }
}

// ============================================================
// Mod Loader version queries (BMCLAPI)
// ============================================================

static void queryModLoaderApi(ShadowBackend* self, const QString& url,
                               const QString& loaderName,
                               std::function<QVariantList(const QByteArray&)> parseFn,
                               std::function<void(const QVariantList&)> emitFn) {
    // Fire request immediately (no throttling needed)
    QTimer::singleShot(0, self, [=]() {
        auto* mgr = new QNetworkAccessManager(self);
    QUrl qurl(url);
    QNetworkRequest req(qurl);
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = mgr->get(req);
    qCInfo(logApp) << QStringLiteral("[加载器] 查询版本列表 加载器=%1 url=%2").arg(loaderName, url);

    // Track for cancellation
    self->m_modLoaderReplies.append(reply);

    QObject::connect(reply, &QNetworkReply::finished, self, [self, reply, mgr, loaderName, parseFn, emitFn]() {
        // Remove from tracking
        self->m_modLoaderReplies.removeAll(reply);

        if (self->m_modLoaderQueriesCancelled) {
            qCInfo(logApp) << QStringLiteral("[加载器] 查询已取消 加载器=%1").arg(loaderName);
            reply->deleteLater();
            mgr->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(logApp) << QStringLiteral("[加载器] 查询失败 加载器=%1 错误=%2").arg(loaderName, reply->errorString());
            reply->deleteLater();
            mgr->deleteLater();
            emitFn({});
            return;
        }
        QByteArray data = reply->readAll();
        reply->deleteLater();
        mgr->deleteLater();
        qCInfo(logApp) << QStringLiteral("[加载器] 获取版本数据 加载器=%1 大小=%2字节").arg(loaderName).arg(data.size());
        QVariantList list = parseFn(data);
        qCInfo(logApp) << QStringLiteral("[加载器] 解析版本完成 加载器=%1 版本数=%2").arg(loaderName).arg(list.size());
        emitFn(list);
    });
    });  // QTimer::singleShot
}

// ── Forge 官方 HTML 版本列表解析 ──
// URL: https://files.minecraftforge.net/maven/net/minecraftforge/forge/index_{mcVer}.html
// 注意：- 需要替换为 _（兼容 1.7.10-pre4 等版本）
static QVariantList parseForgeOfficialVersions(const QByteArray& html, const QString& filterMc) {
    QString text = QString::fromUtf8(html);
    if (text.length() < 1000) return {};

    QVariantList result;

    // Split by <td class="download-version" to get individual version blocks
    QStringList versionBlocks = text.split(QStringLiteral("<td class=\"download-version"));
    // First element is before the first version block
    for (int blockIdx = 1; blockIdx < versionBlocks.size(); ++blockIdx) {
        const QString& block = versionBlocks[blockIdx];

        // Extract version name: e.g. "50.1.9"
        QRegularExpression nameRe(QStringLiteral("[0-9]+(?:\\.[0-9]+)+"));
        QString versionName;
        QRegularExpressionMatch nameMatch = nameRe.match(block);
        if (nameMatch.hasMatch())
            versionName = nameMatch.captured();
        if (versionName.isEmpty()) continue;

        bool isRecommended = block.contains(QStringLiteral("fa promo-recommended"));

        // Extract branch from URL: e.g. "forge-50.1.9-1.10.0-installer.jar" → "1.10.0"
        QString branch;
        QRegularExpression branchRe(QStringLiteral("-") + QRegularExpression::escape(versionName) + QStringLiteral("-([^\"]+?)(?=-[a-z]+\\.[a-z]{3})"));
        QRegularExpressionMatch branchMatch = branchRe.match(block);
        if (branchMatch.hasMatch())
            branch = branchMatch.captured(1);
        // 已知有问题的特殊版本
        if (versionName == QStringLiteral("11.15.1.2318") ||
            versionName == QStringLiteral("11.15.1.1902") ||
            versionName == QStringLiteral("11.15.1.1890"))
            branch = QStringLiteral("1.8.9");
        if (branch.isEmpty() && filterMc == QStringLiteral("1.7.10")) {
            // For 1.7.10, branch is needed when version's fourth segment >= 1300
            QStringList vParts = versionName.split(QLatin1Char('.'));
            if (vParts.size() >= 4 && vParts[3].toInt() >= 1300)
                branch = QStringLiteral("1.7.10");
        }

        // Extract release time: e.g. "2021-02-15 03:24:02"
        QRegularExpression timeRe(QStringLiteral("(?<=download-time\" title=\")[^\"]+"));
        QString releaseTime;
        QRegularExpressionMatch timeMatch = timeRe.match(block);
        if (timeMatch.hasMatch()) {
            QString orig = timeMatch.captured();
            // Convert "2021-02-15 03:24:02" → "2021-02-15"
            releaseTime = orig.left(10);
        }

        // Determine category and extract hash
        QString category;
        QString fileHash;
        if (block.contains(QStringLiteral("classifier-installer\""))) {
            // installer.jar
            QString sub = block.mid(block.indexOf(QStringLiteral("installer.jar")));
            QRegularExpression hashRe(QStringLiteral("(?<=MD5:</strong> )[^<]+"));
            QRegularExpressionMatch hashMatch = hashRe.match(sub);
            if (hashMatch.hasMatch())
                fileHash = hashMatch.captured().trimmed();
            category = QStringLiteral("installer");
        } else if (block.contains(QStringLiteral("classifier-universal\""))) {
            QString sub = block.mid(block.indexOf(QStringLiteral("universal.zip")));
            QRegularExpression hashRe(QStringLiteral("(?<=MD5:</strong> )[^<]+"));
            QRegularExpressionMatch hashMatch = hashRe.match(sub);
            if (hashMatch.hasMatch())
                fileHash = hashMatch.captured().trimmed();
            category = QStringLiteral("universal");
        } else if (block.contains(QStringLiteral("client.zip"))) {
            QString sub = block.mid(block.indexOf(QStringLiteral("client.zip")));
            QRegularExpression hashRe(QStringLiteral("(?<=MD5:</strong> )[^<]+"));
            QRegularExpressionMatch hashMatch = hashRe.match(sub);
            if (hashMatch.hasMatch())
                fileHash = hashMatch.captured().trimmed();
            category = QStringLiteral("client");
        } else {
            // No downloadable file for this entry
            continue;
        }

        // 对齐 主流启动器：只保留 installer 类别（universal/client 无法自动安装）2026-08-07
        if (category != QStringLiteral("installer")) continue;

        QVariantMap m;
        m[QStringLiteral("version")] = versionName;
        m[QStringLiteral("type")] = isRecommended ? QStringLiteral("recommended") : QStringLiteral("release");
        m[QStringLiteral("date")] = releaseTime;
        m[QStringLiteral("branch")] = branch;
        m[QStringLiteral("category")] = category;
        m[QStringLiteral("hash")] = fileHash;
        result.append(m);
    }

    return result;
}

// ── Forge BMCLAPI JSON 解析（抽取为独立函数供双源复用）──
static QVariantList parseForgeBmclapiVersions(const QByteArray& data, const QString& mcVersion,
                                                std::function<void(const QString&, const QString&, const QString&)> cacheBranchFn) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) return {};
    QJsonArray arr = doc.array();
    QVariantList list;
    for (const QJsonValue& v : arr) {
        QJsonObject obj = v.toObject();
        QString ver = obj.value(QStringLiteral("version")).toString();
        if (ver.isEmpty()) continue;
        QString date = obj.value(QStringLiteral("modified")).toString().left(10);
        QString installerSha1;
        bool hasInstaller = false;
        if (obj.contains(QStringLiteral("files"))) {
            for (const QJsonValue& fv : obj[QStringLiteral("files")].toArray()) {
                QJsonObject f = fv.toObject();
                const QString cat = f.value(QStringLiteral("category")).toString();
                if (cat == QStringLiteral("installer")) {
                    installerSha1 = f.value(QStringLiteral("hash")).toString();
                    hasInstaller = true;
                    break;
                }
            }
        }
        // 对齐 主流启动器（PageDownloadInstall L724/757）：只保留 installer 类别，
        // universal/client（1.6.1 及更早）无法自动安装 → 直接过滤（2026-08-07）
        if (!hasInstaller) continue;
        QString branch = obj.value(QStringLiteral("branch")).toString();
        if (!branch.isEmpty() && cacheBranchFn)
            cacheBranchFn(mcVersion, ver, branch);
        QVariantMap m;
        m[QStringLiteral("version")] = ver;
        m[QStringLiteral("type")] = QStringLiteral("release");
        m[QStringLiteral("date")] = date;
        if (!installerSha1.isEmpty())
            m[QStringLiteral("installerSha1")] = installerSha1;
        m[QStringLiteral("branch")] = branch;
        list.append(m);
    }
    return list;
}

void ShadowBackend::queryForgeVersions(const QString& mcVersion) {
    m_modLoaderQueriesCancelled = false;

    // 1.5 及更早（1.0~1.5.x）：老式 Forge 安装格式，FML 4.x 运行时依赖 fmllibs
    // （argo-2.25/guava-12.0.1/asm-all-4.0/bcprov-jdk15on-147 等），官方源
    // files.minecraftforge.net/fmllibs/ 已死（404 实测）→ 装完也启动不了。
    // 从根源杜绝：列表层直接过滤（对齐 主流启动器，2026-08-07）
    static const QRegularExpression legacyForgeVer(QStringLiteral(R"(^1\.[0-5](?:\.|$))"));
    if (legacyForgeVer.match(mcVersion).hasMatch()) {
        qCInfo(logApp) << QStringLiteral("[加载器] Forge 版本列表过滤: MC %1（1.5 及更早不支持自动安装）").arg(mcVersion);
        emit forgeVersionsReady(QVariantList());
        return;
    }

    struct SourceReq { bool done = false; QByteArray data; };
    struct DualState { SourceReq bmcl; SourceReq official; bool parsedBmcl = false; bool parsedOfficial = false; QVariantList bmclResult; QVariantList officialResult; bool emitted = false; };
    auto state = std::make_shared<DualState>();

    // Capture this for the BMCLAPI parser lambda (ShadowBackend member)
    auto cacheBranch = [this](const QString& mc, const QString& ver, const QString& branch) {
        cacheForgeInstallerBranch(mc, ver, branch);
    };

    auto checkSourceComplete = [this, state, mcVersion, cacheBranch](bool isBmcl) {
        if (state->emitted) return;
        SourceReq& src = isBmcl ? state->bmcl : state->official;
        if (!src.done) return;

        QVariantList list;
        if (isBmcl) {
            list = parseForgeBmclapiVersions(src.data, mcVersion, cacheBranch);
            state->bmclResult = list;
            state->parsedBmcl = true;
        } else {
            list = parseForgeOfficialVersions(src.data, mcVersion);
            state->officialResult = list;
            state->parsedOfficial = true;
        }

        if (!list.isEmpty()) {
            state->emitted = true;
            // Cache branch info from official source results too
            if (!isBmcl) {
                for (const QVariant& v : list) {
                    QVariantMap m = v.toMap();
                    QString ver = m.value(QStringLiteral("version")).toString();
                    QString branch = m.value(QStringLiteral("branch")).toString();
                    if (!ver.isEmpty() && !branch.isEmpty())
                        cacheForgeInstallerBranch(mcVersion, ver, branch);
                }
            }
            emit forgeVersionsReady(list);
            return;
        }
        bool otherParsed = isBmcl ? state->parsedOfficial : state->parsedBmcl;
        if (otherParsed) {
            state->emitted = true;
            const QVariantList& otherResult = isBmcl ? state->officialResult : state->bmclResult;
            emit forgeVersionsReady(otherResult.isEmpty() ? QVariantList() : otherResult);
        }
    };

    auto fireRequest = [this, state, checkSourceComplete, &mcVersion](bool isBmcl) {
        // Official URL: replace - with _ for versions like 1.7.10-pre4
        QString mcSanitized = mcVersion;
        mcSanitized.replace(QLatin1Char('-'), QLatin1Char('_'));

        QString url = isBmcl
            ? QStringLiteral("https://bmclapi2.bangbang93.com/forge/minecraft/") + mcVersion
            : QStringLiteral("https://files.minecraftforge.net/maven/net/minecraftforge/forge/index_") + mcSanitized + QStringLiteral(".html");

        auto* mgr = new QNetworkAccessManager(this);
        QNetworkRequest req{QUrl(url)};
        req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply* reply = mgr->get(req);
        qCInfo(logApp) << QStringLiteral("[加载器] 查询版本列表 加载器=Forge url=%1").arg(url);
        m_modLoaderReplies.append(reply);

        QObject::connect(reply, &QNetworkReply::finished, this,
            [this, reply, mgr, isBmcl, state, checkSourceComplete]() {
                m_modLoaderReplies.removeAll(reply);
                if (m_modLoaderQueriesCancelled) {
                    qCInfo(logApp) << QStringLiteral("[加载器] 查询已取消 加载器=Forge");
                    reply->deleteLater(); mgr->deleteLater(); return;
                }
                if (reply->error() != QNetworkReply::NoError) {
                    qCWarning(logApp) << QStringLiteral("[加载器] 查询失败 加载器=Forge 源=%1 错误=%2")
                        .arg(isBmcl ? QStringLiteral("BMCLAPI") : QStringLiteral("官方"))
                        .arg(reply->errorString());
                } else {
                    SourceReq& src = isBmcl ? state->bmcl : state->official;
                    src.data = reply->readAll();
                    qCInfo(logApp) << QStringLiteral("[加载器] 获取版本数据 加载器=Forge 源=%1 大小=%2字节")
                        .arg(isBmcl ? QStringLiteral("BMCLAPI") : QStringLiteral("官方"))
                        .arg(src.data.size());
                }
                SourceReq& src = isBmcl ? state->bmcl : state->official;
                src.done = true;
                reply->deleteLater();
                mgr->deleteLater();
                checkSourceComplete(isBmcl);
            });
    };

    fireRequest(true);
    fireRequest(false);
}

void ShadowBackend::queryFabricVersions(const QString& mcVersion) {
    m_modLoaderQueriesCancelled = false;

    struct SourceReq { bool done = false; QByteArray data; };
    struct DualState { SourceReq bmcl; SourceReq official; bool parsedBmcl = false; bool parsedOfficial = false; QVariantList bmclResult; QVariantList officialResult; bool emitted = false; };
    auto state = std::make_shared<DualState>();

    auto checkSourceComplete = [this, state](bool isBmcl) {
        if (state->emitted) return;
        SourceReq& src = isBmcl ? state->bmcl : state->official;
        if (!src.done) return;

        // Parse Fabric versions (same JSON format for both BMCLAPI and official)
        QJsonDocument doc = QJsonDocument::fromJson(src.data);
        QJsonArray arr;
        if (doc.isArray()) {
            arr = doc.array();
        } else if (doc.isObject()) {
            arr = doc.object().value(QStringLiteral("versions")).toArray();
        }

        QVariantList list;
        for (const QJsonValue& v : arr) {
            QJsonObject obj = v.toObject();
            QJsonObject loader = obj.value(QStringLiteral("loader")).toObject();
            QString ver = loader.value(QStringLiteral("version")).toString();
            if (ver.isEmpty()) continue;
            bool stable = loader.value(QStringLiteral("stable")).toBool(true);
            QVariantMap m;
            m[QStringLiteral("version")] = ver;
            m[QStringLiteral("type")] = stable ? QStringLiteral("release") : QStringLiteral("beta");
            m[QStringLiteral("date")] = QString();
            list.append(m);
        }

        if (isBmcl) {
            state->bmclResult = list;
            state->parsedBmcl = true;
        } else {
            state->officialResult = list;
            state->parsedOfficial = true;
        }

        if (!list.isEmpty()) {
            state->emitted = true;
            emit fabricVersionsReady(list);
            return;
        }
        // This source returned empty; check if the other source is ready
        bool otherParsed = isBmcl ? state->parsedOfficial : state->parsedBmcl;
        if (otherParsed) {
            state->emitted = true;
            const QVariantList& otherResult = isBmcl ? state->officialResult : state->bmclResult;
            emit fabricVersionsReady(otherResult.isEmpty() ? QVariantList() : otherResult);
        }
    };

    auto fireRequest = [this, state, checkSourceComplete, &mcVersion](bool isBmcl) {
        QString url = isBmcl
            ? QStringLiteral("https://bmclapi2.bangbang93.com/fabric-meta/v2/versions/loader/") + mcVersion
            : QStringLiteral("https://meta.fabricmc.net/v2/versions/loader/") + mcVersion;

        auto* mgr = new QNetworkAccessManager(this);
        QNetworkRequest req{QUrl(url)};
        req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply* reply = mgr->get(req);
        qCInfo(logApp) << QStringLiteral("[加载器] 查询版本列表 加载器=Fabric url=%1").arg(url);
        m_modLoaderReplies.append(reply);

        QObject::connect(reply, &QNetworkReply::finished, this,
            [this, reply, mgr, isBmcl, state, checkSourceComplete]() {
                m_modLoaderReplies.removeAll(reply);
                if (m_modLoaderQueriesCancelled) {
                    qCInfo(logApp) << QStringLiteral("[加载器] 查询已取消 加载器=Fabric");
                    reply->deleteLater(); mgr->deleteLater(); return;
                }
                if (reply->error() != QNetworkReply::NoError) {
                    qCWarning(logApp) << QStringLiteral("[加载器] 查询失败 加载器=Fabric 源=%1 错误=%2")
                        .arg(isBmcl ? QStringLiteral("BMCLAPI") : QStringLiteral("官方"))
                        .arg(reply->errorString());
                } else {
                    SourceReq& src = isBmcl ? state->bmcl : state->official;
                    src.data = reply->readAll();
                    qCInfo(logApp) << QStringLiteral("[加载器] 获取版本数据 加载器=Fabric 源=%1 大小=%2字节")
                        .arg(isBmcl ? QStringLiteral("BMCLAPI") : QStringLiteral("官方"))
                        .arg(src.data.size());
                }
                SourceReq& src = isBmcl ? state->bmcl : state->official;
                src.done = true;
                reply->deleteLater();
                mgr->deleteLater();
                checkSourceComplete(isBmcl);
            });
    };

    fireRequest(true);
    fireRequest(false);
}


static QStringList parseNeoForgeApiNames(const QByteArray& data) {
    if (data.isEmpty()) return {};
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return {};
    QJsonObject obj = doc.object();

    // Try official Maven API: {"versions": ["str", ...]}
    QJsonArray arr = obj.value(QStringLiteral("versions")).toArray();
    if (!arr.isEmpty()) {
        QStringList names;
        for (const QJsonValue& v : arr) {
            if (v.isString()) {
                QString n = v.toString();
                if (!n.isEmpty()) names.append(n);
            }
        }
        if (!names.isEmpty()) return names;
    }

    // Try BMCLAPI: {"files": [{"name": "str", ...}, ...]}
    arr = obj.value(QStringLiteral("files")).toArray();
    QStringList names;
    for (const QJsonValue& v : arr) {
        if (v.isObject()) {
            QString n = v.toObject().value(QStringLiteral("name")).toString();
            if (!n.isEmpty()) names.append(n);
        }
    }
    return names;
}

static QVariantList parseNeoForgeVersions(const QByteArray& latestData, const QByteArray& legacyData, const QString& filterMc) {
    QStringList versionNames = parseNeoForgeApiNames(latestData);
    versionNames.append(parseNeoForgeApiNames(legacyData));

    QVariantList result;
    for (const QString& apiName : versionNames) {
        bool isBeta = apiName.contains(QStringLiteral("beta"), Qt::CaseInsensitive)
                   || apiName.contains(QStringLiteral("alpha"), Qt::CaseInsensitive);
        QString versionName;
        QString inherit;

        // Format 1: Legacy 1.20.1-47.1.99
        if (apiName.contains(QStringLiteral("1.20.1"))) {
            versionName = apiName;
            versionName.replace(QStringLiteral("1.20.1-"), QString());
            inherit = QStringLiteral("1.20.1");
        }
        // Format 2: Experimental / snapshot 0.25w14craftmine.3-beta
        else if (apiName.startsWith(QStringLiteral("0."))) {
            versionName = apiName;
            QStringList segments = apiName.section(QLatin1Char('-'), 0, 0).split(QLatin1Char('.'));
            inherit = segments.value(1);
            if (inherit.isEmpty()) continue;
        }
        // Format 3: Standard 20.4.30-beta or 26.1.0.0-alpha
        else {
            versionName = apiName;
            QString verStr = apiName.section(QLatin1Char('-'), 0, 0);
            QStringList parts = verStr.split(QLatin1Char('.'));
            if (parts.size() < 3) continue;
            int major = parts[0].toInt();
            int minor = parts[1].toInt();
            if (major >= 24) {
                inherit = QStringLiteral("%1.%2").arg(major).arg(minor);
            } else {
                inherit = QStringLiteral("1.%1.%2").arg(major).arg(minor);
            }
            if (inherit.endsWith(QStringLiteral(".0")))
                inherit.chop(2);
        }

        if (inherit != filterMc) continue;

        QVariantMap m;
        m[QStringLiteral("version")] = versionName;
        m[QStringLiteral("type")] = isBeta ? QStringLiteral("beta") : QStringLiteral("release");
        m[QStringLiteral("date")] = QString();
        m[QStringLiteral("inherit")] = inherit;
        result.append(m);
    }
    return result;
}


void ShadowBackend::queryNeoForgeVersions(const QString& mcVersion) {
    m_modLoaderQueriesCancelled = false;

    struct SubReq { bool done = false; QByteArray data; };
    struct SourceState {
        SubReq latest; SubReq legacy; bool parsed = false; QVariantList result;
    };
    struct DualState { SourceState bmcl; SourceState official; bool emitted = false; };
    auto state = std::make_shared<DualState>();

    auto checkSourceComplete = [this, state, mcVersion](bool isBmcl) {
        if (state->emitted) return;
        SourceState& src = isBmcl ? state->bmcl : state->official;
        if (!src.latest.done || !src.legacy.done) return;
        if (src.parsed) return;
        src.parsed = true;
        src.result = parseNeoForgeVersions(src.latest.data, src.legacy.data, mcVersion);

        if (!src.result.isEmpty()) {
            state->emitted = true;
            emit neoforgeVersionsReady(src.result);
            return;
        }
        SourceState& other = isBmcl ? state->official : state->bmcl;
        if (other.parsed) {
            state->emitted = true;
            emit neoforgeVersionsReady(other.result.isEmpty() ? QVariantList() : other.result);
        }
    };

    auto fireSubRequest = [this, state, checkSourceComplete](bool isBmcl, bool isLatest) {
        QString url = isBmcl
            ? (isLatest
                ? QStringLiteral("https://bmclapi2.bangbang93.com/neoforge/meta/api/maven/details/releases/net/neoforged/neoforge")
                : QStringLiteral("https://bmclapi2.bangbang93.com/neoforge/meta/api/maven/details/releases/net/neoforged/forge"))
            : (isLatest
                ? QStringLiteral("https://maven.neoforged.net/api/maven/versions/releases/net/neoforged/neoforge")
                : QStringLiteral("https://maven.neoforged.net/api/maven/versions/releases/net/neoforged/forge"));

        auto* mgr = new QNetworkAccessManager(this);
        QNetworkRequest req{QUrl(url)};
        req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
        QNetworkReply* reply = mgr->get(req);
        m_modLoaderReplies.append(reply);

        QObject::connect(reply, &QNetworkReply::finished, this,
            [this, reply, mgr, isBmcl, isLatest, state, checkSourceComplete]() {
                m_modLoaderReplies.removeAll(reply);
                if (m_modLoaderQueriesCancelled) {
                    reply->deleteLater(); mgr->deleteLater(); return;
                }
                SubReq& sub = isBmcl
                    ? (isLatest ? state->bmcl.latest : state->bmcl.legacy)
                    : (isLatest ? state->official.latest : state->official.legacy);
                if (reply->error() == QNetworkReply::NoError)
                    sub.data = reply->readAll();
                sub.done = true;
                reply->deleteLater();
                mgr->deleteLater();
                checkSourceComplete(isBmcl);
            });
    };

    fireSubRequest(true, true);
    fireSubRequest(true, false);
    fireSubRequest(false, true);
    fireSubRequest(false, false);
}



// ── OptiFine HTML 官方源解析 ──

static QVariantList parseOptifineOfficialVersions(const QByteArray& html, const QString& filterMc) {
    QString text = QString::fromUtf8(html);
    if (text.length() < 200) return {};

    // Extract version names: e.g. "OptiFine_1.12.2_HD_U_C8.jar"
    QRegularExpression nameRe(QStringLiteral("OptiFine_([0-9A-Za-z_.]+)(?=\\.jar\")"));
    // Extract Forge compatibility: e.g. "Forge 61.0.8" or "Forge N/A"
    QRegularExpression forgeRe(QStringLiteral("(?<=colForge'>)[^<]*"));
    // Extract release dates: e.g. "2024.1.15"
    QRegularExpression dateRe(QStringLiteral("(?<=colDate'>)[^<]+"));

    QStringList names;
    QRegularExpressionMatchIterator ni = nameRe.globalMatch(text);
    while (ni.hasNext()) names.append(ni.next().captured(1));

    QStringList forgeStrs;
    QRegularExpressionMatchIterator fi = forgeRe.globalMatch(text);
    while (fi.hasNext()) forgeStrs.append(fi.next().captured());

    QStringList dates;
    QRegularExpressionMatchIterator di = dateRe.globalMatch(text);
    while (di.hasNext()) dates.append(di.next().captured());

    if (names.isEmpty()) return {};

    QVariantList result;
    int count = qMin(names.size(), qMin(forgeStrs.size(), dates.size()));
    if (count == 0) count = names.size();  // fallback if colCount mismatch

    for (int i = 0; i < names.size(); ++i) {
        QString rawName = names[i];  // e.g. "1.12.2_HD_U_C8"
        QString spaced = rawName;
        spaced.replace(QStringLiteral("_"), QStringLiteral(" "));  // "1.12.2 HD U C8"
        QStringList parts = spaced.split(QStringLiteral(" "));
        if (parts.isEmpty()) continue;
        QString inherit = parts[0];  // MC version from the name
        if (inherit.endsWith(QStringLiteral(".0")))
            inherit.chop(2);

        // Filter by requested MC version
        if (inherit != filterMc) continue;

        // Reconstruct version string like BMCLAPI: e.g. "HD_U_C8"
        QString prefix = QStringLiteral("OptiFine_") + filterMc + QStringLiteral("_");
        QString ver = rawName;
        if (ver.startsWith(prefix))
            ver = ver.mid(prefix.length());

        QVariantMap m;
        m[QStringLiteral("version")] = ver;
        m[QStringLiteral("type")] = QStringLiteral("release");
        // 解析日期格式 "2024.1.15" → "2024/01/15"
        QString dateStr;
        if (i < dates.size()) {
            QStringList d = dates[i].split(QLatin1Char('.'));
            if (d.size() == 3)
                dateStr = QStringLiteral("%1/%2/%3").arg(d[2], 2, QLatin1Char('0')).arg(d[1], 2, QLatin1Char('0')).arg(d[0], 2, QLatin1Char('0'));
        }
        m[QStringLiteral("date")] = dateStr;
        // Derive bmclType/bmclPatch from version string (consistent with BMCLAPI format)
        // ver e.g. "HD_U_C8" → type="HD_U", patch="C8"
        // ver e.g. "HD_U_J9_pre1" → type="HD_U", patch="J9_pre1"
        {
            QStringList vParts = ver.split(QLatin1Char('_'));
            if (vParts.size() >= 2) {
                m[QStringLiteral("bmclType")] = vParts[0] + QLatin1Char('_') + vParts[1];
                vParts.removeFirst(); vParts.removeFirst();
                m[QStringLiteral("bmclPatch")] = vParts.join(QLatin1Char('_'));
            } else {
                m[QStringLiteral("bmclType")] = QStringLiteral("HD_U");
                m[QStringLiteral("bmclPatch")] = ver;
            }
        }
        // Forge compatibility
        if (i < forgeStrs.size()) {
            QString f = forgeStrs[i];
            if (f.contains(QStringLiteral("N/A")))
                m[QStringLiteral("forge")] = QStringLiteral("Forge N/A");
            else if (!f.isEmpty())
                m[QStringLiteral("forge")] = f;
            else
                m[QStringLiteral("forge")] = QString();
        } else {
            m[QStringLiteral("forge")] = QString();
        }
        result.append(m);
    }
    return result;
}

void ShadowBackend::queryOptifineVersions(const QString& mcVersion) {
    m_modLoaderQueriesCancelled = false;

    struct SourceReq { bool done = false; QByteArray data; };
    struct DualState { SourceReq bmcl; SourceReq official; bool parsedBmcl = false; bool parsedOfficial = false; QVariantList bmclResult; QVariantList officialResult; bool emitted = false; };
    auto state = std::make_shared<DualState>();

    auto parseBmclJson = [mcVersion](const QByteArray& data) -> QVariantList {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isArray()) return {};
        QJsonArray arr = doc.array();
        QVariantList list;
        for (const QJsonValue& v : arr) {
            QJsonObject obj = v.toObject();
            QString fn = obj.value(QStringLiteral("filename")).toString();
            if (fn.isEmpty()) continue;
            QString prefix = QStringLiteral("OptiFine_") + mcVersion + QStringLiteral("_");
            QString ver = fn;
            if (fn.startsWith(prefix)) ver = fn.mid(prefix.length());
            if (ver.endsWith(QStringLiteral(".jar"))) ver = ver.left(ver.length() - 4);
            QVariantMap m;
            m[QStringLiteral("version")] = ver;
            m[QStringLiteral("type")] = QStringLiteral("release");
            m[QStringLiteral("date")] = QString();
            m[QStringLiteral("bmclType")] = obj.value(QStringLiteral("type")).toString();
            m[QStringLiteral("bmclPatch")] = obj.value(QStringLiteral("patch")).toString();
            m[QStringLiteral("forge")] = obj.value(QStringLiteral("forge")).toString();
            list.append(m);
        }
        return list;
    };

    auto checkSourceComplete = [this, state, parseBmclJson, mcVersion](bool isBmcl) {
        if (state->emitted) return;
        SourceReq& src = isBmcl ? state->bmcl : state->official;
        if (!src.done) return;

        QVariantList list;
        if (isBmcl) {
            list = parseBmclJson(src.data);
            state->bmclResult = list;
            state->parsedBmcl = true;
        } else {
            list = parseOptifineOfficialVersions(src.data, mcVersion);
            state->officialResult = list;
            state->parsedOfficial = true;
        }

        if (!list.isEmpty()) {
            state->emitted = true;
            emit optifineVersionsReady(list);
            return;
        }
        bool otherParsed = isBmcl ? state->parsedOfficial : state->parsedBmcl;
        if (otherParsed) {
            state->emitted = true;
            const QVariantList& otherResult = isBmcl ? state->officialResult : state->bmclResult;
            emit optifineVersionsReady(otherResult.isEmpty() ? QVariantList() : otherResult);
        }
    };

    auto fireRequest = [this, state, checkSourceComplete, &mcVersion](bool isBmcl) {
        QString url = isBmcl
            ? QStringLiteral("https://bmclapi2.bangbang93.com/optifine/") + mcVersion
            : QStringLiteral("https://optifine.net/downloads");

        auto* mgr = new QNetworkAccessManager(this);
        QNetworkRequest req{QUrl(url)};
        req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply* reply = mgr->get(req);
        qCInfo(logApp) << QStringLiteral("[加载器] 查询版本列表 加载器=OptiFine url=%1").arg(url);
        m_modLoaderReplies.append(reply);

        QObject::connect(reply, &QNetworkReply::finished, this,
            [this, reply, mgr, isBmcl, state, checkSourceComplete]() {
                m_modLoaderReplies.removeAll(reply);
                if (m_modLoaderQueriesCancelled) {
                    qCInfo(logApp) << QStringLiteral("[加载器] 查询已取消 加载器=OptiFine");
                    reply->deleteLater(); mgr->deleteLater(); return;
                }
                if (reply->error() != QNetworkReply::NoError) {
                    qCWarning(logApp) << QStringLiteral("[加载器] 查询失败 加载器=OptiFine 源=%1 错误=%2")
                        .arg(isBmcl ? QStringLiteral("BMCLAPI") : QStringLiteral("官方"))
                        .arg(reply->errorString());
                } else {
                    SourceReq& src = isBmcl ? state->bmcl : state->official;
                    src.data = reply->readAll();
                    qCInfo(logApp) << QStringLiteral("[加载器] 获取版本数据 加载器=OptiFine 源=%1 大小=%2字节")
                        .arg(isBmcl ? QStringLiteral("BMCLAPI") : QStringLiteral("官方"))
                        .arg(src.data.size());
                }
                SourceReq& src = isBmcl ? state->bmcl : state->official;
                src.done = true;
                reply->deleteLater();
                mgr->deleteLater();
                checkSourceComplete(isBmcl);
            });
    };

    fireRequest(true);
    fireRequest(false);
}

void ShadowBackend::queryFabricApiVersions(const QString& mcVersion) {
    auto parseResponse = [](const QByteArray& data) -> QVariantList {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isArray()) return {};
        QJsonArray arr = doc.array();
        QVariantList list;
        for (const QJsonValue& v : arr) {
            QJsonObject obj = v.toObject();
            QJsonArray files = obj.value(QStringLiteral("files")).toArray();
            if (files.isEmpty()) continue;
            QJsonObject file = files.first().toObject();
            QVariantMap m;
            m[QStringLiteral("version")] = obj.value(QStringLiteral("version_number")).toString();
            m[QStringLiteral("name")] = obj.value(QStringLiteral("name")).toString();
            m[QStringLiteral("date")] = obj.value(QStringLiteral("date_published")).toString().left(10);
            m[QStringLiteral("url")] = file.value(QStringLiteral("url")).toString();
            m[QStringLiteral("filename")] = file.value(QStringLiteral("filename")).toString();
            m[QStringLiteral("sha1")] = file.value(QStringLiteral("hashes")).toObject().value(QStringLiteral("sha1")).toString();
            m[QStringLiteral("size")] = file.value(QStringLiteral("size")).toDouble();
            list.append(m);
        }
        return list;
    };

    const QString path = QStringLiteral("/project/fabric-api/version"
        "?loaders=[%22fabric%22]&game_versions=[%22") + mcVersion + QStringLiteral("%22]");
    const QString mcimUrl = QStringLiteral("https://mod.mcimirror.top/modrinth/v2") + path;
    const QString fallbackUrl = QStringLiteral("https://api.modrinth.com/v2") + path;

    qCDebug(logApp) << "[FabricApi] querying MCIM mirror:" << mcimUrl;
    HttpClient::instance().get(mcimUrl,
        [this, parseResponse, fallbackUrl](int status, const QByteArray& body) {
            if (m_modLoaderQueriesCancelled) {
                qCDebug(logApp) << "[FabricApi] query cancelled";
                return;
            }
            if (status == 200 && !body.isEmpty()) {
                QVariantList list = parseResponse(body);
                qCDebug(logApp) << "[FabricApi] MCIM got" << list.size() << "versions";
                emit fabricApiVersionsReady(list);
                return;
            }
            if (m_modLoaderQueriesCancelled) return;
            qCWarning(logApp) << QStringLiteral("[FabricApi] MCIM镜像失败 状态码=%1").arg(status)
                               << "), falling back to Modrinth direct";
            HttpClient::instance().get(fallbackUrl,
                [this, parseResponse](int status2, const QByteArray& body2) {
                    if (m_modLoaderQueriesCancelled) {
                        qCDebug(logApp) << "[FabricApi] fallback query cancelled";
                        return;
                    }
                    if (status2 == 200 && !body2.isEmpty()) {
                        QVariantList list = parseResponse(body2);
                        qCDebug(logApp) << "[FabricApi] direct got" << list.size() << "versions";
                        emit fabricApiVersionsReady(list);
                    } else {
                        qCWarning(logApp) << QStringLiteral("[FabricApi] 直连也失败 状态码=%1").arg(status2);
                        emit fabricApiVersionsReady({});
                    }
                });
        });
}

void ShadowBackend::cancelModLoaderQueries() {
    qCDebug(logApp) << "[ModLoader] cancelling all in-flight queries";
    m_modLoaderQueriesCancelled = true;
    // Abort all tracked network replies
    for (const QPointer<QNetworkReply>& r : m_modLoaderReplies) {
        if (r && r->isRunning())
            r->abort();
    }
    m_modLoaderReplies.clear();
}

void ShadowBackend::cacheForgeInstallerSha1(const QString& mcVer, const QString& forgeVer, const QString& sha1) {
    if (!sha1.isEmpty())
        m_forgeInstallerSha1Cache.insert(mcVer + QStringLiteral("-") + forgeVer, sha1);
}

QString ShadowBackend::getForgeInstallerSha1(const QString& mcVer, const QString& forgeVer) const {
    return m_forgeInstallerSha1Cache.value(mcVer + QStringLiteral("-") + forgeVer);
}

void ShadowBackend::cacheForgeInstallerBranch(const QString& mcVer, const QString& forgeVer, const QString& branch) {
    m_forgeInstallerBranchCache.insert(mcVer + QStringLiteral("-") + forgeVer, branch);
}

QString ShadowBackend::getForgeInstallerBranch(const QString& mcVer, const QString& forgeVer) const {
    return m_forgeInstallerBranchCache.value(mcVer + QStringLiteral("-") + forgeVer);
}

void ShadowBackend::installModLoader(const QString& mcVersion, const QString& loaderType,
                                      const QString& loaderVersion, const QString& installName,
                                      const QString& fabricApiVersion,
                                      const QString& fabricApiUrl,
                                      const QString& fabricApiSavePath,
                                      const QString& forgeInstallerSha1) {
    qDebug() << "[install] ShadowBackend::installModLoader" << loaderType << mcVersion << loaderVersion << installName;
    QString sha1 = forgeInstallerSha1.isEmpty()
        ? getForgeInstallerSha1(mcVersion, loaderVersion)
        : forgeInstallerSha1;
    if (!sha1.isEmpty())
        qDebug() << "[install] Forge SHA1 cached:" << sha1.left(16) << "...";
    // Always look up branch from cache (populated alongside SHA1 in queryForgeVersions)
    QString branch = getForgeInstallerBranch(mcVersion, loaderVersion);
    if (!branch.isEmpty())
        qDebug() << "[install] Forge branch:" << branch;
    if (m_version) m_version->installModLoader(mcVersion, loaderType, loaderVersion, installName,
                                                 fabricApiVersion, fabricApiUrl, fabricApiSavePath, sha1, branch);
}

void ShadowBackend::installOptifine(const QString& mcVersion, const QString& optifineVersion,
                                     const QString& forgeVersion, const QString& installName,
                                     const QString& bmclType, const QString& bmclPatch) {
    qDebug() << "[install] ShadowBackend::installOptifine" << mcVersion << optifineVersion << forgeVersion << installName;
    if (m_version) m_version->installOptifine(mcVersion, optifineVersion, forgeVersion, installName, bmclType, bmclPatch);
}

void ShadowBackend::installOptifineJar(const QString& mcVersion, const QString& optifineVersion,
                                       const QString& bmclType, const QString& bmclPatch,
                                       const QString& installName) {
    if (m_version) m_version->installOptifineJar(mcVersion, optifineVersion, bmclType, bmclPatch, installName);
}

void ShadowBackend::setPendingUserDataImport(const QString& installId, const QString& archivePath) {
    if (m_version) m_version->setPendingUserDataImport(installId, archivePath);
}

void ShadowBackend::cancelPendingUserDataImport(const QString& installId) {
    if (m_version) m_version->cancelPendingUserDataImport(installId);
}

// ── Language hot-switch implementation ──
void ShadowBackend::switchLanguage(int index)
{
    const QStringList codes = { QStringLiteral("zh_CN"), QStringLiteral("zh_HK"), QStringLiteral("zh_TW") };
    if (index < 0 || index >= codes.size()) return;
    QString lang = codes[index];
    if (lang == m_currentLang) return;
    m_currentLang = lang;

    // 1. Save preference
    m_settings->setLanguageIndex(index);

    // 2. Remove old translator
    if (m_translator) {
        qApp->removeTranslator(m_translator);
        delete m_translator;
        m_translator = nullptr;
    }

    // 3. Install new translator (skip for zh_CN — source language)
    if (lang != QStringLiteral("zh_CN")) {
        m_translator = new QTranslator(this);
        if (m_translator->load(QStringLiteral(":/i18n/shadow_%1").arg(lang))) {
            qApp->installTranslator(m_translator);
            qCInfo(logApp) << QStringLiteral("语言切换 lang=%1").arg(lang);
        } else {
            qCWarning(logApp) << QStringLiteral("翻译加载失败 lang=%1").arg(lang);
            delete m_translator;
            m_translator = nullptr;
        }
    }

    // 4. Re-evaluate QML bindings (Qt 6.2+ retranslate)
    if (m_engine) {
        m_engine->retranslate();
    }

    // 5. Notify C++ widgets (tr() strings will reload via changeEvent)
    QEvent ev(QEvent::LanguageChange);
    QCoreApplication::sendEvent(qApp, &ev);

    // 6. Persist to language.txt for QML recovery
    writeLanguageFile(index);
}

bool ShadowBackend::event(QEvent* ev)
{
    if (ev->type() == QEvent::LanguageChange) {
        // Cached strings that need retranslation after LanguageChange
        // (QML bindings are handled by engine->retranslate() above)
    }
    return QObject::event(ev);
}

int ShadowBackend::readLanguageFile() const
{
    QString path = m_app->dataDir() + "/language.txt";
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        bool ok = false;
        int idx = f.readAll().trimmed().toInt(&ok);
        f.close();
        if (ok && idx >= 0 && idx <= 2) return idx;
    }
    return 0;  // default: zh_CN
}

void ShadowBackend::writeLanguageFile(int index) const
{
    QString path = m_app->dataDir() + "/language.txt";
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QByteArray::number(index));
        f.close();
        qDebug() << "[Language] written" << index << "to" << path;
    }
}

// ============================================================
// Beta key gate
// ============================================================

void ShadowBackend::submitBetaKey(const QString& key)
{
    if (key.trimmed().isEmpty()) {
        emit betaKeyInvalid(QStringLiteral("请输入内测密钥"));
        return;
    }

    m_betaStatus = QStringLiteral("checking");
    emit betaStatusChanged();

    // Beta key verification endpoint — decrypted from opaque flat blob on each call.
    // Not cached: decrypted inline, zeroed after QNetworkRequest consumes it.
    const uint8_t* blob = getAssembledBlob();
    QByteArray plain = aesGcmDecrypt(
        blob + kWorkerNonceOff, kWorkerNonceLen,
        blob + kWorkerCTOff,   kWorkerCTLen,
        blob + kWorkerTagOff,  kWorkerTagLen,
        blob + kWorkerKeyOff,  blob + kWorkerSaltOff);
    QString kWorkerUrl;
    if (!plain.isEmpty())
        kWorkerUrl = QString::fromUtf8(plain);
    SecureZeroMemory(plain.data(), plain.size());

    if (kWorkerUrl.isEmpty()) {
        qCInfo(logApp) << QStringLiteral("[BetaKey] 验证跳过 未配置Worker URL");
        m_betaStatus = QStringLiteral("disabled");
        emit betaStatusChanged();
        return;
    }

    auto* mgr = new QNetworkAccessManager(this);
    QNetworkRequest req{QUrl(kWorkerUrl)};
    // Worker URL consumed by QNetworkRequest — zero the local copy
    secureWipe(kWorkerUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["key"] = key.trimmed();

    QNetworkReply* reply = mgr->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, key, reply, mgr]() {
        reply->deleteLater();
        mgr->deleteLater();

        bool ok = false;
        if (reply->error() == QNetworkReply::NoError) {
            auto json = QJsonDocument::fromJson(reply->readAll()).object();
            ok = json["allowed"].toBool();
            if (ok) {
                // Save encrypted key locally
                if (saveBetaKey(key)) {
                    m_betaStatus = QStringLiteral("verified");
                    emit betaStatusChanged();
                    emit betaVerified();
                } else {
                    emit betaKeyInvalid(QStringLiteral("密钥保存失败，请检查磁盘空间"));
                }
                return;
            }
        }

        m_betaStatus.clear();
        emit betaStatusChanged();
        emit betaKeyInvalid(QStringLiteral("无效的内测密钥"));
    });
}

bool ShadowBackend::saveBetaKey(const QString& key)
{
    QString keyPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                      + QStringLiteral("/.shadow_beta_key");
    QDir().mkpath(QFileInfo(keyPath).absolutePath());

#ifdef Q_OS_WIN
    DATA_BLOB in, out;
    QByteArray utf8 = key.toUtf8();
    in.pbData = reinterpret_cast<BYTE*>(utf8.data());
    in.cbData = static_cast<DWORD>(utf8.size());

    if (!CryptProtectData(&in, L"Shadow Beta", nullptr, nullptr, nullptr,
                          0, &out)) {
        qCWarning(logApp) << QStringLiteral("[BetaKey] CryptProtectData失败");
        return false;
    }

    QFile f(keyPath);
    if (!f.open(QIODevice::WriteOnly)) {
        LocalFree(out.pbData);
        return false;
    }
    f.write(reinterpret_cast<const char*>(out.pbData), static_cast<qint64>(out.cbData));
    f.close();
    LocalFree(out.pbData);
    qCInfo(logApp) << QStringLiteral("[BetaKey] 加密密钥已保存");
    return true;
#else
    // Non-Windows: store as plain text (beta gate is Windows-only for now)
    QFile f(keyPath);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(key.toUtf8());
    f.close();
    return true;
#endif
}

QString ShadowBackend::loadBetaKey()
{
    QString keyPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                      + QStringLiteral("/.shadow_beta_key");
    QFile f(keyPath);
    if (!f.exists()) return {};

    if (!f.open(QIODevice::ReadOnly)) return {};
    QByteArray encrypted = f.readAll();
    f.close();

#ifdef Q_OS_WIN
    DATA_BLOB in, out;
    in.pbData = reinterpret_cast<BYTE*>(encrypted.data());
    in.cbData = static_cast<DWORD>(encrypted.size());

    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
        qCWarning(logApp) << QStringLiteral("[BetaKey] CryptUnprotectData失败 密钥可能已损坏");
        f.remove();
        return {};
    }

    QString key = QString::fromUtf8(reinterpret_cast<const char*>(out.pbData),
                                     static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return key;
#else
    return QString::fromUtf8(encrypted);
#endif
}

bool ShadowBackend::validateBetaKey(const QString& key, QString* outError)
{
    // Decrypt Worker endpoint from opaque flat blob (AES-256-GCM).
    // With public all-zero placeholder → decrypt fails → validation disabled.
    // Not cached: decrypted inline, zeroed after QNetworkRequest consumes it.
    const uint8_t* blob = getAssembledBlob();
    QByteArray plain = aesGcmDecrypt(
        blob + kWorkerNonceOff, kWorkerNonceLen,
        blob + kWorkerCTOff,   kWorkerCTLen,
        blob + kWorkerTagOff,  kWorkerTagLen,
        blob + kWorkerKeyOff,  blob + kWorkerSaltOff);
    QString kWorkerUrl;
    if (!plain.isEmpty())
        kWorkerUrl = QString::fromUtf8(plain);
    SecureZeroMemory(plain.data(), plain.size());

    if (kWorkerUrl.isEmpty()) {
        if (outError) *outError = QStringLiteral("内测验证未配置");
        return false;
    }

    QNetworkAccessManager mgr;
    QNetworkRequest req{QUrl(kWorkerUrl)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    secureWipe(kWorkerUrl);

    QJsonObject body;
    body["key"] = key;

    QNetworkReply* reply = mgr.post(req, QJsonDocument(body).toJson());

    // 5-second timeout so we don't hang on network issues
    QTimer timeout;
    timeout.setSingleShot(true);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        reply->abort();
        loop.quit();
    });
    timeout.start(5000);
    loop.exec();

    bool ok = false;
    if (reply->error() == QNetworkReply::NoError) {
        auto json = QJsonDocument::fromJson(reply->readAll()).object();
        ok = json["allowed"].toBool();
    } else if (outError) {
        *outError = reply->errorString();
    }
    reply->deleteLater();
    return ok;
}

void ShadowBackend::logUiMsg(const QString& msg)
{
    qCInfo(logUI) << msg;
}

int ShadowBackend::diagAutoLangComboIdx() const
{
    if (!m_settings) return 0;
    return m_settings->autoLangModeComboIndex();
}

void ShadowBackend::setAutoLangModeFromCombo(int idx)
{
    if (!m_settings) return;
    // idx → mode: 0→1(系统区域), 1→2(IP属地), 2→0(关闭)
    int mode = idx == 0 ? 1 : (idx == 1 ? 2 : 0);
    m_settings->setAutoLangMode(mode);
}

void ShadowBackend::refreshGameStats()
{
    if (m_stats)
        m_stats->refresh();
}

double ShadowBackend::totalGameHours() const
{
    return m_stats ? m_stats->totalHours() : 0;
}

QVariantList ShadowBackend::versionGameStats() const
{
    return m_stats ? m_stats->versionStats() : QVariantList();
}

bool ShadowBackend::statsLoading() const
{
    return m_stats && m_stats->loading();
}

bool ShadowBackend::statsEmpty() const
{
    return m_stats ? m_stats->isEmpty() : true;
}

// ============================================================
// Update
// ============================================================

void ShadowBackend::checkChangelog()
{
    QString updateDir = QCoreApplication::applicationDirPath()
                        + QStringLiteral("/_update/");

    // ── Debug mechanism: force changelog popup via debug file ──
    QString debugPath = updateDir + QStringLiteral("debug_changelog.json");
    QFileInfo debugFi(debugPath);
    if (debugFi.exists() && debugFi.size() <= 65536) {
        QFile df(debugPath);
        if (df.open(QIODevice::ReadOnly)) {
            QJsonParseError derr;
            QJsonDocument ddoc = QJsonDocument::fromJson(df.readAll(), &derr);
            df.close();
            if (derr.error == QJsonParseError::NoError) {
                QJsonObject dobj = ddoc.object();
                QString dver = dobj.value("version").toString();
                bool dPersistent = dobj.value("persistent").toBool(false); // 默认 false：仅弹出一次

                // ── Gitee mode: fetch latest release notes from Gitee API ──
                if (dobj.value("gitee").toBool()) {
                    if (dver.isEmpty()) dver = appVersion();
                    qCInfo(logApp) << "[ShadowBackend] 调试公告 — 从Gitee获取发布说明"
                                   << (dPersistent ? "[持久模式]" : "[单次模式]");
                    QString giteeUrl = QStringLiteral(
                        "https://gitee.com/api/v5/repos/YOUR_GITEE_OWNER/YOUR_GITEE_REPO/releases/latest");
                    HttpClient::instance().get(giteeUrl,
                        [this, dver, dPersistent, debugPath](int status, const QByteArray& body) {
                            if (status != 200 || body.isEmpty()) {
                                qCWarning(logApp) << "[ShadowBackend] Gitee获取失败 status=" << status;
                                if (!dPersistent) QFile::remove(debugPath);
                                return;
                            }
                            QJsonParseError perr;
                            QJsonDocument pdoc = QJsonDocument::fromJson(body, &perr);
                            if (perr.error != QJsonParseError::NoError) {
                                qCWarning(logApp) << "[ShadowBackend] Gitee响应JSON解析失败";
                                if (!dPersistent) QFile::remove(debugPath);
                                return;
                            }
                            QJsonObject release = pdoc.object();
                            QString rawNotes = release.value("body").toString();
                            if (rawNotes.isEmpty()) {
                                qCWarning(logApp) << "[ShadowBackend] Gitee发布说明为空";
                                if (!dPersistent) QFile::remove(debugPath);
                                return;
                            }
                            // 规范化换行以适应 Markdown 渲染：
                            //   \r\n → \n\n（空白行 = 段落间隔）
                            //   连续 \n\n → 单个 \n\n（避免三重空行）
                            QString giteeNotes = rawNotes;
                            giteeNotes.replace(QStringLiteral("\r\n"), QStringLiteral("\n\n"));
                            giteeNotes.replace(QRegularExpression(QStringLiteral("\n{3,}")),
                                               QStringLiteral("\n\n"));
                            qCInfo(logApp) << "[ShadowBackend] Gitee公告获取成功 version=" << dver
                                           << " len=" << giteeNotes.size();
                            emit updateChangelogAvailable(dver, giteeNotes);
                            // 非持久模式：发射信号后立即删除调试文件，下次启动不再弹出
                            if (!dPersistent) {
                                QFile::remove(debugPath);
                                qCInfo(logApp) << "[ShadowBackend] 调试公告文件已删除（单次模式）";
                            }
                        },
                        [debugPath, dPersistent](const QString& err) {
                            qCWarning(logApp) << "[ShadowBackend] Gitee请求失败:" << err;
                            if (!dPersistent) QFile::remove(debugPath);
                        });
                    return;
                }
                // ── Local mode: use notes from debug file ──
                QString dnotes = dobj.value("notes").toString();
                if (!dver.isEmpty() && !dnotes.isEmpty()) {
                    qCInfo(logApp) << "[ShadowBackend] 调试公告触发 version=" << dver
                                   << (dPersistent ? "[持久模式]" : "[单次模式]");
                    emit updateChangelogAvailable(dver, dnotes);
                    // 非持久模式：删除调试文件
                    if (!dPersistent) {
                        QFile::remove(debugPath);
                        qCInfo(logApp) << "[ShadowBackend] 调试公告文件已删除（单次模式）";
                    }
                    return;
                }
            }
        }
    }

    // ── Normal post-update changelog ──
    QString clPath = updateDir + QStringLiteral("changelog_to_show.json");
    QFileInfo fi(clPath);
    if (!fi.exists() || fi.size() > 65536) return; // 64KB sanity limit

    QFile f(clPath);
    if (!f.open(QIODevice::ReadOnly)) return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();

    if (err.error != QJsonParseError::NoError) {
        QFile::remove(clPath);
        return;
    }

    QJsonObject obj = doc.object();
    QString version = obj.value("version").toString();
    QString notes = obj.value("notes").toString();

    if (version == appVersion() && !notes.isEmpty()) {
        qCInfo(logApp) << "[ShadowBackend] 发现更新公告 version=" << version
                       << " len=" << notes.size();
        emit updateChangelogAvailable(version, notes);
    }

    QFile::remove(clPath);
}

void ShadowBackend::checkForUpdate()
{
    if (!m_updateManager) return;
    m_updateManager->checkUserInitiated();
}

bool ShadowBackend::updateChecking() const
{
    return m_updateManager ? m_updateManager->isBusy() : false;
}

int ShadowBackend::updateState() const
{
    return m_updateManager ? static_cast<int>(m_updateManager->state()) : 0;
}

} // namespace ShadowLauncher

