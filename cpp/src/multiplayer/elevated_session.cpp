// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "elevated_session.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace ElevatedSession {

// ── Static state ──
static bool s_active = false;
static QString s_relayEndpoint;
static SessionData s_currentSession;

bool isActive() { return s_active; }
void setActive(bool v) { s_active = v; }
QString relayEndpoint() { return s_relayEndpoint; }
void setRelayEndpoint(const QString& v) { s_relayEndpoint = v; }
SessionData currentSession() { return s_currentSession; }
void setCurrentSession(const SessionData& data) {
    s_currentSession = data;
    s_relayEndpoint = data.relayEp;
}

// ── Save ──
QString saveElevationConfig(const QString& networkName,
                           const QString& networkKey,
                           const QString& relayEp,
                           const QString& hostname,
                           const QString& roomCode,
                           quint16 mcPort,
                           const QString& role)
{
    QJsonObject obj;
    obj[QStringLiteral("role")] = role;
    obj[QStringLiteral("networkName")] = networkName;
    obj[QStringLiteral("networkKey")] = networkKey;
    obj[QStringLiteral("relayEndpoint")] = relayEp;
    obj[QStringLiteral("hostname")] = hostname;
    obj[QStringLiteral("roomCode")] = roomCode;
    obj[QStringLiteral("mcPort")] = mcPort;
    obj[QStringLiteral("role")] = role;
    obj[QStringLiteral("exePath")] = QCoreApplication::applicationFilePath();

    QString path = QDir::tempPath()
        + QStringLiteral("/shadow_elevate_")
        + QUuid::createUuid().toString(QUuid::Id128)
        + QStringLiteral(".json");

    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        f.close();
    }
    return path;
}

// ── Load ──
SessionData loadAndDelete(const QString& path)
{
    SessionData data;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return data;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    QFile::remove(path);

    if (doc.isNull() || !doc.isObject())
        return data;

    QJsonObject obj = doc.object();
    data.role = obj.value(QStringLiteral("role")).toString(QStringLiteral("host"));
    data.networkName = obj.value(QStringLiteral("networkName")).toString();
    data.networkKey = obj.value(QStringLiteral("networkKey")).toString();
    data.relayEp = obj.value(QStringLiteral("relayEndpoint")).toString();
    data.hostname = obj.value(QStringLiteral("hostname")).toString();
    data.roomCode = obj.value(QStringLiteral("roomCode")).toString();
    data.mcPort = static_cast<quint16>(obj.value(QStringLiteral("mcPort")).toInt());
    return data;
}

// ── Build environment ──
QProcessEnvironment buildEnvironment(const QString& networkName,
                                    const QString& networkKey)
{
    QProcessEnvironment env;
    // Inherit essential system vars so the child can find DLLs
    auto inheritVar = [&](const QString& name) {
        QString val = QString::fromLocal8Bit(qgetenv(name.toLocal8Bit().constData()));
        if (!val.isEmpty())
            env.insert(name, val);
    };
    inheritVar(QStringLiteral("PATH"));
    inheritVar(QStringLiteral("SYSTEMROOT"));
    inheritVar(QStringLiteral("TMP"));
    inheritVar(QStringLiteral("USERPROFILE"));

    // Our ET_* vars
    env.insert(QStringLiteral("ET_NETWORK_NAME"), networkName);
    env.insert(QStringLiteral("ET_NETWORK_SECRET"), networkKey);
    env.insert(QStringLiteral("ET_PEERS"), s_relayEndpoint);
    env.insert(QStringLiteral("ET_DHCP"), QStringLiteral("true"));

    return env;
}

} // namespace ElevatedSession
