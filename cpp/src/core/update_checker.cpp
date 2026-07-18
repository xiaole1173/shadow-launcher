// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "update_checker.h"
#include "http_client.h"
#include "utils/logger.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVersionNumber>

namespace ShadowLauncher {

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
{
}

void UpdateChecker::setRepo(const QString& owner, const QString& repo)
{
    // https://gitee.com/api/v5/repos/{owner}/{repo}/releases/latest
    m_apiUrl = QStringLiteral("https://gitee.com/api/v5/repos/%1/%2/releases/latest")
                   .arg(owner, repo);
}

void UpdateChecker::check()
{
    if (m_checking) {
        qCInfo(logApp) << "[更新检查] 已有检查进行中，跳过";
        return;
    }
    if (m_apiUrl.isEmpty()) {
        emit checkFailed(QStringLiteral("更新地址未配置"));
        return;
    }

    m_checking = true;
    qCInfo(logApp) << "[更新检查] 正在检查更新... 当前版本:" << m_currentVersion;

    HttpClient::instance().get(m_apiUrl,
        [this](int status, const QByteArray& body) {
            m_checking = false;

            if (status != 200) {
                qCWarning(logApp) << "[更新检查] Gitee API 返回 HTTP" << status;
                emit checkFailed(
                    QStringLiteral("Gitee API 请求失败 (HTTP %1)").arg(status));
                return;
            }

            QJsonParseError parseErr;
            QJsonDocument doc = QJsonDocument::fromJson(body, &parseErr);
            if (parseErr.error != QJsonParseError::NoError) {
                qCWarning(logApp) << "[更新检查] JSON 解析失败:" << parseErr.errorString();
                emit checkFailed(QStringLiteral("服务器返回数据格式异常"));
                return;
            }

            QJsonObject release = doc.object();

            // Gitee API returns tag_name, not tag_name directly — it uses
            // "tag_name" just like GitHub
            QString tagName = release.value(QStringLiteral("tag_name")).toString();
            if (tagName.isEmpty()) {
                qCWarning(logApp) << "[更新检查] Release 中找不到 tag_name";
                emit checkFailed(QStringLiteral("未找到版本信息"));
                return;
            }

            QVersionNumber serverVersion = parseTag(tagName);
            QVersionNumber currentVersion = parseTag(m_currentVersion);

            qCInfo(logApp) << "[更新检查] 服务器版本:" << tagName
                           << "当前版本:" << m_currentVersion;

            if (QVersionNumber::compare(serverVersion, currentVersion) <= 0) {
                qCInfo(logApp) << "[更新检查] 已是最新版本";
                emit noUpdateAvailable();
                return;
            }

            // New version available — extract download info
            QString releaseNotes = release.value(QStringLiteral("body")).toString();
            QJsonArray assets = release.value(QStringLiteral("assets")).toArray();

            if (assets.isEmpty()) {
                qCWarning(logApp) << "[更新检查] Release 中没有附件";
                emit checkFailed(QStringLiteral("未找到下载文件"));
                return;
            }

            // Pick first asset (typically the .7z archive)
            QJsonObject asset = assets.first().toObject();
            QString downloadUrl = asset.value(QStringLiteral("browser_download_url")).toString();
            QString fileName    = asset.value(QStringLiteral("name")).toString();
            qint64  fileSize    = static_cast<qint64>(
                                      asset.value(QStringLiteral("size")).toDouble());

            qCInfo(logApp) << "[更新检查] 发现新版本:" << tagName
                           << "文件:" << fileName
                           << "大小:" << (fileSize / 1024 / 1024) << "MB";

            emit updateAvailable(tagName, releaseNotes, downloadUrl, fileName, fileSize);
        },
        [this](const QString& error) {
            m_checking = false;
            qCWarning(logApp) << "[更新检查] 网络错误:" << error;
            emit checkFailed(QStringLiteral("网络连接失败: %1").arg(error));
        }
    );
}

QVersionNumber UpdateChecker::parseTag(const QString& tag) const
{
    // Strip leading 'v' if present: "v0.2.1-beta" → "0.2.1-beta"
    QString cleaned = tag;
    if (cleaned.startsWith(QLatin1Char('v')) || cleaned.startsWith(QLatin1Char('V')))
        cleaned = cleaned.mid(1);

    // Split off pre-release suffix for comparison
    // "0.2.1-beta" → segments [0, 2, 1]
    int dashIdx = cleaned.indexOf(QLatin1Char('-'));
    QString versionPart = (dashIdx >= 0) ? cleaned.left(dashIdx) : cleaned;

    return QVersionNumber::fromString(versionPart);
}

} // namespace ShadowLauncher
