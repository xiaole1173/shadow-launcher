// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// EasyTier process manager — launches/shuts down EasyTier as a child process
#pragma once
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QList>
#include <QTimer>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

// NAT types from EasyTier peer JSON — declared before use
enum class EasyTierNatType {
    Unknown,
    OpenInternet,
    NoPAT,
    FullCone,
    Restricted,
    PortRestricted,
    Symmetric,
    SymmetricUdpWall,
    SymmetricEasyIncrease,
    SymmetricEasyDecrease
};

// Peer member info parsed from CLI output
struct EasyTierPeerInfo {
    QString hostname;
    QString ipv4;
    bool isLocal = false;
    EasyTierNatType natType = EasyTierNatType::Unknown;
};

class EasyTierProcess : public QObject {
    Q_OBJECT
public:
    explicit EasyTierProcess(QObject* parent = nullptr);
    ~EasyTierProcess() override;

    void start(const QString& networkName, const QString& networkKey,
               const QString& hostname = QString(),
               const QList<quint16>& whitelistPorts = {});

    bool addPortForward(const QString& localAddr, quint16 localPort,
                        const QString& remoteAddr, quint16 remotePort,
                        const QString& proto = QStringLiteral("tcp"));

    void stop();

    // Elevated mode: start easytier via QProcess (config file has public peers)
    void startViaQProcess(const QString& exe, const QStringList& args,
                          const QByteArray& tomlData);

    // Add relay connector dynamically (no --peers on CLI or env var needed)
    void addRelayConnector(const QString& relayEp);
    bool isRunning() const;

    QString virtualIp() const { return m_virtualIp; }
    quint16 centerPort() const { return m_centerPort; }
    quint16 rpcPort() const { return m_rpcPort; }

    // Parse peer list JSON from easytier-cli peer -o json and return structured info
    QList<EasyTierPeerInfo> parsePeerListJson(const QString& jsonOutput) const;

    // Local peer NAT type, refreshed on every peer-list poll (text table parse)
    EasyTierNatType currentNatType() const { return m_currentNatType; }

signals:
    void networkReady(const QString& virtualIp);
    void virtualIpChanged(const QString& ip);
    void errorOccurred(const QString& msg);
    void stateChanged(const QString& state);
    // Emitted when the local peer's NAT type is (re)detected from the peer table
    void localNatTypeChanged(int natType);

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
    QString m_peerConfigPath;

    // CLI polling for peer status
    QProcess* m_cliProcess = nullptr;

    // RPC port for easytier-cli communication
    quint16 m_rpcPort = 0;

    // Local peer NAT type (extracted from peer table text, NOT just logged)
    EasyTierNatType m_currentNatType = EasyTierNatType::Unknown;

    // Parse a single table cell / string into a NAT type (keyword scanning, order matters)
    static EasyTierNatType parseNatCell(const QString& cell);
};
