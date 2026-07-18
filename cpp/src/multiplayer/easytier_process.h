// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// EasyTier process manager — launches/shuts down EasyTier as a child process
#pragma once
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

class EasyTierProcess : public QObject {
    Q_OBJECT
public:
    explicit EasyTierProcess(QObject* parent = nullptr);
    ~EasyTierProcess() override;

    void start(const QString& networkName, const QString& networkKey,
               const QString& hostname = QString());
    void stop();
    bool isRunning() const;

    QString virtualIp() const { return m_virtualIp; }
    quint16 centerPort() const { return m_centerPort; }

signals:
    void networkReady(const QString& virtualIp);
    void errorOccurred(const QString& msg);
    void stateChanged(const QString& state);

private slots:
    void onProcessStarted();
    void onProcessError(QProcess::ProcessError error);
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void checkOutput();
    void onTimerTick();
    void pollPeerList();

private:
    QString findEasyTierExe() const;
    QString findEasyTierCli() const;
    void parseOutputLine(const QString& line);
    void parsePeerList(const QString& output);

    QProcess* m_process = nullptr;
#ifdef Q_OS_WIN
    HANDLE m_winProcess = nullptr;
#endif
    QString m_networkName;
    QString m_networkKey;
    QString m_hostname;
    QString m_virtualIp;
    quint16 m_centerPort = 0;
    bool m_ready = false;
    QTimer* m_outputTimer = nullptr;
    QTimer* m_timeoutTimer = nullptr;
    QString m_outputBuffer;

    // CLI polling for peer status
    QProcess* m_cliProcess = nullptr;
};
