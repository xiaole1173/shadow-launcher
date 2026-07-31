// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173

#include "modpack_install_task.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>
#include <QtConcurrent>
#include <QDateTime>
#include <QUuid>

#include "modpack_parser.h"
#include "modpack_downloader.h"
#include "zip_archive.h"
#include "../http_client.h"
#include "../../backend/version_backend.h"
#include "../version_isolation.h"
#include "../version_manager.h"
#include "../../utils/logger.h"

#include <algorithm>
#include <functional>

using namespace ShadowLauncher;

namespace {

// 进度权重（主流启动器 ProgressWeight 思路）：解析 5% / 解压 10% / 下载 55% / MC安装 25% / 收尾 5%
constexpr qreal kParseWeight = 0.05;
constexpr qreal kExtractWeight = 0.10;
constexpr qreal kDownloadWeight = 0.55;
constexpr qreal kMcInstallWeight = 0.25;
constexpr qreal kFinalizeWeight = 0.05;

} // namespace

// ── 构造 / 析构 ──

ModpackInstallTask::ModpackInstallTask(QObject* parent)
    : QObject(parent)
    , m_downloader(new ModpackDownloader(this))
{
}

ModpackInstallTask::~ModpackInstallTask() = default;

void ModpackInstallTask::configure(VersionBackend* vb, VersionIsolation* iso,
                                   const QString& gameDir, const QString& cfApiKey)
{
    m_vb = vb;
    m_iso = iso;
    m_gameDir = gameDir;
    m_cfApiKey = cfApiKey;
    m_downloader->setApiKey(cfApiKey);
}

// ── 启动 ──

void ModpackInstallTask::start(const QString& zipPath, bool includeOptional)
{
    if (m_busy) return;
    if (m_vb->isInstalling()) {
        fail(tr("已有版本安装任务正在进行，请等待完成后再导入整合包"));
        return;
    }

    m_busy = true;
    m_cancel = false;
    m_zipPath = zipPath;
    m_includeOptional = includeOptional;
    m_meta = ModpackMeta{};
    m_targetName.clear();
    m_versionDir.clear();
    m_resourceDir.clear();
    m_versionDirCreated = false;
    m_createdFiles.clear();
    m_overwriteBackup.clear();
    m_installingMc = false;
    m_installedVanillaId.clear();
    m_vanillaPreinstalled = false;

    if (!m_backupDir.isEmpty()) {
        QDir(m_backupDir).removeRecursively();
        m_backupDir.clear();
    }

    runParse();
}

// ── 解析（工作线程）──

void ModpackInstallTask::runParse()
{
    setStep(tr("解析整合包"));
    setProgress(kParseWeight * 0.2, tr("正在识别整合包格式…"), QFileInfo(m_zipPath).fileName());

    QtConcurrent::run([this]() {
        bool ok = false;
        QString error;

        ModpackFormat fmt = ModpackParser::detectFormat(m_zipPath);
        if (fmt == ModpackFormat::Unknown) {
            error = tr("无法识别整合包格式，请确认文件是有效的 CurseForge (.zip) 或 Modrinth (.mrpack) 压缩包");
        } else if (!ModpackParser::parse(m_zipPath, fmt, m_meta, error)) {
            // error 已填充
        } else {
            // 目标版本名：净化 + 查重（不覆盖既有版本，主流启动器 ValidateFolderName 思路）
            const QString base = sanitizeVersionName(m_meta.name + QLatin1Char('-') + m_meta.versionId);
            m_targetName = findFreeVersionName(base);
            m_versionDir = m_gameDir + QStringLiteral("/versions/") + m_targetName;
            m_versionDirCreated = !QDir(m_versionDir).exists();

            // 资源落盘目录：动态读取版本隔离开关（规格：禁止硬编码路径）
            if (m_iso) {
                if (m_iso->isEnabled()) {
                    // 隔离：{versions}/{name}/game 私有目录（与启动器 getVersionGameDir 规范一致）
                    m_resourceDir = m_versionDir + QStringLiteral("/game");
                    QDir().mkpath(m_resourceDir);
                } else {
                    // 共享：公共游戏根目录
                    m_resourceDir = m_iso->gameDir();
                    if (m_resourceDir.isEmpty()) m_resourceDir = m_gameDir;
                }
            } else {
                m_resourceDir = m_gameDir;
            }
            ok = true;
        }

        QMetaObject::invokeMethod(this, [this, ok, error]() {
            onParsed(ok, error);
        }, Qt::QueuedConnection);
    });
}

void ModpackInstallTask::onParsed(bool ok, const QString& error)
{
    if (!ok) {
        fail(error);
        return;
    }
    if (m_cancel) {
        rollback();
        finishCancelled();
        return;
    }

    emit logLine(tr("整合包「%1」 版本 %2  MC %3  加载器 %4 %5")
        .arg(m_meta.name, m_meta.versionId, m_meta.mcVersion,
             m_meta.loaderType.isEmpty() ? tr("无") : m_meta.loaderType,
             m_meta.loaderVersion));
    if (!m_meta.summary.isEmpty())
        emit logLine(tr("注意：%1").arg(m_meta.summary));
    emit logLine(tr("目标版本目录: %1").arg(m_versionDir));
    emit logLine(tr("资源写入目录（版本隔离%1）: %2")
        .arg(m_iso && m_iso->isEnabled() ? tr("开启") : tr("关闭"), m_resourceDir));

    // 通知 UI 模组列表
    emit modItemsChanged();

    runExtract();
}

// ── 解压 overrides（工作线程）──

void ModpackInstallTask::runExtract()
{
    setStep(tr("解压整合包资源"));
    emit logLine(tr("开始解压覆写目录…"));

    const QStringList dirs = m_meta.overrideDirs;
    const QString resourceDir = m_resourceDir;
    const bool hasAny = std::any_of(dirs.begin(), dirs.end(),
                                    [](const QString& d) { return !d.isEmpty(); });

    if (!hasAny) {
        // 无覆写目录也要确保资源目录存在（后续模组下载落盘）
        QDir().mkpath(resourceDir);
        onExtracted(0);
        return;
    }

    QtConcurrent::run([this, dirs, resourceDir]() {
        ZipArchive zip;
        int totalExtracted = 0;
        bool fatal = false;
        QString fatalError;

        if (!zip.open(m_zipPath)) {
            fatal = true;
            fatalError = zip.error();
        } else {
            for (const QString& dir : dirs) {
                if (m_cancel) break;
                if (dir.isEmpty()) continue;  // CF overrides="." 已归一为空 → 整包

                const QString prefix = sanitizeRelPath(dir);
                const int n = zip.extractPrefixTo(
                    prefix, resourceDir, &m_cancel,
                    [this](int done, int total) {
                        if (total > 0)
                            setProgress(kParseWeight + kExtractWeight * (0.2 + 0.8 * (qreal)done / total),
                                        tr("正在解压资源 (%1/%2)…").arg(done).arg(total));
                    },
                    [this](const QString& destPath, bool existed) {
                        if (existed) {
                            registerOverwrite(destPath);
                        } else {
                            registerCreatedFile(destPath);
                        }
                    });
                if (n == -1) {
                    fatal = true;
                    fatalError = tr("解压整合包失败（文件损坏或磁盘错误）");
                    break;
                }
                if (n == -2) break;  // 取消
                totalExtracted += qMax(0, n);
            }
        }

        QMetaObject::invokeMethod(this, [this, fatal, fatalError, totalExtracted]() {
            if (fatal) {
                fail(fatalError);
                return;
            }
            if (m_cancel) {
                rollback();
                finishCancelled();
                return;
            }
            onExtracted(totalExtracted);
        }, Qt::QueuedConnection);
    });
}

void ModpackInstallTask::onExtracted(int fileCount)
{
    emit logLine(fileCount > 0
        ? tr("覆写资源解压完成，共 %1 个文件").arg(fileCount)
        : tr("整合包不含覆写资源，跳过解压"));
    runDownload();
}

// ── 模组批量下载 ──

void ModpackInstallTask::runDownload()
{
    if (m_meta.files.isEmpty()) {
        emit logLine(tr("整合包不含远端模组，跳过下载"));
        onDownloadAllFinished(false);
        return;
    }

    setStep(tr("下载模组"));
    setProgress(kParseWeight + kExtractWeight + 0.01, tr("准备下载 %1 个文件…").arg(m_meta.files.size()));

    // 一次性接线（每个任务生命周期只 start 一次）
    disconnect(m_downloader, nullptr, this, nullptr);

    connect(m_downloader, &ModpackDownloader::statusChanged, this, [this](const QString& t) {
        m_statusText = t;
        emit progressChanged(m_progress, t, m_currentFile);
    });
    connect(m_downloader, &ModpackDownloader::fileProgress, this,
            [this](int index, const QString& name, qint64 received, qint64 total) {
        Q_UNUSED(index);
        m_currentFile = name;
        emit fileProgressChanged(name, total > 0 ? (qreal)received / total : 0.0);
    });
    connect(m_downloader, &ModpackDownloader::fileFinished, this,
            [this](int index, bool success, const QString& error) {
                Q_UNUSED(success); Q_UNUSED(error);
                Q_UNUSED(index);
                emit modItemsChanged();
            });
    connect(m_downloader, &ModpackDownloader::queueProgress, this,
            [this](int completed, int total, int failed) {
                if (total <= 0) return;
                const qreal base = kParseWeight + kExtractWeight;
                const qreal frac = (qreal)completed / total;
                const QString text = failed > 0
                    ? tr("正在下载模组 (%1/%2，%3 个失败)…").arg(completed).arg(total).arg(failed)
                    : tr("正在下载模组 (%1/%2)…").arg(completed).arg(total);
                setProgress(base + kDownloadWeight * frac, text, m_currentFile);
            });
    connect(m_downloader, &ModpackDownloader::logLine, this, &ModpackInstallTask::logLine);
    connect(m_downloader, &ModpackDownloader::allFinished, this, &ModpackInstallTask::onDownloadAllFinished);

    m_downloader->setTargetDir(m_resourceDir);
    m_downloader->setFiles(&m_meta.files);
    m_downloader->setOverwriteHook([this](const QString& savePath) {
        registerOverwrite(savePath);
    });
    m_downloader->start(m_includeOptional);
}

void ModpackInstallTask::onDownloadAllFinished(bool cancelled)
{
    if (cancelled) {
        rollback();
        finishCancelled();
        return;
    }

    // 统计失败
    int failed = 0;
    QString firstError;
    for (const ModpackRemoteFile& rf : m_meta.files) {
        if (rf.status == QLatin1String("fail")) {
            ++failed;
            if (firstError.isEmpty()) firstError = rf.error;
        }
    }
    const int total = m_meta.files.size();

    if (failed == total && total > 0) {
        // 全部失败：视为导入失败（如 CF 密钥 401/403、网络完全不可用）
        fail(tr("全部 %1 个文件下载失败，首个错误: %2").arg(total).arg(firstError));
        return;
    }
    if (failed > 0) {
        emit logLine(tr("⚠ %1/%2 个文件下载失败（已跳过），整合包可能不完整: %3")
            .arg(failed).arg(total).arg(firstError));
    }

    runMcInstall();
}

// ── MC + 加载器安装（复用 VersionBackend merged 流程）──

void ModpackInstallTask::runMcInstall()
{
    setStep(tr("安装游戏本体与加载器"));
    setProgress(kParseWeight + kExtractWeight + kDownloadWeight + 0.02,
                tr("正在安装 Minecraft %1…").arg(m_meta.mcVersion));

    m_installingMc = true;

    if (!m_meta.loaderType.isEmpty() && !m_meta.loaderVersion.isEmpty()) {
        // 带加载器：merged 安装（MC + 加载器一体）
        emit logLine(tr("安装 Minecraft %1 + %2 %3 → %4")
            .arg(m_meta.mcVersion, m_meta.loaderType, m_meta.loaderVersion, m_targetName));

        connect(m_vb, &VersionBackend::installFinished, this, [this](bool ok) {
            // 只处理本任务触发的安装
            if (!m_installingMc) return;
            m_installingMc = false;
            onMcInstallFinished(ok);
        }, Qt::UniqueConnection);

        m_vb->installModLoader(m_meta.mcVersion, m_meta.loaderType,
                               m_meta.loaderVersion, m_targetName);
        return;
    }

    // 纯原版整合包：安装 vanilla 后改为目标名
    emit logLine(tr("整合包未指定加载器，按纯原版处理（安装 %1）").arg(m_meta.mcVersion));

    const bool preinstalled = m_vb->versionManager() && m_vb->versionManager()->isInstalled(m_meta.mcVersion);
    m_vanillaPreinstalled = preinstalled;

    connect(m_vb, &VersionBackend::installFinished, this, [this](bool ok) {
        if (!m_installingMc) return;
        m_installingMc = false;
        onMcInstallFinished(ok);
    }, Qt::UniqueConnection);

    if (preinstalled) {
        // 本机已有该 vanilla 版本：复制版本目录 → 目标名（不动原版本）
        m_installedVanillaId = m_meta.mcVersion;
        QTimer::singleShot(0, this, [this]() {
            m_installingMc = false;
            onMcInstallFinished(true);
        });
    } else {
        m_installedVanillaId = m_meta.mcVersion;
        m_vb->installVersion(m_meta.mcVersion);
    }
}

void ModpackInstallTask::onMcInstallFinished(bool ok)
{
    if (m_cancel) {
        // 取消等待安装结束 → 直接回滚
        rollback();
        finishCancelled();
        return;
    }
    if (!ok) {
        fail(tr("Minecraft %1 安装失败").arg(m_meta.mcVersion));
        return;
    }

    // 纯原版 + 非预装：把 vanilla 版本目录改名为目标名
    if (m_meta.loaderType.isEmpty() && !m_vanillaPreinstalled
        && !m_installedVanillaId.isEmpty() && m_installedVanillaId != m_targetName) {
        emit logLine(tr("将版本 %1 重命名为 %2").arg(m_installedVanillaId, m_targetName));
        if (!m_vb->renameVersion(m_installedVanillaId, m_targetName)) {
            fail(tr("版本重命名失败: %1 → %2").arg(m_installedVanillaId, m_targetName));
            return;
        }
    }

    // 纯原版 + 预装：复制版本目录（含 game 子目录与 json id 改写），工作线程执行
    if (m_vanillaPreinstalled && !m_installedVanillaId.isEmpty()) {
        const QString src = m_vb->versionManager()->getVersionPath(m_installedVanillaId);
        const QString dst = m_versionDir;
        const QString vanillaId = m_installedVanillaId;
        const QString targetName = m_targetName;
        emit logLine(tr("复制已安装的 %1 版本目录到 %2").arg(vanillaId, targetName));
        QtConcurrent::run([this, src, dst, vanillaId, targetName]() {
            if (!QDir(src).exists()) {
                QMetaObject::invokeMethod(this, [this]() {
                    fail(tr("原版版本目录不存在: %1").arg(m_installedVanillaId));
                }, Qt::QueuedConnection);
                return;
            }
            QDir().mkpath(dst);
            // 逐项复制（跳过下载过程标记文件）
            const QStringList entries = QDir(src).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
            for (const QString& e : entries) {
                if (e == QStringLiteral(".download_progress.json")) continue;
                const QString s = src + QLatin1Char('/') + e;
                const QString d = dst + QLatin1Char('/') + e;
                QFileInfo fi(s);
                if (fi.isDir()) {
                    // 递归复制目录
                    std::function<void(const QString&, const QString&)> copyDir;
                    copyDir = [&copyDir](const QString& from, const QString& to) {
                        QDir().mkpath(to);
                        const QStringList subs = QDir(from).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
                        for (const QString& sub : subs) {
                            const QString sf = from + QLatin1Char('/') + sub;
                            const QString df = to + QLatin1Char('/') + sub;
                            if (QFileInfo(sf).isDir()) copyDir(sf, df);
                            else QFile::copy(sf, df);
                        }
                    };
                    copyDir(s, d);
                } else {
                    QFile::copy(s, d);
                }
            }
            // 修正 json id：{vanilla}.json → {target}.json 且 id 字段改写
            const QString jsonPath = dst + QLatin1Char('/') + vanillaId + QLatin1String(".json");
            const QString newJsonPath = dst + QLatin1Char('/') + targetName + QLatin1String(".json");
            QFile jf(jsonPath);
            if (jf.exists()) {
                if (jf.open(QIODevice::ReadOnly)) {
                    QJsonDocument doc = QJsonDocument::fromJson(jf.readAll());
                    jf.close();
                    if (doc.isObject()) {
                        QJsonObject o = doc.object();
                        o[QStringLiteral("id")] = targetName;
                        QFile wf(newJsonPath);
                        if (wf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                            wf.write(QJsonDocument(o).toJson());
                            wf.close();
                        }
                    }
                }
                QFile::remove(jsonPath);
            }
            // jar 改名（findVersionJar 优先 {name}/{name}.jar）
            const QString jarPath = dst + QLatin1Char('/') + vanillaId + QLatin1String(".jar");
            if (QFileInfo::exists(jarPath))
                QFile::rename(jarPath, dst + QLatin1Char('/') + targetName + QLatin1String(".jar"));
        }).then(this, [this]() {
            if (m_cancel) {
                rollback();
                finishCancelled();
                return;
            }
            runFinalize();
        });
        return;
    }

    runFinalize();
}

// ── 收尾：核对 version.json + 注册版本列表 ──

void ModpackInstallTask::runFinalize()
{
    setStep(tr("整理版本信息"));
    setProgress(kParseWeight + kExtractWeight + kDownloadWeight + kMcInstallWeight,
                tr("正在生成版本信息…"));

    // version.json 核对：{versions}/{name}/{name}.json 必须存在
    const QString jsonPath = m_versionDir + QLatin1Char('/') + m_targetName + QLatin1String(".json");
    if (QFileInfo::exists(jsonPath)) {
        completeImport();
        return;
    }

    // 兜底：从 Mojang 清单取原版 JSON 改写 id（旧代码同思路，作为安全网保留）
    emit logLine(tr("未找到版本 JSON，尝试从原版清单补全…"));
    ensureVersionJsonFallback(0);
}

void ModpackInstallTask::ensureVersionJsonFallback(int attempt)
{
    if (attempt > 2) {
        fail(tr("版本 JSON 缺失（%1），安装流程异常").arg(m_versionDir + QLatin1Char('/') + m_targetName + QLatin1String(".json")));
        return;
    }
    if (m_cancel) {
        rollback();
        finishCancelled();
        return;
    }

    static const char* kManifestUrls[] = {
        "https://bmclapi2.bangbang93.com/mc/game/version_manifest.json",
        "https://launchermeta.mojang.com/mc/game/version_manifest.json"
    };
    const QString mcVersion = m_meta.mcVersion;
    const QString jsonPath = m_versionDir + QLatin1Char('/') + m_targetName + QLatin1String(".json");
    const QString targetName = m_targetName;
    const int idx = attempt < 2 ? attempt : 1;

    HttpClient& http = HttpClient::instance();
    http.get(QLatin1String(kManifestUrls[idx]),
             [this, &http, mcVersion, jsonPath, targetName, attempt](int status, const QByteArray& body) {
        Q_UNUSED(status);
        QJsonParseError perr;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
        QString versionUrl;
        if (perr.error == QJsonParseError::NoError) {
            const QJsonArray versions = doc.object().value(QStringLiteral("versions")).toArray();
            for (const QJsonValue& v : versions) {
                const QJsonObject o = v.toObject();
                if (o.value(QStringLiteral("id")).toString() == mcVersion) {
                    versionUrl = o.value(QStringLiteral("url")).toString();
                    break;
                }
            }
        }
        if (versionUrl.isEmpty()) {
            ensureVersionJsonFallback(attempt + 1);
            return;
        }
        http.get(versionUrl, [this, &http, jsonPath, targetName, attempt](int st2, const QByteArray& vbody) {
            Q_UNUSED(http);
            if (m_cancel) {
                rollback();
                finishCancelled();
                return;
            }
            if (st2 != 200 || vbody.isEmpty()) {
                ensureVersionJsonFallback(attempt + 1);
                return;
            }
            QJsonParseError perr2;
            QJsonDocument vdoc = QJsonDocument::fromJson(vbody, &perr2);
            if (perr2.error != QJsonParseError::NoError || !vdoc.isObject()) {
                ensureVersionJsonFallback(attempt + 1);
                return;
            }
            QJsonObject vo = vdoc.object();
            vo[QStringLiteral("id")] = targetName;
            QDir().mkpath(QFileInfo(jsonPath).absolutePath());
            QFile f(jsonPath);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                f.write(QJsonDocument(vo).toJson());
                f.close();
                emit logLine(tr("已补全版本 JSON: %1").arg(jsonPath));
                completeImport();
            } else {
                fail(tr("无法写入版本 JSON: %1").arg(jsonPath));
            }
        });
    });
}

void ModpackInstallTask::completeImport()
{
    // 注册到版本列表（QML 版本页立即可见，不再出现 versions 空白）
    if (m_vb) {
        m_vb->refreshInstalled();
        emit logLine(tr("已刷新版本列表"));
    }

    setProgress(1.0, tr("导入完成"), {});
    m_phase = Phase::Done;
    m_busy = false;
    emit stepChanged(tr("完成"));
    emit logLine(tr("✅ 整合包导入完成：%1 → 版本 %2").arg(m_meta.name, m_targetName));
    emit finished(true, m_meta.name, m_targetName);
}

// ── 取消 ──

void ModpackInstallTask::cancel()
{
    if (!m_busy) return;
    m_cancel = true;

    if (m_downloader->isRunning()) {
        m_downloader->cancel();
        return;  // onDownloadAllFinished(cancelled=true) 走回滚
    }
    if (m_installingMc && m_vb) {
        // MC/加载器安装中：调用版本后端的定向取消，随后回滚
        emit logLine(tr("正在取消版本安装…"));
        m_vb->cancelVersionInstall(m_targetName);
        QTimer::singleShot(800, this, [this]() {
            if (!m_busy) return;
            rollback();
            finishCancelled();
        });
        return;
    }
    // 解析/解压阶段：工作线程检查 m_cancel 后自行退出
    rollback();
    finishCancelled();
}

// ── 失败 / 回滚 ──

void ModpackInstallTask::fail(const QString& error)
{
    if (!m_busy && m_phase == Phase::Done) return;
    m_phase = Phase::Error;
    m_busy = false;
    m_statusText = error;
    emit logLine(tr("❌ 导入失败: %1").arg(error));
    emit progressChanged(m_progress, error, {});
    emit stepChanged(tr("失败"));
    rollback();
    emit finished(false, {}, error);
}

void ModpackInstallTask::finishCancelled()
{
    if (!m_busy) return;
    m_phase = Phase::Cancelled;
    m_busy = false;
    m_statusText = tr("已取消");
    emit logLine(tr("导入已取消，已清理本次生成的文件"));
    emit progressChanged(0.0, tr("已取消"), {});
    emit stepChanged(tr("已取消"));
    emit finished(false, {}, tr("已取消"));
}

void ModpackInstallTask::rollback()
{
    emit logLine(tr("回滚清理中…"));

    // 1. 恢复被覆盖的旧文件
    for (auto it = m_overwriteBackup.constBegin(); it != m_overwriteBackup.constEnd(); ++it) {
        QFile::remove(it.key());
        QFile::copy(it.value(), it.key());
    }
    // 2. 删除本任务创建的文件
    for (const QString& f : m_createdFiles)
        QFile::remove(f);
    // 3. 清理创建文件产生的空目录（从叶子向上，rmdir 只删空目录，安全）
    for (const QString& f : m_createdFiles) {
        QDir dir = QFileInfo(f).absoluteDir();
        while (dir.absolutePath().startsWith(m_resourceDir)
               && dir.absolutePath() != m_resourceDir
               && dir.absolutePath() != m_gameDir) {
            if (!dir.rmdir(dir.absolutePath())) break;
            dir.cdUp();
        }
    }
    // 4. 版本目录整体删除（仅本任务创建且无玩家存档时）
    if (m_versionDirCreated && !m_versionDir.isEmpty() && QDir(m_versionDir).exists()) {
        const QString saves = m_versionDir + QStringLiteral("/game/saves");
        const QString saves2 = m_versionDir + QStringLiteral("/saves");
        const bool hasSaves = QDir(saves).exists() || QDir(saves2).exists();
        if (hasSaves) {
            emit logLine(tr("检测到玩家存档，保留版本目录: %1").arg(m_versionDir));
        } else {
            QDir(m_versionDir).removeRecursively();
            emit logLine(tr("已删除版本目录: %1").arg(m_versionDir));
        }
    }
    // 5. 备份目录
    if (!m_backupDir.isEmpty()) {
        QDir(m_backupDir).removeRecursively();
        m_backupDir.clear();
    }
    emit modItemsChanged();
}

// ── 工具 ──

QString ModpackInstallTask::findFreeVersionName(const QString& base) const
{
    if (m_gameDir.isEmpty()) return base;
    const QString dir = m_gameDir + QStringLiteral("/versions/");
    if (!QDir(dir + base).exists()) return base;
    for (int i = 2; i <= 999; ++i) {
        const QString cand = base + QStringLiteral("-%1").arg(i);
        if (!QDir(dir + cand).exists()) return cand;
    }
    return base + QStringLiteral("-%1").arg(QDateTime::currentMSecsSinceEpoch());
}

void ModpackInstallTask::setStep(const QString& name)
{
    m_stepName = name;
    emit stepChanged(name);
}

void ModpackInstallTask::setProgress(qreal p, const QString& text, const QString& file)
{
    m_progress = qBound(0.0, p, 1.0);
    m_statusText = text;
    if (!file.isNull()) m_currentFile = file;
    emit progressChanged(m_progress, text, m_currentFile);
}

void ModpackInstallTask::log(const QString& msg)
{
    emit logLine(msg);
}

void ModpackInstallTask::registerCreatedFile(const QString& path)
{
    if (!m_createdFiles.contains(path))
        m_createdFiles.insert(path);
}

void ModpackInstallTask::registerOverwrite(const QString& path)
{
    if (m_overwriteBackup.contains(path)) return;
    if (m_backupDir.isEmpty()) {
        m_backupDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
            + QStringLiteral("/shadow-modpack-backup-%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    }
    QDir().mkpath(m_backupDir);
    const QString backup = m_backupDir + QLatin1Char('/')
        + QString::number(qHash(path)) + QLatin1String(".bak");
    if (QFile::copy(path, backup)) {
        m_overwriteBackup.insert(path, backup);
        emit logLine(tr("已备份将被覆盖的文件: %1").arg(path));
    }
}

QVariantList ModpackInstallTask::modItems() const
{
    QVariantList out;
    for (const ModpackRemoteFile& rf : m_meta.files) {
        QVariantMap item;
        item[QStringLiteral("index")] = rf.index;
        QString name = rf.displayName;
        if (name.isEmpty()) name = rf.fileName;
        if (name.isEmpty()) name = QStringLiteral("CF:%1/%2").arg(rf.projectId).arg(rf.fileId);
        item[QStringLiteral("name")] = name;
        item[QStringLiteral("size")] = rf.size;
        item[QStringLiteral("status")] = rf.status;
        item[QStringLiteral("error")] = rf.error;
        out.append(item);
    }
    return out;
}
