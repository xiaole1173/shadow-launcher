// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace ShadowLauncher {

class CheckBackend : public QObject {
    Q_OBJECT

public:
    explicit CheckBackend(QObject *parent = nullptr);

    // ── P0 checks (all synchronous — fast) ──
    Q_INVOKABLE QVariantMap checkJava(const QString &javaPath);
    Q_INVOKABLE QVariantMap checkVersionJar(const QString &versionId,
                                            const QString &gameDir);
    Q_INVOKABLE QVariantMap checkVersionJson(const QString &versionId,
                                             const QString &gameDir);
    Q_INVOKABLE QVariantMap checkMemory(int maxMemoryMB);
    Q_INVOKABLE QVariantMap checkAll(const QString &versionId,
                                     const QString &javaPath,
                                     int maxMemoryMB,
                                     const QString &gameDir);

signals:
    void logMessage(const QString &msg);

private:
    bool checkJavaArch(const QString &javaPath, bool &is64bit, QString &archStr);
    qint64 getAvailableMemoryMB();
};

} // namespace ShadowLauncher
