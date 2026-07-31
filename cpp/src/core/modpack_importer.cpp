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
        qCWarning(logMod) << "[modpack] 未配置 CurseForge API Key"
                          << "(设置项: 环境变量 SHADOW_CF_API_KEY 或 " << path << ")";
    }
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
