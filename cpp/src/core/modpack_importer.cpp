// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "modpack_importer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include "modpack/modpack_install_task.h"
#include "modpack/modpack_parser.h"
#include "../backend/version_backend.h"
#include "../core/version_isolation.h"
#include "../utils/logger.h"
#include "../core/cf_key_crypto.h"

// 编译期嵌入的 CF API Key（应用标识，向 CurseForge 表明下载来源）—— 密文存储。
// 本地私有文件 cf_api_key_local.h 已被 .gitignore 忽略（远程仓库不含真实 Key）；
// 文件不存在时加密宏缺失 → decryptEmbeddedCfKey 返回空串，回退环境变量/配置文件。
#if defined(__has_include)
#  if __has_include("cf_api_key_local.h")
#    include "cf_api_key_local.h"
#    define SHADOW_HAS_CF_ENC 1
#  endif
#endif
#ifndef SHADOW_HAS_CF_ENC
#  define SHADOW_HAS_CF_ENC 0
#endif

using namespace ShadowLauncher;

// ── 构造 / 析构 ──

ModpackImporter::ModpackImporter(QObject* parent)
    : QObject(parent)
    , m_task(new ModpackInstallTask(this))
{
    connect(m_task, &ModpackInstallTask::stepChanged, this, [this](const QString& s) {
        m_currentStep = s;
        emit stepChanged();
    });
    connect(m_task, &ModpackInstallTask::progressChanged, this,
            [this](qreal p, const QString& text, const QString& file) {
        m_progress = p;
        m_statusText = text;
        m_currentFile = file;
        emit progressChanged();
    });
    connect(m_task, &ModpackInstallTask::fileProgressChanged, this,
            &ModpackImporter::fileProgressChanged);
    connect(m_task, &ModpackInstallTask::logLine, this, [this](const QString& msg) {
        m_lastLogLine = msg;
        qCInfo(logMod).noquote() << msg;
        emit logLine(msg);
    });
    connect(m_task, &ModpackInstallTask::modItemsChanged, this, [this]() {
        m_modItems = m_task->modItems();
        emit modListChanged();
    });
    connect(m_task, &ModpackInstallTask::finished, this, [this](bool ok, const QString& name, const QString& info) {
        m_busy = false;
        emit busyChanged();
        if (ok) {
            m_hasResult = true;
            m_resultName = name;
            m_resultVersionId = info;
            emit hasResultChanged();
        } else {
            m_statusText = info;
            emit progressChanged();
        }
        emit importFinished(ok, name, info);
    });
}

ModpackImporter::~ModpackImporter() = default;

// ── Properties ──

bool ModpackImporter::isBusy() const { return m_busy; }
QString ModpackImporter::statusText() const { return m_statusText; }
qreal ModpackImporter::progress() const { return m_progress; }
QString ModpackImporter::currentFile() const { return m_currentFile; }
QString ModpackImporter::currentStep() const { return m_currentStep; }
bool ModpackImporter::hasResult() const { return m_hasResult; }
QString ModpackImporter::resultName() const { return m_resultName; }
QString ModpackImporter::resultVersionId() const { return m_resultVersionId; }
QVariantList ModpackImporter::modItems() const { return m_modItems; }

// ── Public API ──

void ModpackImporter::startImport(const QString& zipFilePath, bool includeOptional)
{
    // 入口日志：任何路径（含 busy 早退）都先落日志，便于定位「点击无响应」类问题
    qCInfo(logMod) << "[modpack] startImport 被调用:" << zipFilePath
                   << "busy=" << m_busy
                   << "includeOptional=" << includeOptional;
    if (m_busy) return;

    loadApiKeyFromConfig();

    if (m_gameDir.isEmpty()) {
        qCWarning(logMod) << "[modpack] 未设置游戏目录，无法导入";
        m_statusText = tr("启动器未就绪（缺少游戏目录）");
        emit progressChanged();
        emit importFinished(false, {}, m_statusText);
        return;
    }

    m_busy = true;
    m_hasResult = false;
    m_resultName.clear();
    m_resultVersionId.clear();
    m_currentStep.clear();
    m_modItems.clear();
    emit busyChanged();
    emit hasResultChanged();
    emit modListChanged();

    qCInfo(logMod) << "[modpack] 开始导入:" << zipFilePath
                   << (includeOptional ? "(含可选文件)" : "");

    m_task->start(zipFilePath, includeOptional);
}

void ModpackImporter::cancelImport()
{
    if (!m_busy) return;
    m_task->cancel();
}

void ModpackImporter::dismissResult()
{
    m_hasResult = false;
    m_resultName.clear();
    m_resultVersionId.clear();
    emit hasResultChanged();
}

// ── 依赖注入 ──

void ModpackImporter::setVersionBackend(VersionBackend* vb)
{
    m_versionBackend = vb;
    m_task->configure(vb, m_isolation, m_gameDir, m_apiKey);
}

void ModpackImporter::setGameDir(const QString& dir)
{
    m_gameDir = dir;
    m_task->configure(m_versionBackend, m_isolation, dir, m_apiKey);
}

void ModpackImporter::setIsolation(VersionIsolation* iso)
{
    m_isolation = iso;
    m_task->configure(m_versionBackend, iso, m_gameDir, m_apiKey);
}

void ModpackImporter::setCurseForgeApiKey(const QString& key)
{
    m_apiKey = key;
    m_apiKeyLoaded = !key.isEmpty();
    m_task->configure(m_versionBackend, m_isolation, m_gameDir, key);
}

// ── API Key 读取：环境变量 → {gameDir}/config/cf_api_key.json ──

void ModpackImporter::loadApiKeyFromConfig()
{
    if (m_apiKeyLoaded && !m_apiKey.isEmpty()) return;

    QString key = qEnvironmentVariable("SHADOW_CF_API_KEY");
    if (!key.isEmpty()) {
        setCurseForgeApiKey(key);
        qCInfo(logMod) << "[modpack] 已从环境变量读取 CF API Key";
        return;
    }

    if (!m_gameDir.isEmpty()) {
        const QString path = m_gameDir + QStringLiteral("/config/cf_api_key.json");
        QFile f(path);
        if (f.exists() && f.open(QIODevice::ReadOnly)) {
            QJsonParseError perr;
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
            f.close();
            if (perr.error == QJsonParseError::NoError && doc.isObject()) {
                key = doc.object().value(QStringLiteral("key")).toString();
                if (!key.isEmpty()) {
                    setCurseForgeApiKey(key);
                    qCInfo(logMod) << "[modpack] 已从配置文件读取 CF API Key";
                    return;
                }
            }
        }
    }

    // 编译期嵌入 Key（作者默认来源标识，随 Release 分发；环境变量/配置文件可覆盖）
    // 密文存储：HKDF-SHA256 + AES-256-GCM 解密（见 cf_key_crypto.h）。
    // 注：此处只做读取不告警——是否真的需要 CF Key 由下载器在遇到 CF 文件时判定，
    // 避免 Modrinth 等纯第三方包导入时出现无关的 CF Key 提示。
    if (SHADOW_HAS_CF_ENC) {
        key = CfKeyCrypto::decryptEmbeddedCfKey(
            SHADOW_CF_ENC_IKM_HEX, SHADOW_CF_ENC_SALT_HEX,
            SHADOW_CF_ENC_NONCE_HEX, SHADOW_CF_ENC_CIPHER_HEX, SHADOW_CF_ENC_TAG_HEX);
        if (!key.isEmpty()) {
            setCurseForgeApiKey(key);
            return;
        }
    }

    // 无内嵌 key 时不在此告警（交由 ModpackDownloader 在确有 CF 文件时提示）
}

// ── 静态兼容方法 ──

ModpackImporter::Format ModpackImporter::detectFormat(const QString& zipPath)
{
    switch (ModpackParser::detectFormat(zipPath)) {
    case ModpackFormat::Modrinth: return Modrinth;
    case ModpackFormat::CurseForge: return CurseForge;
    default: return Unknown;
    }
}

QJsonObject ModpackImporter::parseManifest(const QString& zipPath, Format fmt)
{
    ModpackMeta meta;
    QString error;
    ModpackFormat f = (fmt == Modrinth) ? ModpackFormat::Modrinth : ModpackFormat::CurseForge;
    if (!ModpackParser::parse(zipPath, f, meta, error))
        return {};
    QJsonObject out;
    out[QStringLiteral("name")] = meta.name;
    out[QStringLiteral("version")] = meta.versionId;
    out[QStringLiteral("mcVersion")] = meta.mcVersion;
    out[QStringLiteral("loaderType")] = meta.loaderType;
    out[QStringLiteral("loaderVersion")] = meta.loaderVersion;
    return out;
}
