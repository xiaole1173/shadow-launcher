#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "utils/types.h"
#include "../core/microsoft_auth.h"

namespace ShadowLauncher {

class AccountBackend : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString username READ username NOTIFY accountChanged)
    Q_PROPERTY(bool isLoggedIn READ isLoggedIn NOTIFY accountChanged)
    Q_PROPERTY(bool isOnline READ isOnline NOTIFY accountChanged)
    Q_PROPERTY(QString accountUuid READ accountUuid NOTIFY accountChanged)
    Q_PROPERTY(QString skinPath READ skinPath NOTIFY skinReady)
    Q_PROPERTY(QString offlineSkinPath READ offlineSkinPath NOTIFY offlineSkinReady)
    Q_PROPERTY(QString capePath READ capePath NOTIFY capeReady)
    Q_PROPERTY(QStringList offlineUsernames READ offlineUsernames NOTIFY offlineHistoryChanged)
    Q_PROPERTY(bool microsoftLoginBusy READ isMicrosoftLoginBusy NOTIFY microsoftLoginBusyChanged)

public:
    explicit AccountBackend(QObject *parent = nullptr);

    // ── Property getters ──
    QString username() const { return m_username; }
    bool isLoggedIn() const { return m_loggedIn; }
    bool isOnline() const { return m_isOnline; }
    QString accountUuid() const { return m_uuid; }
    QString mcToken() const { return m_msMcToken; }
    QString skinPath() const { return m_skinPath; }
    QString offlineSkinPath() const { return m_offlineSkinPath; }
    QString capePath() const { return m_capePath; }
    QStringList offlineUsernames() const { return m_offlineUsernames; }

    // ── Slots ──
    Q_INVOKABLE void offlineLogin(const QString &username);
    Q_INVOKABLE void microsoftLogin();
    Q_INVOKABLE void cancelMicrosoftLogin();
    Q_INVOKABLE void updateOfflineSkin(const QString &username);
    Q_INVOKABLE void setOnlineMode();
    Q_INVOKABLE void logout();
    Q_INVOKABLE bool isMicrosoftLoginBusy() const { return m_msAuth && m_msAuth->isBusy(); }

    // Microsoft login state
    Q_PROPERTY(QString msStatus READ msStatus NOTIFY microsoftLoginProgress)
    QString msStatus() const { return m_msStatus; }

signals:
    void accountChanged();
    void skinReady();
    void offlineSkinReady();
    void capeReady();
    void offlineHistoryChanged();
    void logMessage(const QString &msg);

    // Microsoft login signals
    void microsoftLoginProgress(const QString& step, const QString& detail);
    void microsoftLoginSuccess(const QString& username, const QString& uuid);
    void microsoftLoginFailed(const QString& error);
    void microsoftLoginBusyChanged();

private:
    void downloadSkin(const QString &username);
    void downloadOnlineSkin();
    void setFallbackSkin();
    QString renderHead3D(const QString& skinPath);
    static QString toImageUrl(const QString& filePath);
    void loadOfflineHistory();
    void saveOfflineHistory();
    void saveMicrosoftSession();
    void loadMicrosoftSession();
    void refreshMicrosoftToken();
    QString generateOfflineUuid(const QString &username) const;
    QString skinCachePath(const QString &username) const;
    QString capeCachePath(const QString &username) const;
    void initOfflineHeads();

    // Microsoft login state
    MicrosoftAuth* m_msAuth = nullptr;
    QString m_msStatus;
    QString m_msRefreshToken;
    QString m_msMcToken;

    QString m_username;
    QString m_uuid;
    QString m_skinPath;
    QString m_offlineSkinPath;
    QString m_capePath;
    bool m_loggedIn = false;
    bool m_isOnline = false;
    QStringList m_offlineUsernames;
    QString m_dataDir;
};

} // namespace ShadowLauncher
