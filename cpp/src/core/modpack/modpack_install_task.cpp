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
    // MC 阶段 300ms 轮询会话进度/速度/原子步骤 → 卡片（并行期同时做模组 EMA 衰减）
    m_mcPollTimer = new QTimer(this);
    m_mcPollTimer->setInterval(300);
    connect(m_mcPollTimer, &QTimer::timeout, this, [this]() {
        if (!m_vb || m_cardId.isEmpty() || m_mcSessionId.isEmpty()) return;
        const qreal p = m_vb->installProgressOf(m_mcSessionId);
        m_mcSpeed = m_vb->installSpeedOf(m_mcSessionId);
        // 模组路 EMA 无数据衰减：长时间无字节流入 → 减半回落，不滞留旧速度
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_lastBytesMs > 0 && now - m_lastBytesMs > 800) {
            m_modEma = m_modEma > 1024 ? m_modEma / 2 : 0;
        }
        m_cardSpeed = m_modEma + m_mcSpeed;
        // 全量同步 MC 原子步骤（PCL 同款细分：JSON/库/资源/校验/loader 主文件/校验/安装）
        syncMcSteps();
        // 总进度合成：模组路权重 + MC 路权重（并行两路各自推进）
        const qreal base = kParseWeight + kExtractWeight + kDownloadWeight * m_modFrac;
        setProgress(base + kMcInstallWeight * p,
                    tr("正在安装 Minecraft %1…").arg(m_meta.mcVersion), m_currentFile);
    });
    // 下载阶段 500ms 同步模组明细/日志附件（避免高频推送卡模型）
    m_attachSyncTimer = new QTimer(this);
    m_attachSyncTimer->setInterval(500);
    connect(m_attachSyncTimer, &QTimer::timeout, this, [this]() {
        if (m_downloader && m_downloader->isRunning()) syncCardAttachments();
    });
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
    // 并行汇合状态复位
    m_modsDone = false;
    m_mcDone = false;
    m_pendingFailSet = false;
    m_pendingFail.clear();
    m_modEma = 0;
    m_mcSpeed = 0;
    m_lastBytes = 0;
    m_lastBytesMs = 0;
    m_lastFileProgMs = 0;
    m_modFrac = 0.0;

    if (!m_backupDir.isEmpty()) {
        QDir(m_backupDir).removeRecursively();
        m_backupDir.clear();
    }

    initTaskCard();

    runParse();
}

// ── 解析（工作线程）──

void ModpackInstallTask::runParse()
{
    setStep(tr("解析整合包"));
    setCardStep(0, QStringLiteral("active"), 0);
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

            // 资源落盘目录：对齐项目现有目录规则（VersionIsolation::getVersionGameDir
            // 方案 C：game/ 存在且非空 → game；否则版本根目录；隔离关闭 → 公共 gameDir）。
            // 不硬编码迁移到 game 子文件夹——启动器读哪里，整合包就写哪里。
            if (m_iso) {
                m_resourceDir = m_iso->getVersionGameDir(m_targetName);
                if (m_resourceDir.isEmpty()) m_resourceDir = m_gameDir;
            } else {
                m_resourceDir = m_gameDir;
            }
            QDir().mkpath(m_resourceDir);
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

    // 纯原版 + 本机已装该 vanilla：复制路径会把源版本目录（含可能的 game/）整体复制到目标。
    // 此时目标 game/ 的存在性取决于源目录结构——预判并重定向资源目录，
    // 避免「mods 写在版本根目录、启动器却读 game/」的结构错位。
    if (m_meta.loaderType.isEmpty() && m_iso && m_iso->isEnabled()
        && m_vb && m_vb->versionManager()
        && m_vb->versionManager()->isInstalled(m_meta.mcVersion)) {
        const QString src = m_vb->versionManager()->getVersionPath(m_meta.mcVersion);
        const QString srcGame = src + QStringLiteral("/game");
        if (QDir(srcGame).exists() && !QDir(srcGame).isEmpty()) {
            m_resourceDir = m_versionDir + QStringLiteral("/game");
            QDir().mkpath(m_resourceDir);
            emit logLine(tr("检测到预装版本含 game 目录，资源写入调整为: %1").arg(m_resourceDir));
        }
    }

    // 卡片：解析完成 → 步骤 0 完成；注册 MC 会话抑制目标（杜绝双卡）
    setCardStep(0, QStringLiteral("completed"), 100);
    if (m_vb) {
        m_vb->updateModpackCardTargets(m_targetName, m_meta.mcVersion);
        m_vb->updateTaskCard(m_cardId, m_progress, m_statusText, false, {}, 0, true,
                             tr("导入整合包：%1").arg(m_meta.name));
    }
    m_cardInfo[QStringLiteral("name")] = m_meta.name;
    m_cardInfo[QStringLiteral("version")] = m_meta.versionId;
    m_cardInfo[QStringLiteral("mc")] = m_meta.mcVersion;
    m_cardInfo[QStringLiteral("loader")] = m_meta.loaderType;
    m_cardInfo[QStringLiteral("format")] =
        m_meta.format == ModpackFormat::Modrinth ? QStringLiteral("Modrinth") : QStringLiteral("CurseForge");
    m_cardInfo[QStringLiteral("targetName")] = m_targetName;
    refreshCardMods();

    // 通知 UI 模组列表
    emit modItemsChanged();

    runExtract();
}

// ── 解压 overrides（工作线程）──

void ModpackInstallTask::runExtract()
{
    setStep(tr("解压整合包资源"));
    setCardStep(1, QStringLiteral("active"), 0);
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
                        if (total > 0) {
                            setProgress(kParseWeight + kExtractWeight * (0.2 + 0.8 * (qreal)done / total),
                                        tr("正在解压资源 (%1/%2)…").arg(done).arg(total));
                            setCardStep(1, QStringLiteral("active"), qRound(100.0 * done / total));
                        }
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
                if (n == -3) {
                    // 致命写盘错误（磁盘满/权限拒绝）：必须中止导入，不能当作取消静默继续
                    fatal = true;
                    fatalError = tr("解压失败：磁盘空间不足或文件写入被拒绝");
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
    m_cardInfo[QStringLiteral("fileCount")] = QString::number(fileCount);
    setCardStep(1, QStringLiteral("completed"), 100);
    runDownload();
}

// ── 模组批量下载 ──

void ModpackInstallTask::runDownload()
{
    if (m_meta.files.isEmpty()) {
        emit logLine(tr("整合包不含远端模组，跳过下载"));
        setCardStep(2, QStringLiteral("completed"), 100);
        m_modsDone = true;
        // 并行：模组路已完成，直接启动 MC 路
        runMcInstall();
        tryFinalize();
        return;
    }

    setStep(tr("下载模组"));
    setCardStep(2, QStringLiteral("active"), 0);
    setProgress(kParseWeight + kExtractWeight + 0.01, tr("准备下载 %1 个文件…").arg(m_meta.files.size()));

    // 一次性接线（每个任务生命周期只 start 一次）
    disconnect(m_downloader, nullptr, this, nullptr);

    connect(m_downloader, &ModpackDownloader::statusChanged, this, [this](const QString& t) {
        m_statusText = t;
        emit progressChanged(m_progress, t, m_currentFile);
        syncCard();
    });
    connect(m_downloader, &ModpackDownloader::fileProgress, this,
            [this](int index, const QString& name, qint64 received, qint64 total) {
        Q_UNUSED(index);
        // 200ms 节流：避免 EMA/折算/卡片更新高频抖动
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastFileProgMs < 200) return;
        m_lastFileProgMs = now;
        m_currentFile = name;
        emit fileProgressChanged(name, total > 0 ? (qreal)received / total : 0.0);
        // 模组路速度：直接用引擎全局 EMA（基于全局已下载字节，带平滑与衰减）——
        // 旧实现用单文件 received 瞬时差：分片/多文件切换时 received 跳变 → 虚高；
        // 且停滞时 fileProgress 不再触发 → 旧速度永不下落。
        m_modEma = qMax<qint64>(0, qint64(m_downloader->currentSpeedMBps() * 1024.0 * 1024.0));
        m_lastBytes = received;
        m_lastBytesMs = now;
        // 速度聚合：模组路 EMA + MC 路（并行期两路同跑时显示总和）
        m_mcSpeed = (!m_mcSessionId.isEmpty() && m_vb) ? m_vb->installSpeedOf(m_mcSessionId) : 0;
        m_cardSpeed = m_modEma + m_mcSpeed;
        // 聚合速度日志（1s 节流）：界面数值 = 模组 EMA + MC EMA，与日志逐条可比
        {
            const qint64 now2 = QDateTime::currentMSecsSinceEpoch();
            if (now2 - m_lastSpeedLogMs >= 1000) {
                m_lastSpeedLogMs = now2;
                emit logLine(tr("[速度] 模组=%1 MB/s MC=%2 MB/s 合计=%3 MB/s")
                    .arg(double(m_modEma) / (1024.0 * 1024.0), 0, 'f', 1)
                    .arg(double(m_mcSpeed) / (1024.0 * 1024.0), 0, 'f', 1)
                    .arg(double(m_cardSpeed) / (1024.0 * 1024.0), 0, 'f', 1));
            }
        }
        syncCard();
        // 步骤 2 字节级折算：单大文件下载中百分比持续前进（修进度停滞观感）
        int doneCount = 0, skipCount = 0;
        for (const ModpackRemoteFile& rf : m_meta.files) {
            if (rf.status == QLatin1String("done")) ++doneCount;
            else if (rf.status == QLatin1String("skipped")) ++skipCount;
        }
        const int effective = qMax(0, m_meta.files.size() - skipCount);
        if (effective > 0) {
            const qreal frac = total > 0 ? (qreal)received / total : 0.0;
            setCardStep(2, QStringLiteral("active"),
                        qRound(100.0 * (doneCount + frac) / effective));
        }
        for (int i = 0; i < m_cardMods.size(); ++i) {
            QVariantMap mm = m_cardMods[i].toMap();
            if (mm.value(QStringLiteral("name")).toString() == name
                || mm.value(QStringLiteral("status")).toString() == QStringLiteral("downloading")) {
                mm[QStringLiteral("progress")] = total > 0 ? (qreal)received / total : 0.0;
                mm[QStringLiteral("status")] = QStringLiteral("downloading");
                m_cardMods[i] = mm;
                break;
            }
        }
    });
    connect(m_downloader, &ModpackDownloader::fileFinished, this,
            [this](int index, bool success, const QString& error) {
        // 卡片：同步该条目终态
        if (index >= 0 && index < m_cardMods.size()) {
            QVariantMap mm = m_cardMods[index].toMap();
            mm[QStringLiteral("status")] = success ? QStringLiteral("done") : QStringLiteral("fail");
            mm[QStringLiteral("error")] = error;
            mm[QStringLiteral("progress")] = success ? 1.0 : 0.0;
            m_cardMods[index] = mm;
            syncCardAttachments();
        }
        emit modItemsChanged();
    });
    connect(m_downloader, &ModpackDownloader::queueProgress, this,
            [this](int completed, int total, int failed) {
                if (total <= 0) return;
                m_modFrac = (qreal)completed / total;
                // 步骤 2 动态文案：模组批量下载，剩余 XX 个文件
                const int remain = qMax(0, total - completed - failed);
                const QString stepName = failed > 0
                    ? tr("模组批量下载，剩余 %1 个文件（%2 个失败）").arg(remain).arg(failed)
                    : tr("模组批量下载，剩余 %1 个文件").arg(remain);
                setCardStepName(2, stepName);
                const qreal base = kParseWeight + kExtractWeight;
                const QString text = failed > 0
                    ? tr("正在下载模组 (%1/%2，%3 个失败)…").arg(completed).arg(total).arg(failed)
                    : tr("正在下载模组 (%1/%2)…").arg(completed).arg(total);
                // 并行总进度合成：模组路权重 + MC 路权重（MC 未启动时为 0，不变）
                const qreal mcFrac = (!m_mcSessionId.isEmpty() && m_vb)
                    ? m_vb->installProgressOf(m_mcSessionId) : 0.0;
                setProgress(base + kDownloadWeight * m_modFrac + kMcInstallWeight * mcFrac,
                            text, m_currentFile);
                setCardStep(2, QStringLiteral("active"), qRound(100.0 * m_modFrac));
            });
    connect(m_downloader, &ModpackDownloader::logLine, this, [this](const QString& msg) {
        emit logLine(msg);
        pushCardLog(msg);
    });
    connect(m_downloader, &ModpackDownloader::allFinished, this, &ModpackInstallTask::onDownloadAllFinished);

    m_downloader->setTargetDir(m_resourceDir);
    m_downloader->setFiles(&m_meta.files);
    m_downloader->setOverwriteHook([this](const QString& savePath) {
        registerOverwrite(savePath);
    });
    m_downloader->setCreatedHook([this](const QString& savePath) {
        // 新建文件登记回滚：任务失败/取消时清理，共享目录模式不残留
        registerCreatedFile(savePath);
    });
    m_downloader->start(m_includeOptional);
    refreshCardMods();   // 初始模组明细（含可选文件 skipped 标记）
    m_attachSyncTimer->start();

    // ═══ 并行化：模组下载与 MC+加载器安装同时启动，互不阻塞 ═══
    // 两路完成后由 tryFinalize() 汇合进收尾；任一路失败/取消由守卫统一处理。
    runMcInstall();
}

void ModpackInstallTask::onDownloadAllFinished(bool cancelled)
{
    Q_UNUSED(cancelled);   // 取消判定用 m_cancel（downloader 中止也可能是失败守卫触发）
    m_attachSyncTimer->stop();
    m_modsDone = true;

    if (m_cancel) {
        // 用户取消：等待 MC 路停止后统一回滚（两路汇合）
        tryCancelFinish();
        return;
    }

    setCardStep(2, QStringLiteral("completed"), 100);
    refreshCardMods();

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
        if (m_installingMc) {
            // MC 路还在跑：登记待失败，等其停止后统一出口（避免与安装线程竞态）
            m_pendingFailSet = true;
            m_pendingFail = tr("全部 %1 个文件下载失败，首个错误: %2").arg(total).arg(firstError);
            emit logLine(tr("⚠ %1").arg(m_pendingFail));
            return;
        }
        fail(tr("全部 %1 个文件下载失败，首个错误: %2").arg(total).arg(firstError));
        return;
    }
    if (failed > 0) {
        emit logLine(tr("⚠ %1/%2 个文件下载失败（已跳过），整合包可能不完整: %3")
            .arg(failed).arg(total).arg(firstError));
    }

    tryFinalize();
}

// ── 并行汇合：模组路 + MC 路都完成后进收尾 ──

void ModpackInstallTask::tryFinalize()
{
    if (m_cancel) { tryCancelFinish(); return; }
    if (!m_modsDone || !m_mcDone) return;   // 两路都完成才汇合
    if (m_phase == Phase::Error || m_phase == Phase::Cancelled || m_phase == Phase::Done) return;
    runFinalize();
}

void ModpackInstallTask::tryCancelFinish()
{
    if (!m_modsDone || !m_mcDone) return;   // 等两路都停止
    if (!m_busy) return;
    rollback();
    finishCancelled();
}

// ── MC + 加载器安装（复用 VersionBackend merged 流程）──

void ModpackInstallTask::runMcInstall()
{
    setStep(tr("安装游戏本体与加载器"));
    setProgress(kParseWeight + kExtractWeight + kDownloadWeight * m_modFrac + 0.02,
                tr("正在安装 Minecraft %1…").arg(m_meta.mcVersion));

    m_installingMc = true;

    if (!m_meta.loaderType.isEmpty() && !m_meta.loaderVersion.isEmpty()) {
        // 带加载器：merged 安装（MC + 加载器一体）；原子细分步骤由
        // syncMcSteps 从会话 pipeline 实时透传到卡片（PCL 同款，禁止合并封装）
        m_mcSessionId = m_targetName;   // merged 会话 id = 目标版本名
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
    } else {
        // 纯原版整合包：安装 vanilla 后改为目标名
        m_mcSessionId = m_meta.mcVersion;   // vanilla 会话 id = MC 版本
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

    // MC 阶段轮询：进度/速度/原子步骤 → 卡片（300ms）
    m_mcPollTimer->start();
}

void ModpackInstallTask::onMcInstallFinished(bool ok)
{
    m_mcPollTimer->stop();
    m_mcDone = true;

    if (m_cancel) {
        // 取消路径：等待模组路停止后统一回滚（两路汇合）
        tryCancelFinish();
        return;
    }
    if (m_pendingFailSet) {
        // 模组路已判失败且等待本路停止：统一失败出口
        fail(m_pendingFail);
        return;
    }
    if (!ok) {
        fail(tr("Minecraft %1 安装失败").arg(m_meta.mcVersion));
        return;
    }

    syncMcSteps();   // 最后同步一次 MC 原子步骤（pipeline 已完成态）

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
                tryCancelFinish();
                return;
            }
            tryFinalize();
        });
        return;
    }

    tryFinalize();
}

// ── 收尾：核对 version.json + 注册版本列表 ──

void ModpackInstallTask::runFinalize()
{
    setStep(tr("整理版本信息"));
    if (!m_cardSteps.isEmpty())
        setCardStep(m_cardSteps.size() - 1, QStringLiteral("active"), 0);   // 版本注册步骤（动态索引）
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

    // 按全局源策略排序：官方优先时 launchermeta 在前（另一源兜底）
    static const char* kMirrorManifest = "https://bmclapi2.bangbang93.com/mc/game/version_manifest.json";
    static const char* kOfficialManifest = "https://launchermeta.mojang.com/mc/game/version_manifest.json";
    const bool preferOfficial = m_vb && m_vb->downloadPreferOfficial();
    const char* kManifestUrls[2] = {
        preferOfficial ? kOfficialManifest : kMirrorManifest,
        preferOfficial ? kMirrorManifest : kOfficialManifest
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

    if (!m_cardSteps.isEmpty())
        setCardStep(m_cardSteps.size() - 1, QStringLiteral("completed"), 100);
    finishCard(true, {});

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
    if (!m_busy || m_cancel) return;
    m_cancel = true;

    const bool dlRunning = m_downloader->isRunning();
    const bool mcRunning = m_installingMc && m_vb;
    emit logLine(tr("══ 取消触发：终止全部任务（模组下载 / MC 下载 / 加载器安装）══"));
    if (dlRunning) {
        emit logLine(tr("→ 正在终止模组独立下载引擎（网络请求 + 落盘）…"));
        m_downloader->cancel();   // 同步触发 allFinished(true) → 任务侧 tryCancelFinish 汇合
    }
    if (mcRunning) {
        // MC/加载器安装中：调用版本后端定向取消（→ installFinished → 任务侧汇合）。
        // ⚠ 必须传会话 id（merged=targetName / vanilla=mcVersion）：纯原版路径
        //   传 targetName 命中不到 vanilla 下载器（其 id 是 mcVersion），MC 下载不会停。
        emit logLine(tr("→ 正在终止版本安装（会话 %1）…").arg(m_mcSessionId));
        m_vb->cancelVersionInstall(m_mcSessionId);
    }
    if (!dlRunning && !mcRunning) {
        emit logLine(tr("→ 当前处于解析/解压阶段，等待工作线程响应取消…"));
    }
    // 解析/解压阶段：不抢先回滚——工作线程检查 m_cancel 后自行收尾
    // （onParsed/onExtracted 的 m_cancel 分支 rollback+finishCancelled），
    // 立即回滚会与解压写盘并发竞态产生残留。
    // 统一兜底：任一路回调未触发（信号丢失/工作线程异常），5s 后强制收尾。
    emit logLine(tr("→ 等待全部任务停止后统一清理临时文件…"));
    QTimer::singleShot(5000, this, [this]() {
        if (!m_busy) return;
        emit logLine(tr("⚠ 取消兜底触发：强制清理（部分任务未响应）"));
        rollback();
        finishCancelled();
    });
}

// ── 失败 / 回滚 ──

void ModpackInstallTask::fail(const QString& error)
{
    if (m_phase == Phase::Error || m_phase == Phase::Cancelled || m_phase == Phase::Done) return;

    // 另一路还在跑：先停它，等汇合后统一失败出口（避免与下载/安装线程写盘竞态）
    if (m_downloader->isRunning())
        m_downloader->cancel();
    if (m_installingMc && m_vb) {
        m_pendingFailSet = true;
        m_pendingFail = error;
        emit logLine(tr("⚠ %1（等待版本安装停止后回滚）").arg(error));
        // ⚠ 同 cancel()：用会话 id（merged=targetName / vanilla=mcVersion）
        m_vb->cancelVersionInstall(m_mcSessionId);
        return;   // onMcInstallFinished 汇合后 fail(m_pendingFail) 统一出口
    }

    m_phase = Phase::Error;
    m_busy = false;
    m_statusText = error;
    // 卡片：标记当前活动步骤失败 + 终态
    for (int i = m_cardSteps.size() - 1; i >= 0; --i) {
        if (m_cardSteps[i].toMap().value(QStringLiteral("status")).toString() == QStringLiteral("active")) {
            setCardStep(i, QStringLiteral("failed"),
                        m_cardSteps[i].toMap().value(QStringLiteral("percentage")).toInt());
            break;
        }
    }
    finishCard(false, error);
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
    finishCard(false, tr("已取消"));
    emit logLine(tr("导入已取消，已清理本次生成的文件"));
    emit progressChanged(0.0, tr("已取消"), {});
    emit stepChanged(tr("已取消"));
    emit finished(false, {}, tr("已取消"));
}

void ModpackInstallTask::rollback()
{
    emit logLine(tr("回滚清理中…"));

    int restored = 0;
    // 1. 恢复被覆盖的旧文件
    for (auto it = m_overwriteBackup.constBegin(); it != m_overwriteBackup.constEnd(); ++it) {
        QFile::remove(it.key());
        QFile::copy(it.value(), it.key());
        ++restored;
    }
    if (restored > 0)
        emit logLine(tr("→ 已恢复被覆盖的旧文件 %1 个").arg(restored));
    // 2. 删除本任务创建的文件（含已下载模组/解压资源/临时 Jar）
    int removedFiles = 0;
    for (const QString& f : m_createdFiles) {
        if (QFile::remove(f)) ++removedFiles;
    }
    emit logLine(tr("→ 已删除任务创建文件 %1 个（模组/解压资源/临时 Jar）").arg(removedFiles));
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
            emit logLine(tr("→ 已删除版本目录: %1").arg(m_versionDir));
        }
    }
    // 5. 备份目录
    if (!m_backupDir.isEmpty()) {
        QDir(m_backupDir).removeRecursively();
        emit logLine(tr("→ 已删除覆盖备份目录: %1").arg(m_backupDir));
        m_backupDir.clear();
    }
    emit logLine(tr("✅ 临时文件清理完成，本次任务无残留、无挂起后台任务"));
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
    syncCard();
}

void ModpackInstallTask::log(const QString& msg)
{
    emit logLine(msg);
    pushCardLog(msg);
}

// ── 原生任务卡片（InstallCardModel 轮询通道）──

void ModpackInstallTask::initTaskCard()
{
    if (!m_vb) return;
    m_cardId = QStringLiteral("modpack-") + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    m_cardSteps.clear();
    // 基础 3 步 + 版本注册（MC 原子步骤由 syncMcSteps 动态插入中间，PCL 同款细分）
    const QStringList names = {
        tr("解析校验"), tr("Overrides 解压"), tr("模组批量下载"),
        tr("版本信息注册")
    };
    for (const QString& n : names) {
        QVariantMap s;
        s[QStringLiteral("name")] = n;
        s[QStringLiteral("status")] = QStringLiteral("pending");
        s[QStringLiteral("percentage")] = 0;
        s[QStringLiteral("show")] = true;
        m_cardSteps.append(s);
    }
    m_cardLogs.clear();
    m_cardMods.clear();
    m_cardInfo.clear();
    m_cardSpeed = 0;
    m_modEma = 0;
    m_mcSpeed = 0;
    m_lastBytes = 0;
    m_lastBytesMs = 0;
    m_lastFileProgMs = 0;
    m_modFrac = 0.0;
    m_mcSessionId.clear();
    m_cardDone = false;

    m_vb->setModpackCard(m_cardId, [this]() { cancel(); });   // 卡片原生取消控件转发
    m_vb->addTaskCard(m_cardId, tr("导入整合包"));
    m_vb->updateTaskCardSteps(m_cardId, m_cardSteps);
    pushCardLog(tr("开始导入整合包"));
}

void ModpackInstallTask::syncCard()
{
    if (m_cardId.isEmpty() || !m_vb) return;
    // 可能从工作线程（解压进度回调）触发 → 归队主线程再碰模型
    QMetaObject::invokeMethod(this, [this]() {
        if (m_cardId.isEmpty() || !m_vb) return;
        m_vb->updateTaskCard(m_cardId, m_progress, m_statusText, false, {},
                             m_cardSpeed, !m_cardDone);
    }, Qt::QueuedConnection);
}

void ModpackInstallTask::setCardStep(int idx, const QString& status, int pct)
{
    if (m_cardId.isEmpty() || idx < 0 || idx >= m_cardSteps.size()) return;
    QMetaObject::invokeMethod(this, [this, idx, status, pct]() {
        if (idx < 0 || idx >= m_cardSteps.size()) return;
        QVariantMap s = m_cardSteps[idx].toMap();
        s[QStringLiteral("status")] = status;
        s[QStringLiteral("percentage")] = pct;
        s[QStringLiteral("show")] = s.value(QStringLiteral("show"), true);
        m_cardSteps[idx] = s;
        if (m_vb) m_vb->updateTaskCardSteps(m_cardId, m_cardSteps);
    }, Qt::QueuedConnection);
}

void ModpackInstallTask::setCardStepName(int idx, const QString& name)
{
    if (m_cardId.isEmpty() || idx < 0 || idx >= m_cardSteps.size()) return;
    QMetaObject::invokeMethod(this, [this, idx, name]() {
        if (idx < 0 || idx >= m_cardSteps.size()) return;
        QVariantMap s = m_cardSteps[idx].toMap();
        if (s.value(QStringLiteral("name")).toString() == name) return;  // 未变化跳过
        s[QStringLiteral("name")] = name;
        m_cardSteps[idx] = s;
        if (m_vb) m_vb->updateTaskCardSteps(m_cardId, m_cardSteps);
    }, Qt::QueuedConnection);
}

void ModpackInstallTask::syncMcSteps()
{
    // 会话 pipeline 实时原子步骤 → 卡片 MC 区（[解析,解压,模组] + MC原子 + [版本注册]）
    if (!m_vb || m_mcSessionId.isEmpty() || m_cardId.isEmpty()) return;
    const QVariantList ss = m_vb->sessionSteps(m_mcSessionId);
    if (ss.isEmpty() || m_cardSteps.size() < 4) return;
    QMetaObject::invokeMethod(this, [this, ss]() {
        if (m_cardId.isEmpty() || m_cardSteps.size() < 4) return;
        QVariantList newSteps = m_cardSteps.mid(0, 3);   // 解析校验 / Overrides 解压 / 模组批量下载
        for (const QVariant& v : ss) newSteps.append(v);  // MC/加载器原子步骤（PCL 同款细分）
        newSteps.append(m_cardSteps.last());              // 版本信息注册
        // 内容比较：无变化跳过（避免高频重建）
        if (newSteps.size() == m_cardSteps.size()) {
            bool same = true;
            for (int i = 0; i < newSteps.size(); ++i) {
                const QVariantMap a = newSteps[i].toMap();
                const QVariantMap b = m_cardSteps[i].toMap();
                if (a.value(QStringLiteral("name")) != b.value(QStringLiteral("name"))
                    || a.value(QStringLiteral("status")) != b.value(QStringLiteral("status"))
                    || a.value(QStringLiteral("percentage")).toInt() != b.value(QStringLiteral("percentage")).toInt()
                    || a.value(QStringLiteral("show")).toBool() != b.value(QStringLiteral("show")).toBool()) {
                    same = false;
                    break;
                }
            }
            if (same) return;
        }
        m_cardSteps = newSteps;
        if (m_vb) m_vb->updateTaskCardSteps(m_cardId, m_cardSteps);
    }, Qt::QueuedConnection);
}

void ModpackInstallTask::syncCardAttachments()
{
    if (m_cardId.isEmpty() || !m_vb) return;
    QMetaObject::invokeMethod(this, [this]() {
        if (m_cardId.isEmpty() || !m_vb) return;
        m_cardInfo[QStringLiteral("modCount")] = m_cardMods.size();
        m_vb->updateTaskCardAttachments(m_cardId, m_cardMods, m_cardLogs, m_cardInfo);
    }, Qt::QueuedConnection);
}

void ModpackInstallTask::pushCardLog(const QString& msg)
{
    if (msg.isEmpty()) return;
    QVariantMap l;
    l[QStringLiteral("text")] = msg;
    l[QStringLiteral("color")] =
        msg.contains(QStringLiteral("❌")) ? QStringLiteral("#e06060")
        : msg.contains(QStringLiteral("⚠")) ? QStringLiteral("#ff9800")
        : msg.contains(QStringLiteral("✅")) ? QStringLiteral("#4bc870")
        : QStringLiteral("#a8b0c0");
    m_cardLogs.append(l);
    while (m_cardLogs.size() > 300) m_cardLogs.removeFirst();
    syncCardAttachments();
}

void ModpackInstallTask::refreshCardMods()
{
    m_cardMods.clear();
    for (const ModpackRemoteFile& rf : m_meta.files) {
        QVariantMap mm;
        QString name = rf.displayName;
        if (name.isEmpty()) name = rf.fileName;
        if (name.isEmpty()) name = QStringLiteral("CF:%1/%2").arg(rf.projectId).arg(rf.fileId);
        mm[QStringLiteral("name")] = name;
        mm[QStringLiteral("size")] = rf.size;
        mm[QStringLiteral("status")] = rf.status;
        mm[QStringLiteral("error")] = rf.error;
        mm[QStringLiteral("progress")] = rf.status == QLatin1String("done") ? 1.0 : 0.0;
        m_cardMods.append(mm);
    }
    syncCardAttachments();
}

void ModpackInstallTask::finishCard(bool success, const QString& err)
{
    m_cardDone = true;
    if (m_mcPollTimer) m_mcPollTimer->stop();
    if (m_attachSyncTimer) m_attachSyncTimer->stop();
    if (m_cardId.isEmpty() || !m_vb) return;
    if (success) {
        m_vb->updateTaskCard(m_cardId, 1.0, tr("已完成"), false, {}, 0, false);
    } else {
        const bool cancelled = (err == tr("已取消"));
        m_vb->updateTaskCard(m_cardId, m_progress,
                             cancelled ? tr("已取消") : tr("失败"),
                             true, err, 0, false);
    }
    syncCardAttachments();
}

void ModpackInstallTask::removeCard()
{
    if (m_cardId.isEmpty() || !m_vb) return;
    m_vb->removeTaskCard(m_cardId);
    m_cardId.clear();
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
