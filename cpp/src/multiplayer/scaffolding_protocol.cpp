// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "scaffolding_protocol.h"
#include <QDataStream>
#include <QIODevice>

namespace Scaffolding {

QStringList basicProtocols()
{
    return { kPing, kProtocols, kServerPort, kPlayerPing, kPlayerProfilesList };
}

QByteArray buildPacket(const QString& type, const QByteArray& body)
{
    QByteArray typeBytes = type.toUtf8();
    quint8 typeLen = static_cast<quint8>(typeBytes.size());
    quint32 bodyLen = static_cast<quint32>(body.size());

    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << typeLen;
    stream.writeRawData(typeBytes.constData(), typeLen);
    stream << bodyLen;
    if (bodyLen > 0)
        stream.writeRawData(body.constData(), bodyLen);

    return packet;
}

QString packProtocolList(const QStringList& protocols)
{
    return protocols.join(QChar::fromLatin1('\0'));
}

QStringList unpackProtocolList(const QString& packed)
{
    return packed.split(QChar::fromLatin1('\0'), Qt::SkipEmptyParts);
}

} // namespace Scaffolding
