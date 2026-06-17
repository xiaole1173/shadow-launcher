#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "utils/types.h"

namespace ShadowLauncher {

class AccountBackend : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString username READ username NOTIFY accountChanged)
    Q_PROPERTY(bool isLoggedIn READ isLoggedIn NOTIFY accountChanged)
    Q_PROPERTY(bool isOnline READ isOnline NOTIFY accountChanged)
    Q_PROPERTY(QString accountUuid READ accountUuid NOTIFY accountChanged)
    Q_PROPERTY(QString skinPath READ skinPath NOTIFY skinReady)
    Q_PROPERTY(QStringList offlineUsernames READ offlineUsernames NOTIFY offlineHistoryChanged)

public:
    explicit AccountBackend(QObject *parent = nullptr);

    // ── Property getters ──
    QString username() const { return m_username; }
    bool isLoggedIn() const { return m_loggedIn; }
    bool isOnline() const { return m_isOnline; }
    QString accountUuid() const { return m_uuid; }
    QString skinPath() const { return m_skinPath; }
    QStringList offlineUsernames() const { return m_offlineUsernames; }

    // ── Slots ──
    Q_INVOKABLE void offlineLogin(const QString &username);
    Q_INVOKABLE void logout();
    Q_INVOKABLE QString getSkinUrl(const QString &username = {}) const;

signals:
    void accountChanged();
    void skinReady();
    void offlineHistoryChanged();
    void logMessage(const QString &msg);

private:
    void downloadSkin(const QString &username);
    void loadOfflineHistory();
    void saveOfflineHistory();
    QString generateOfflineUuid(const QString &username) const;
    QString skinCachePath(const QString &username) const;

    QString m_username;
    QString m_uuid;
    QString m_skinPath;
    bool m_loggedIn = false;
    bool m_isOnline = false;
    QStringList m_offlineUsernames;
    QString m_dataDir;
};

} // namespace ShadowLauncher
