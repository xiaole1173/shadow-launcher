// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "room_code.h"
#include <QRandomGenerator>
#include <QRegularExpression>

namespace RoomCode {

// Character set: 0-9, A-H, J-N, P-Z  (total 34 chars, mapping to [0, 33])
static const char kCharset[] = "0123456789ABCDEFGHJKLMNPQRSTUVWXYZ";
static constexpr int kCharsetSize = 34;

// Map single character to 0..33 value
static int charToValue(QChar c) {
    c = c.toUpper();
    if (c >= '0' && c <= '9') return c.unicode() - '0';
    if (c >= 'A' && c <= 'H') return 10 + (c.unicode() - 'A');
    if (c >= 'J' && c <= 'N') return 18 + (c.unicode() - 'J');
    if (c >= 'P' && c <= 'Z') return 23 + (c.unicode() - 'P');
    return -1;
}

// Random character from charset
static QChar randomChar() {
    int idx = QRandomGenerator::global()->bounded(kCharsetSize);
    return QChar::fromLatin1(kCharset[idx]);
}

// Generate a segment of length 'len' with valid checksum
static QString generateSegment(int len) {
    QString seg;
    for (int i = 0; i < len; ++i)
        seg += randomChar();

    // Ensure checksum: map chars to [0,33], read as little-endian integer, mod 7 == 0
    while (true) {
        qint64 value = 0;
        for (int i = 0; i < seg.size(); ++i) {
            int v = charToValue(seg[i]);
            value += static_cast<qint64>(v) << (5 * i);  // 5 bits per char (0-33)
        }
        if (value % 7 == 0)
            break;
        // Regenerate last char
        seg[seg.size() - 1] = randomChar();
    }
    return seg;
}

// Validate checksum for a given string segment
static bool validateChecksum(const QString& seg) {
    qint64 value = 0;
    for (int i = 0; i < seg.size(); ++i) {
        int v = charToValue(seg[i]);
        if (v < 0) return false;
        value += static_cast<qint64>(v) << (5 * i);
    }
    return value % 7 == 0;
}

Parts generate()
{
    Parts p;
    QString nn1 = generateSegment(4);
    QString nn2 = generateSegment(4);
    QString ss1 = generateSegment(4);
    QString ss2 = generateSegment(4);

    p.networkName = QStringLiteral("scaffolding-mc-") + nn1 + QStringLiteral("-") + nn2;
    p.networkKey = ss1 + QStringLiteral("-") + ss2;
    p.displayCode = QStringLiteral("U/") + nn1 + QStringLiteral("-") + nn2
                    + QStringLiteral("-") + ss1 + QStringLiteral("-") + ss2;
    return p;
}

std::optional<Parts> parse(const QString& code)
{
    // Match: U/NNNN-NNNN-SSSS-SSSS
    static QRegularExpression rx(
        QStringLiteral("^U/([0-9A-HJ-NP-Z]{4})-([0-9A-HJ-NP-Z]{4})-([0-9A-HJ-NP-Z]{4})-([0-9A-HJ-NP-Z]{4})$"),
        QRegularExpression::CaseInsensitiveOption
    );
    auto m = rx.match(code);
    if (!m.hasMatch())
        return std::nullopt;

    QString nn1 = m.captured(1).toUpper();
    QString nn2 = m.captured(2).toUpper();
    QString ss1 = m.captured(3).toUpper();
    QString ss2 = m.captured(4).toUpper();

    // Validate checksum for all four segments
    if (!validateChecksum(nn1) || !validateChecksum(nn2) ||
        !validateChecksum(ss1) || !validateChecksum(ss2))
        return std::nullopt;

    Parts p;
    p.networkName = QStringLiteral("scaffolding-mc-") + nn1 + QStringLiteral("-") + nn2;
    p.networkKey = ss1 + QStringLiteral("-") + ss2;
    p.displayCode = code.toUpper();
    return p;
}

bool isValidFormat(const QString& code)
{
    return parse(code).has_value();
}

} // namespace RoomCode
