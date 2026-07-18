// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QVersionNumber>

namespace ShadowLauncher {

/// Checks Gitee Releases API for new launcher versions.
/// Emits updateAvailable / noUpdateAvailable / checkFailed.
class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);

    /// Current app version (read from CMake SHADOW_DISPLAY_VERSION)
    void setCurrentVersion(const QString& v) { m_currentVersion = v; }
    QString currentVersion() const { return m_currentVersion; }

    /// Gitee repo: owner/repo
    void setRepo(const QString& owner, const QString& repo);

    /// Whether a check is in flight
    bool isChecking() const { return m_checking; }

public slots:
    /// Start an async version check against Gitee Releases API
    void check();

signals:
    void updateAvailable(const QString& version, const QString& releaseNotes,
                         const QString& downloadUrl, const QString& fileName,
                         qint64 fileSize);
    void noUpdateAvailable();
    void checkFailed(const QString& error);

private:
    QVersionNumber parseTag(const QString& tag) const;

    QString m_currentVersion;
    QString m_apiUrl;
    bool m_checking = false;
};

} // namespace ShadowLauncher
