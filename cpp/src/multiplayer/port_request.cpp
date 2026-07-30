// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "port_request.h"
#include <QTcpServer>
#include <QHostAddress>

namespace PortRequest {

quint16 requestSpecific(quint16 port)
{
    // Aligns with Terracotta: TcpListener::bind((Ipv4Addr::LOCALHOST, port))
    QTcpServer server;
    if (server.listen(QHostAddress::LocalHost, port)) {
        quint16 boundPort = server.serverPort();
        server.close();  // release immediately — we just probed
        return boundPort;
    }
    return 0;  // port is occupied
}

quint16 requestFree(quint16 basePort)
{
    // Aligns with Terracotta: TcpListener::bind((Ipv4Addr::LOCALHOST, 0))
    QTcpServer server;
    if (server.listen(QHostAddress::LocalHost, 0)) {
        quint16 boundPort = server.serverPort();
        server.close();
        return boundPort;
    }
    // Fallback: Terracotta's default is self as u8 as u16 + 35780
    return static_cast<quint16>(basePort + 35780);
}

} // namespace PortRequest
