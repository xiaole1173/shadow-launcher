#include "shadow_backend.h"
#include "../core/http_client.h"
#include "../core/mod_manager.h"
#include "../core/version_downloader.h"
#include "../core/version_isolation.h"
#include "../utils/logger.h"
#include <QElapsedTimer>
#include "account_backend.h"
#include "app_backend.h"
#include "check_backend.h"
#include "launch_backend.h"
#include "resource_backend.h"
#include "settings_backend.h"
#include "version_backend.h"

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
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QProcess>
#include <QRegularExpression>
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

namespace ShadowLauncher {

ShadowBackend::ShadowBackend(QObject* parent)
    : QObject(parent)
{
    QElapsedTimer bt; bt.start();
    auto bp = [&bt](const char* label) {
        qCInfo(logApp) << "[BACKEND +" << bt.elapsed() << "ms]" << label;
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

    // ── Sync game directories: ALL backends use the same path ──
    m_version->setGameDir(m_app->gameDir());
    m_settings->setMinecraftDir(m_app->gameDir());
    m_settings->setIsolationGameDir(m_app->gameDir());
    m_launch->setGameDir(m_app->gameDir());
    m_launch->setAccount(m_account);
    m_version->setIsolation(m_settings->isolation());

    bp("Signal wiring...");
    connect(m_account, &AccountBackend::accountChanged,
            this, &ShadowBackend::accountChanged);
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
                    qCInfo(logLaunch) << "Restored last selected version:" << last;
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
            this, &ShadowBackend::verifyFinished);
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
            // Auto-restore login on startup
            QTimer::singleShot(100, this, [this, lastUser]() {
                m_account->offlineLogin(lastUser);
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
                qCDebug(logLaunch) << "[RP-DOWNLOAD] stateChanged downloading=" << m_resource->isDownloading();
                emit resourceDownloadStateChanged();
            });
    connect(m_resource, &ResourceBackend::downloadProgressChanged,
            this, [this](int completed, int total, const QString& fileName) {
                m_resourceDlProgress = completed;
                m_resourceDlTotal = total;
                m_resourceDlFile = fileName;
                qCDebug(logLaunch) << "[RP-DOWNLOAD] progress" << completed << "/" << total << fileName;
                emit resourceDownloadProgress(completed, total, fileName);
                if (m_version && total > 0) {
                    m_version->updateResourceCard(QStringLiteral("resource"),
                        (qreal)completed / total, fileName);
                }
            });
    connect(m_resource, &ResourceBackend::downloadFinished,
            this, [this](const QString&, bool success, const QString&) {
                qCDebug(logLaunch) << "[RP-DOWNLOAD] completed success=" << success;
                emit resourceDownloadDone(success);
                if (m_version) m_version->removeResourceCard(QStringLiteral("resource"));
            });
    connect(m_resource, &ResourceBackend::searchResultsReady,
            this, &ShadowBackend::searchResultsReady);
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

    bp("Constructor done");
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

bool ShadowBackend::closeAfterLaunch() const {
    return m_settings->closeAfterLaunch();
}

bool ShadowBackend::isolationEnabled() const {
    return m_isolationEnabled;
}

bool ShadowBackend::embeddedLoginEnabled() const {
    return m_settings->embeddedLoginEnabled();
}

void ShadowBackend::setEmbeddedLoginEnabled(bool v) {
    m_settings->setEmbeddedLoginEnabled(v);
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
    return m_version && (m_version->installPhase() == QStringLiteral("verifying")
                        || m_version->installPhase() == tr("校验中..."));
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

QStringList ShadowBackend::snapshotVersions() const {
    QStringList list;
    auto versions = m_version->cachedMcVersions();
    for (const auto& v : versions) {
        if (v.type == QStringLiteral("snapshot")) list.append(v.id);
    }
    return list;
}

QStringList ShadowBackend::oldVersions() const {
    QStringList list;
    auto versions = m_version->cachedMcVersions();
    for (const auto& v : versions) {
        if (v.type != QStringLiteral("release") && v.type != QStringLiteral("snapshot"))
            list.append(v.id);
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

        // Version type from manifest or heuristic
        QString vtype = knownTypes.value(versionId, QString());
        if (vtype.isEmpty()) {
            // Heuristic: versions containing 'w' followed by number are snapshots
            if (versionId.contains(QRegularExpression(QStringLiteral("\\dw\\d|alpha|beta|inf"))))
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

        // Release time (Unix ms) for sorting
        QDateTime rt = releaseTimes.value(versionId);
        detail[QStringLiteral("releaseTimeMs")] = rt.isValid() ? rt.toMSecsSinceEpoch() : 0;

        // Detect mod loader
        QString loaderType = tr("原版");
        QString loaderVersion;
        // Check for Forge
        QString forgeDir = verPath + QStringLiteral("/mods");
        if (QDir(forgeDir).exists()) {
            // Check for Fabric
            QString fabricJson = verPath + QStringLiteral("/fabric-installer.json");
            if (QFileInfo::exists(fabricJson)) {
                loaderType = QStringLiteral("Fabric");
            } else {
                loaderType = QStringLiteral("Forge");
            }
        }
        // Check for NeoForge
        if (QFileInfo::exists(verPath + QStringLiteral("/neoforge"))) {
            loaderType = QStringLiteral("NeoForge");
        }
        // Check for Quilt
        if (QFileInfo::exists(verPath + QStringLiteral("/quilt"))) {
            loaderType = QStringLiteral("Quilt");
        }
        detail[QStringLiteral("loaderType")] = loaderType;
        detail[QStringLiteral("loaderVersion")] = loaderVersion;

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

bool ShadowBackend::devMode() const {
    return m_app->devMode();
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
    return m_settings->scanJavaInstallations();
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
    emit jvmArgsChanged();
}

QString ShadowBackend::browseJava() {
    return m_settings->browseJava();
}

void ShadowBackend::selectJavaByIndex(int index) {
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

void ShadowBackend::installVersion(const QString& versionId, int sourceIndex) {
    m_version->installVersion(versionId, sourceIndex);
}

QVariantList ShadowBackend::getMirrorSources() {
    QVariantList result;
    const auto mirrors = MirrorSource::allMirrors();
    for (int i = 0; i < mirrors.size(); ++i) {
        const auto& m = mirrors[i];
        QVariantMap item;
        item[QStringLiteral("index")] = i;
        item[QStringLiteral("name")] = m.name;
        item[QStringLiteral("desc")] = m.desc;
        item[QStringLiteral("isDefault")] = m.isDefault;
        item[QStringLiteral("attribution")] = m.attribution;
        item[QStringLiteral("isAvailable")] = m.isAvailable;
        result.append(item);
    }
    return result;
}

QString ShadowBackend::bmclapiComplianceNotice() {
    return QString::fromUtf8(kBMCLAPIComplianceNotice);
}

void ShadowBackend::cancelInstall() {
    m_version->cancelInstall();
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
    QString username = m_account->username();
    int maxMemory = m_settings->maxMemoryMB();
    QString jvmArgs = m_app->jvmArgs();
    m_launchVersion = versionId;
    m_launchUsername = username;

    // Determine required Java version from version JSON
    int requiredMajor = requiredJavaMajor(versionId);
    
    // Find best matching Java
    QString javaPath = m_settings->findJavaForVersion(requiredMajor);
    
    if (javaPath.isEmpty()) {
        // Check if scan is still in progress (async, takes ~500ms)
        if (m_settings->isJavaScanning()) {
            emit logMessage(QStringLiteral("[JAVA] Java scan still in progress, please wait and try again"));
        } else {
            emit logMessage(tr("❌ 此版本需要 Java %1，但系统中未找到匹配的 Java 安装")
                                .arg(requiredMajor));
            emit logMessage(tr("👉 请安装 Java %1 后在设置中手动选择").arg(requiredMajor));
        }
        return;
    }

    // Pass auth info based on which tab the user launched from
    if (online) {
        qCInfo(logLaunch) << "[AUTH] Launching with ONLINE auth: username=" << m_account->username() << " uuid=" << m_account->accountUuid();
        m_launch->setAuthInfo(m_account->username(), m_account->accountUuid(),
                              m_account->mcToken(), true);
    } else {
        qCInfo(logLaunch) << "[AUTH] Launching with OFFLINE auth: username=" << m_account->offlineUsername() << " uuid=" << m_account->offlineUuid();
        m_launch->setAuthInfo(m_account->offlineUsername(), m_account->offlineUuid(), QString(), false);
    }

    m_launch->launch(versionId, username, javaPath, maxMemory, jvmArgs);
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

void ShadowBackend::searchShaders(const QString& query, const QString& gameVersion) {
    m_resource->searchShaders(query, gameVersion);
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
            qCWarning(logApp) << "[ICON] download failed:" << webpUrl.left(100) << reply->errorString();
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
                qCWarning(logApp) << "[ICON] decode failed:" << webpUrl.left(100);
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
            qCWarning(logApp) << "[ICON] PNG save failed:" << pngPath;
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

QString ShadowBackend::copyVersionPath(const QString& versionId) {
    return m_version->copyVersionPath(versionId);
}

void ShadowBackend::repairVersion(const QString& versionId) {
    m_version->repairVersion(versionId);
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
    auto* mgr = new QNetworkAccessManager(self);
    QUrl qurl(url);
    QNetworkRequest req(qurl);
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = mgr->get(req);
    qCDebug(logApp) << "[ModLoader] querying" << loaderName << url;

    QObject::connect(reply, &QNetworkReply::finished, self, [self, reply, mgr, loaderName, parseFn, emitFn]() {
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(logApp) << "[ModLoader]" << loaderName << "query failed:" << reply->errorString();
            reply->deleteLater();
            mgr->deleteLater();
            emitFn({});
            return;
        }
        QByteArray data = reply->readAll();
        reply->deleteLater();
        mgr->deleteLater();
        qCDebug(logApp) << "[ModLoader]" << loaderName << "got" << data.size() << "bytes";
        QVariantList list = parseFn(data);
        qCDebug(logApp) << "[ModLoader]" << loaderName << "parsed" << list.size() << "versions";
        emitFn(list);
    });
}

void ShadowBackend::queryForgeVersions(const QString& mcVersion) {
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
                QVariantMap m;
                m["version"] = ver;
                m["type"] = QStringLiteral("release");
                m["date"] = date;
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
            if (status == 200 && !body.isEmpty()) {
                QVariantList list = parseResponse(body);
                qCDebug(logApp) << "[FabricApi] MCIM got" << list.size() << "versions";
                emit fabricApiVersionsReady(list);
                return;
            }
            qCWarning(logApp) << "[FabricApi] MCIM failed (status:" << status
                               << "), falling back to Modrinth direct";
            HttpClient::instance().get(fallbackUrl,
                [this, parseResponse](int status2, const QByteArray& body2) {
                    if (status2 == 200 && !body2.isEmpty()) {
                        QVariantList list = parseResponse(body2);
                        qCDebug(logApp) << "[FabricApi] direct got" << list.size() << "versions";
                        emit fabricApiVersionsReady(list);
                    } else {
                        qCWarning(logApp) << "[FabricApi] direct also failed (status:" << status2 << ")";
                        emit fabricApiVersionsReady({});
                    }
                });
        });
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
                                      const QString& loaderVersion, const QString& installName) {
    qDebug() << "[install] ShadowBackend::installModLoader" << loaderType << mcVersion << loaderVersion << installName;
    if (m_version) m_version->installModLoader(mcVersion, loaderType, loaderVersion, installName);
}

} // namespace ShadowLauncher
