// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "shadow_backend.h"
#include "../core/http_client.h"
#include "../core/mod_manager.h"
#include "../core/update_manager.h"
#include "../core/icon_cache.h"
#include "../core/modpack_importer.h"
#include "../core/local_mod_manager.h"
#include "../multiplayer/multiplayer_manager.h"
#include "../multiplayer/relay_crypto.h"
#if __has_include("encrypted_addr_local.h")
#include "encrypted_addr_local.h"
#else
#include "../multiplayer/encrypted_addr.h"
#endif
#include "../core/version_downloader.h"
#include "../core/geoip_service.h"
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
#include "account_backend.h"
#include "app_backend.h"
#include "check_backend.h"
#include "userdata_backend.h"
#include "launch_backend.h"
#include "resource_backend.h"
#include "settings_backend.h"
#include "version_backend.h"
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
#include <QStorageInfo>
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

    m_userData = new UserDataBackend(this);
    bp("UserDataBackend");

    // ── Icon cache (3 separate caches: mod / shader / rp, each max 100) ──
    QString iconBase = m_app->dataDir() + "/icons";
    m_modIconCache = new IconCache(iconBase + "/mod", 100, this);
    m_shaderIconCache = new IconCache(iconBase + "/shader", 100, this);
    m_rpIconCache = new IconCache(iconBase + "/rp", 100, this);
    m_multiplayer = new MultiplayerManager(this);
    m_localMods = new LocalModManager(this);
    m_modpackImporter = new ModpackImporter(this);
    {
        auto* importer = qobject_cast<ModpackImporter*>(m_modpackImporter);
        if (importer) importer->setVersionBackend(m_version);
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
    // When GeoIP detects region, update VersionBackend
    connect(m_geoIp, &GeoIpService::regionDetected, this, [this](const QString& region) {
        m_version->setDetectedRegion(region);
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
    connect(m_version, &VersionBackend::versionListReady,
            this, [this]() {
                // Restore last selected version if it still exists
                QString last = m_settings->lastSelectedVersion();
                if (!last.isEmpty() && m_version->versionIds().contains(last)) {
                    m_version->setSelectedVersion(last);
                    qCInfo(logLaunch) << QStringLiteral("恢复上次选中版本 版本=%1").arg(last);
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

        QString gameDir = getVersionGameDir(vid);
        QString versionDir = m_app->gameDir() + QStringLiteral("/versions/") + vid;
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
        QString assetsPath = m_app->gameDir() + QStringLiteral("/assets/objects");
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
        m_currentVersionSummary = summary;
        emit currentVersionSummaryChanged();
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
        if ((m_lastLoginMode == 1 || m_lastLoginMode == 0) && !lastUser.isEmpty()) {
            // Auto-restore login on startup — differentiate mode
            QTimer::singleShot(100, this, [this, lastUser]() {
                if (m_lastLoginMode == 0) {
                    // Microsoft (online) — session already restored in AccountBackend ctor via loadMicrosoftSession()
                    // Just update the offline skin as fallback, don't override online state
                    qCInfo(logApp) << QStringLiteral("[启动] 恢复正版登录: ") << lastUser;
                } else {
                    // Offline mode
                    qCInfo(logApp) << QStringLiteral("[启动] 恢复离线登录: ") << lastUser;
                    m_account->offlineLogin(lastUser);
                }
            });
        }
    }

    // ── Signal forwarding: ResourceBackend → ShadowBackend ──
    connect(m_resource, &ResourceBackend::downloadStateChanged,
            this, [this]() {
                bool downloading = m_resource->isDownloading();
                if (!downloading) {
                    m_resourceDlProgress = 0;
                    m_resourceDlTotal = 0;
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
                if (m_version) m_version->removeResourceCard(QStringLiteral("resource"));
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
            this, [this](int dlId, qint64 received, qint64 total) {
                if (m_modDownloadCards.contains(dlId) && m_version && total > 0) {
                    m_version->updateResourceCard(m_modDownloadCards[dlId],
                        (qreal)received / total, QString());
                }
            });
    connect(m_resource, &ResourceBackend::modFileDownloadFinished,
            this, [this](int dlId, bool success, const QString&, const QString&) {
                Q_UNUSED(success);
                if (m_modDownloadCards.contains(dlId)) {
                    if (m_version) m_version->removeResourceCard(m_modDownloadCards[dlId]);
                    m_modDownloadCards.remove(dlId);
                }
            });
    connect(m_resource, &ResourceBackend::modFileDownloadFailed,
            this, [this](int dlId, const QString&, const QString&) {
                if (m_modDownloadCards.contains(dlId)) {
                    if (m_version) m_version->removeResourceCard(m_modDownloadCards[dlId]);
                    m_modDownloadCards.remove(dlId);
                }
            });

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
    QDir gameDir(m_app->gameDir());
    QString versionsPath = gameDir.absoluteFilePath(QStringLiteral("versions"));
    QDir versionsDir(versionsPath);

    m_versionDetails.clear();
    if (!versionsDir.exists()) {
        emit versionDetailsReady();
        return;
    }

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
        QString jarPath = verPath + QStringLiteral("/") + versionId + QStringLiteral(".jar");
        if (!QFileInfo::exists(jarPath)) continue;

        QVariantMap detail;
        detail[QStringLiteral("id")] = versionId;

        // ── Detect mod loader FIRST (directory-based + versionId-based) ──
        QString loaderType = tr("原版");
        QString loaderVersion;
        QString baseMcVersion = versionId;  // default: same as versionId

        // Try to detect loader from versionId pattern: "XX.X-neoforge-XX.X" etc.
        static const QRegularExpression loaderIdPattern(R"(^(.+?)-(neoforge|forge|fabric|quilt)-(.+)$)");
        QRegularExpressionMatch loaderMatch = loaderIdPattern.match(versionId);
        if (loaderMatch.hasMatch()) {
            baseMcVersion = loaderMatch.captured(1);  // e.g. "26.2"
            loaderVersion = loaderMatch.captured(3);  // e.g. "26.2.0.7-beta"
        }

        // Directory-based loader detection (more reliable than versionId parsing)
        // Check for NeoForge first (has dedicated neoforge/ subdir)
        if (QFileInfo::exists(verPath + QStringLiteral("/neoforge"))) {
            loaderType = QStringLiteral("NeoForge");
        }
        // Check for Fabric
        QString fabricJson = verPath + QStringLiteral("/fabric-installer.json");
        if (QFileInfo::exists(fabricJson)) {
            loaderType = QStringLiteral("Fabric");
        }
        // Check for Forge (mods dir exists AND no Fabric/NeoForge markers)
        QString forgeDir = verPath + QStringLiteral("/mods");
        if (QDir(forgeDir).exists()
            && !QFileInfo::exists(fabricJson)
            && !QFileInfo::exists(verPath + QStringLiteral("/neoforge"))
            && !QFileInfo::exists(verPath + QStringLiteral("/quilt"))) {
            loaderType = QStringLiteral("Forge");
        }
        // Check for Quilt
        if (QFileInfo::exists(verPath + QStringLiteral("/quilt"))) {
            loaderType = QStringLiteral("Quilt");
        }

        // Fallback: detect loader from versionId pattern when dir check fails
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

        // Compute total size
        qint64 totalSize = 0;
        QDirIterator it(verPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            totalSize += it.fileInfo().size();
        }
        detail[QStringLiteral("sizeBytes")] = totalSize;

        // Count mods
        int modCount = 0;
        if (QDir(forgeDir).exists()) {
            QDirIterator modIt(forgeDir, QStringList() << QStringLiteral("*.jar"), QDir::Files);
            while (modIt.hasNext()) { modIt.next(); modCount++; }
        }
        detail[QStringLiteral("modCount")] = modCount;

        m_versionDetails.append(detail);
    }

    emit versionDetailsReady();
    emit logMessage(tr("已扫描 %1 个已安装版本").arg(m_versionDetails.size()));
}

void ShadowBackend::refreshGameDirInfo()
{
    QDir gameDir(m_app->gameDir());
    QVariantMap info;

    // Count installed versions
    QString versionsPath = gameDir.absoluteFilePath(QStringLiteral("versions"));
    QDir versionsDir(versionsPath);
    int versionCount = 0;
    qint64 totalSize = 0;

    if (versionsDir.exists()) {
        const QStringList entries = versionsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& versionId : entries) {
            QString jarPath = versionsDir.filePath(versionId + QStringLiteral("/") + versionId + QStringLiteral(".jar"));
            if (QFileInfo::exists(jarPath)) versionCount++;

            QDirIterator it(versionsDir.filePath(versionId), QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) { it.next(); totalSize += it.fileInfo().size(); }
            // Also count isolated game directory
            QString gamePath = versionsDir.filePath(versionId + QStringLiteral("/game"));
            if (QDir(gamePath).exists()) {
                QDirIterator git(gamePath, QDir::Files, QDirIterator::Subdirectories);
                while (git.hasNext()) { git.next(); totalSize += git.fileInfo().size(); }
            }
        }
    }
    info[QStringLiteral("versionCount")] = versionCount;

    // Count shared assets (objects/ directory)
    QString assetsPath = gameDir.absoluteFilePath(QStringLiteral("assets"));
    QDir assetsDir(assetsPath);
    if (assetsDir.exists()) {
        QDirIterator ait(assetsPath, QDir::Files, QDirIterator::Subdirectories);
        while (ait.hasNext()) { ait.next(); totalSize += ait.fileInfo().size(); }
    }

    // Count mods
    QString modsPath = gameDir.absoluteFilePath(QStringLiteral("mods"));
    QDir modsDir(modsPath);
    int modCount = 0;
    if (modsDir.exists()) {
        QDirIterator modIt(modsPath, QStringList() << QStringLiteral("*.jar"), QDir::Files);
        while (modIt.hasNext()) { modIt.next(); modCount++; }
    }
    info[QStringLiteral("modCount")] = modCount;

    // Format size
    QString sizeDisplay;
    if (totalSize >= 1073741824) {
        sizeDisplay = QString::number(totalSize / 1073741824.0, 'f', 2) + QStringLiteral(" GB");
    } else if (totalSize >= 1048576) {
        sizeDisplay = QString::number(totalSize / 1048576.0, 'f', 1) + QStringLiteral(" MB");
    } else {
        sizeDisplay = QString::number(totalSize / 1024.0, 'f', 0) + QStringLiteral(" KB");
    }
    info[QStringLiteral("sizeDisplay")] = sizeDisplay;

    m_gameDirInfo = info;
    emit gameDirChanged();
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
    return m_modIconCache ? m_modIconCache->resolveUrl(url) : url;
}

void ShadowBackend::cacheIconBatchAsync(const QStringList &urls)
{
    if (m_modIconCache) m_modIconCache->cacheBatchAsync(urls);
}

QString ShadowBackend::iconCachedPath(const QString &url) const
{
    return m_modIconCache ? m_modIconCache->cachedPath(url) : QString();
}

QString ShadowBackend::resolveShaderIconUrl(const QString &url)
{
    return m_shaderIconCache ? m_shaderIconCache->resolveUrl(url) : url;
}

void ShadowBackend::cacheShaderIconBatchAsync(const QStringList &urls)
{
    if (m_shaderIconCache) m_shaderIconCache->cacheBatchAsync(urls);
}

QString ShadowBackend::resolveRpIconUrl(const QString &url)
{
    return m_rpIconCache ? m_rpIconCache->resolveUrl(url) : url;
}

void ShadowBackend::cacheRpIconBatchAsync(const QStringList &urls)
{
    if (m_rpIconCache) m_rpIconCache->cacheBatchAsync(urls);
}

qint64 ShadowBackend::diskFree() const
{
    QStorageInfo storage(m_app->gameDir());
    if (storage.isValid() && storage.bytesAvailable() > 0) {
        return storage.bytesAvailable();
    }
    // Fallback: query root drive
    QStorageInfo root(QDir::rootPath());
    return root.isValid() ? root.bytesAvailable() : 100LL * 1024 * 1024 * 1024;
}

int ShadowBackend::diskPercent() const
{
    QStorageInfo storage(m_app->gameDir());
    if (storage.isValid() && storage.bytesTotal() > 0) {
        return static_cast<int>(100.0 * (1.0 - static_cast<double>(storage.bytesAvailable()) / storage.bytesTotal()));
    }
    QStorageInfo root(QDir::rootPath());
    if (root.isValid() && root.bytesTotal() > 0) {
        return static_cast<int>(100.0 * (1.0 - static_cast<double>(root.bytesAvailable()) / root.bytesTotal()));
    }
    return 30;
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

bool ShadowBackend::openLogsFolder(const QString& versionId) {
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

bool ShadowBackend::openSavesFolder(const QString& versionId) {
    QString savesDir = gameDirForVersion(versionId) + QStringLiteral("/saves");
    QDir().mkpath(savesDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(savesDir));
    emit logMessage(tr("已打开存档文件夹"));
    return true;
}

bool ShadowBackend::openScreenshotsFolder(const QString& versionId) {
    QString screenshotsDir = gameDirForVersion(versionId) + QStringLiteral("/screenshots");
    QDir().mkpath(screenshotsDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(screenshotsDir));
    emit logMessage(tr("已打开截图文件夹"));
    return true;
}

bool ShadowBackend::openModsFolder(const QString& versionId) {
    QString modsDir = gameDirForVersion(versionId) + QStringLiteral("/mods");
    QDir().mkpath(modsDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(modsDir));
    emit logMessage(tr("已打开 Mod 文件夹"));
    return true;
}

bool ShadowBackend::openResourcePacksFolder(const QString& versionId) {
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

// ── 辅助：递归计算目录大小 ──
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

void ShadowBackend::openVersionDir(const QString& versionId) {
    m_settings->openVersionDir(versionId);
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
    m_version->refreshInstalled();
}

void ShadowBackend::installVersion(const QString& versionId) {
    m_version->installVersion(versionId);
}

void ShadowBackend::cancelInstall() {
    m_version->cancelInstall();
}

void ShadowBackend::cancelVersionInstall(const QString& versionId) {
    if (m_version) m_version->cancelVersionInstall(versionId);
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

    QString username = m_account->username();

    // Check per-version memory override
    int verMode = m_settings->versionMemoryMode(versionId);
    int maxMemory;
    if (verMode == 1) {
        // Per-version: auto
        maxMemory = m_launch->getAutoMemory();
    } else if (verMode == 2) {
        // Per-version: manual
        maxMemory = m_settings->versionMemoryManualMB(versionId);
    } else {
        // Per-version: follow global (0 or unset)
        maxMemory = m_settings->autoMemoryEnabled()
            ? m_launch->getAutoMemory()
            : m_settings->maxMemoryMB();
    }
    qCInfo(logApp) << QStringLiteral("启动内存: %1MB (verMode=%2, auto=%3)").arg(maxMemory).arg(verMode).arg(m_settings->autoMemoryEnabled());
    QString jvmArgs = resolvedJvmArgs(versionId);
    QString gameArgs = resolvedGameArgs(versionId);
    bool highPerfGpu = resolvedHighPerfGpu(versionId);
    m_launchVersion = versionId;
    m_launchUsername = username;

    // Determine required Java version from version JSON
    int requiredMajor = requiredJavaMajor(versionId);
    
    // Find best matching Java: prefer manual selection, fallback to auto-match
    QString javaPath;
    QString manualJava = m_settings->javaPath();

    if (!manualJava.isEmpty() && QFileInfo::exists(manualJava)) {
        javaPath = manualJava;
        emit logMessage(tr("[完成] 使用已配置的 Java: %1").arg(javaPath));
    } else {
        javaPath = m_settings->findJavaForVersion(requiredMajor);
        if (!javaPath.isEmpty()) {
            emit logMessage(tr("[完成] 已自动匹配 Java %1: %2").arg(requiredMajor).arg(javaPath));
        }
    }

    if (javaPath.isEmpty()) {
        // Check if scan is still in progress (async, takes ~500ms)
        if (m_settings->isJavaScanning()) {
            emit logMessage(QStringLiteral("[JAVA] Java scan still in progress, please wait and try again"));
            emit launchBlocked(tr("Java 扫描进行中，请稍后再试"));
        } else {
            emit logMessage(tr("[失败] 此版本需要 Java %1，但系统中未找到匹配的 Java 安装")
                                .arg(requiredMajor));
            emit logMessage(tr("[提示] 请安装 Java %1 后在设置中手动选择").arg(requiredMajor));
            emit launchBlocked(tr("Java未安装！无法启动游戏！"));
        }
        return;
    }

    // Pass auth info based on which tab the user launched from
    if (online) {
        qCInfo(logLaunch) << QStringLiteral("[认证] 在线模式启动 玩家名=%1 uuid=%2").arg(m_account->username(), m_account->accountUuid());
        m_launch->setAuthInfo(m_account->username(), m_account->accountUuid(),
                              m_account->mcToken(), true);
    } else {
        qCInfo(logLaunch) << QStringLiteral("[认证] 离线模式启动 玩家名=%1 uuid=%2").arg(m_account->offlineUsername(), m_account->offlineUuid());
        m_launch->setAuthInfo(m_account->offlineUsername(), m_account->offlineUuid(), QString(), false);
    }

    m_launch->setAutoLangMode(m_settings->autoLangMode());
    m_launch->launch(versionId, username, javaPath, maxMemory, jvmArgs, gameArgs, highPerfGpu);
}

void ShadowBackend::cancelLaunch() {
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
    int offset, int limit) {
    QStringList versions;
    if (!gameVersion.isEmpty())
        versions << gameVersion;
    m_resource->searchModsEx(query, loader, category, versions,
                             environment, license, offset, limit);
}

QVariantMap ShadowBackend::getModCategories() {
    return m_resource->getModCategories();
}

void ShadowBackend::searchShadersEx(const QString& query, const QStringList& gameVersions,
    const QStringList& categories, const QStringList& performance,
    const QStringList& loader, int offset, int limit) {
    m_resource->searchShadersEx(query, gameVersions, categories, performance, loader, offset, limit);
}

void ShadowBackend::downloadMod(const QString& slug, const QString& gameVersion, const QString& minecraftDir) {
    m_resource->downloadMod(slug, gameVersion, minecraftDir);
}

void ShadowBackend::downloadShader(const QString& slug, const QString& gameVersion, const QString& minecraftDir) {
    m_resource->downloadShader(slug, gameVersion, minecraftDir);
}

void ShadowBackend::searchResourcepacks(const QString& query, const QString& gameVersion, int offset, const QStringList& categories) {
    m_resource->searchResourcepacks(query, gameVersion, offset, categories);
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

// ── Mod file download proxy ──
int ShadowBackend::downloadModFile(const QString& url, const QString& savePath,
                                    const QString& displayName, qint64 expectedSize,
                                    const QString& sha1, qint64 receivedOffset, int resumeId) {
    return m_resource->downloadModFile(url, savePath, displayName, expectedSize, sha1, receivedOffset, resumeId);
}
void ShadowBackend::cancelModFileDownload(int downloadId) {
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

void ShadowBackend::setGameDir(const QString& dir) {
    m_app->setGameDir(dir);
    // Sync all backends to the new directory
    m_version->setGameDir(dir);
    m_settings->setMinecraftDir(dir);
    m_settings->setIsolationGameDir(dir);
    m_localMods->setGameDir(dir);
    m_launch->setGameDir(dir);
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

void ShadowBackend::queryForgeVersions(const QString& mcVersion) {
    m_modLoaderQueriesCancelled = false;  // fresh install page entry
    QString url = QStringLiteral("https://bmclapi2.bangbang93.com/forge/minecraft/") + mcVersion;
    queryModLoaderApi(this, url, "Forge",
        [](const QByteArray& data) -> QVariantList {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isArray()) return {};
            QJsonArray arr = doc.array();
            QVariantList list;
            for (const QJsonValue& v : arr) {
                QJsonObject obj = v.toObject();
                QString ver = obj.value("version").toString();
                if (ver.isEmpty()) continue;
                QString date = obj.value("modified").toString().left(10);
                // Extract installer SHA1 from files[category=="installer"]
                QString installerSha1;
                if (obj.contains("files")) {
                    for (const QJsonValue& fv : obj["files"].toArray()) {
                        QJsonObject f = fv.toObject();
                        if (f.value("category").toString() == QStringLiteral("installer")) {
                            installerSha1 = f.value("hash").toString();
                            break;
                        }
                    }
                }
                QVariantMap m;
                m["version"] = ver;
                m["type"] = QStringLiteral("release");
                m["date"] = date;
                if (!installerSha1.isEmpty())
                    m["installerSha1"] = installerSha1;
                list.append(m);
            }
            return list;
        },
        [this](const QVariantList& list) { emit forgeVersionsReady(list); });
}

void ShadowBackend::queryFabricVersions(const QString& mcVersion) {
    QString url = QStringLiteral("https://bmclapi2.bangbang93.com/fabric-meta/v2/versions/loader/") + mcVersion;
    queryModLoaderApi(this, url, "Fabric",
        [](const QByteArray& data) -> QVariantList {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonArray arr;
            if (doc.isArray()) {
                arr = doc.array();
            } else if (doc.isObject()) {
                arr = doc.object().value("versions").toArray();
            } else return {};
            QVariantList list;
            for (const QJsonValue& v : arr) {
                QJsonObject obj = v.toObject();
                QJsonObject loader = obj.value("loader").toObject();
                QString ver = loader.value("version").toString();
                if (ver.isEmpty()) continue;
                bool stable = loader.value("stable").toBool(true);
                QVariantMap m;
                m["version"] = ver;
                m["type"] = stable ? QStringLiteral("release") : QStringLiteral("beta");
                m["date"] = QString();
                list.append(m);
            }
            return list;
        },
        [this](const QVariantList& list) { emit fabricVersionsReady(list); });
}

void ShadowBackend::queryNeoForgeVersions(const QString& mcVersion) {
    QString url = QStringLiteral("https://bmclapi2.bangbang93.com/maven/net/neoforged/neoforge/maven-metadata.xml");
    queryModLoaderApi(this, url, "NeoForge",
        [mcVersion](const QByteArray& data) -> QVariantList {
            QVariantList list;
            QString xml = QString::fromUtf8(data);
            QStringList mcParts = mcVersion.split('.');
            QString neoPrefix;
            // MC version "1.21.1" → NeoForge prefix "21.1" (drop leading "1.")
            // MC version "26.2"   → NeoForge prefix "26.2" (no leading "1.")
            if (mcParts.size() >= 2 && mcParts.at(0) == QStringLiteral("1")) {
                mcParts.removeFirst();
                neoPrefix = mcParts.join('.');
            } else {
                neoPrefix = mcVersion;
            }
            QString neoPrefixDot = neoPrefix + QStringLiteral(".");

            QRegularExpression re("<version>([^<]+)</version>");
            QRegularExpressionMatchIterator it = re.globalMatch(xml);
            while (it.hasNext()) {
                QRegularExpressionMatch match = it.next();
                QString ver = match.captured(1);
                if (!ver.startsWith(neoPrefixDot)) continue;  // Match full prefix e.g. "21.1."
                QVariantMap m;
                m["version"] = ver;
                m["type"] = QStringLiteral("release");
                m["date"] = QString();
                list.append(m);
            }
            return list;
        },
        [this](const QVariantList& list) { emit neoforgeVersionsReady(list); });
}

void ShadowBackend::queryOptifineVersions(const QString& mcVersion) {
    QString url = QStringLiteral("https://bmclapi2.bangbang93.com/optifine/") + mcVersion;
    queryModLoaderApi(this, url, "Optifine",
        [mcVersion](const QByteArray& data) -> QVariantList {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isArray()) return {};
            QJsonArray arr = doc.array();
            QVariantList list;
            for (const QJsonValue& v : arr) {
                QJsonObject obj = v.toObject();
                QString fn = obj.value("filename").toString();
                if (fn.isEmpty()) continue;
                QString prefix = QStringLiteral("OptiFine_") + mcVersion + QStringLiteral("_");
                QString ver = fn;
                if (fn.startsWith(prefix)) ver = fn.mid(prefix.length());
                if (ver.endsWith(".jar")) ver = ver.left(ver.length() - 4);
                QVariantMap m;
                m["version"] = ver;
                m["type"] = QStringLiteral("release");
                m["date"] = QString();
                // Preserve BMCLAPI type/patch for correct download URL construction
                m["bmclType"] = obj.value("type").toString();
                m["bmclPatch"] = obj.value("patch").toString();
                // Forge compatibility: "Forge 61.0.8", "Forge N/A", or empty
                m["forge"] = obj.value("forge").toString();
                list.append(m);
            }
            return list;
        },
        [this](const QVariantList& list) { emit optifineVersionsReady(list); });
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

bool ShadowBackend::installFabricApi(const QString& version, const QString& url, const QString& savePath) {
    if (url.isEmpty() || savePath.isEmpty()) return false;
    QString dir = QFileInfo(savePath).absolutePath();
    if (!dir.isEmpty()) QDir().mkpath(dir);
    qDebug() << "[FabricApi] installing" << version << "to" << savePath;
    m_resource->downloadModFile(url, savePath, QStringLiteral("Fabric API %1").arg(version),
                                0, QString(), 0, -1);
    return true;
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
    if (m_version) m_version->installModLoader(mcVersion, loaderType, loaderVersion, installName,
                                                 fabricApiVersion, fabricApiUrl, fabricApiSavePath, sha1);
}

void ShadowBackend::installOptifine(const QString& mcVersion, const QString& optifineVersion,
                                     const QString& forgeVersion, const QString& installName,
                                     const QString& bmclType, const QString& bmclPatch) {
    qDebug() << "[install] ShadowBackend::installOptifine" << mcVersion << optifineVersion << forgeVersion << installName;
    if (m_version) m_version->installOptifine(mcVersion, optifineVersion, forgeVersion, installName, bmclType, bmclPatch);
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

    // Beta key verification endpoint — decrypted from opaque flat blob.
    // Without local encrypted_addr_local.h, all kBlob entries are zero → disabled.
    static const QString kWorkerUrl = []() -> QString {
        QByteArray plain = aesGcmDecrypt(
            kBlob + kWorkerNonceOff, kWorkerNonceLen,
            kBlob + kWorkerCTOff,   kWorkerCTLen,
            kBlob + kWorkerTagOff,  kWorkerTagLen,
            kBlob + kWorkerKeyOff,  kBlob + kWorkerSaltOff);
        if (plain.isEmpty()) return {};
        return QString::fromUtf8(plain);
    }();

    if (kWorkerUrl.isEmpty()) {
        qCInfo(logApp) << QStringLiteral("[BetaKey] 验证跳过 未配置Worker URL");
        m_betaStatus = QStringLiteral("disabled");
        emit betaStatusChanged();
        return;
    }

    auto* mgr = new QNetworkAccessManager(this);
    QNetworkRequest req{QUrl(kWorkerUrl)};
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
                          CRYPTPROTECT_LOCAL_MACHINE, &out)) {
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
    static const QString kWorkerUrl = []() -> QString {
        QByteArray plain = aesGcmDecrypt(
            kBlob + kWorkerNonceOff, kWorkerNonceLen,
            kBlob + kWorkerCTOff,   kWorkerCTLen,
            kBlob + kWorkerTagOff,  kWorkerTagLen,
            kBlob + kWorkerKeyOff,  kBlob + kWorkerSaltOff);
        if (plain.isEmpty()) return {};
        return QString::fromUtf8(plain);
    }();

    if (kWorkerUrl.isEmpty()) {
        if (outError) *outError = QStringLiteral("内测验证未配置");
        return false;
    }

    QNetworkAccessManager mgr;
    QNetworkRequest req{QUrl(kWorkerUrl)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

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
                        "https://gitee.com/api/v5/repos/xiaole1173/shadow-launcher/releases/latest");
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

