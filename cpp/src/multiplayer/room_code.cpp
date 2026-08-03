// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "room_code.h"
#include <QRandomGenerator>
#include <QRegularExpression>

namespace RoomCode {

// ── Terracotta-compatible room code ──
// 对齐陶瓦联机 (Terracotta) src/controller/rooms/scaffolding/room.rs:
//   - 整个 16 字符房码 = 一个 base34 大整数 (34^16)，最低位在前
//   - 校验: 整体 value % 7 == 0（生成时强制，解析时验证）
//   - 字符集 0123456789ABCDEFGHJKLMNPQRSTUVWXYZ（不含 I/O）；解析时 I→1、O→0 兼容旧码
static const char kCharset[] = "0123456789ABCDEFGHJKLMNPQRSTUVWXYZ";
static constexpr int kCharsetSize = 34;
static constexpr int kCodeChars = 16;  // U/XXXX-XXXX-XXXX-XXXX 中的有效字符数

// Map single character to 0..33 value; I→1, O→0 (Terracotta lookup_char)
static int charToValue(QChar c) {
    c = c.toUpper();
    if (c == QLatin1Char('I')) return 1;
    if (c == QLatin1Char('O')) return 0;
    if (c >= '0' && c <= '9') return c.unicode() - '0';
    if (c >= 'A' && c <= 'H') return 10 + (c.unicode() - 'A');
    if (c >= 'J' && c <= 'N') return 18 + (c.unicode() - 'J');
    if (c >= 'P' && c <= 'Z') return 23 + (c.unicode() - 'P');
    return -1;
}

// Whole-code base34 value mod 7 (Terracotta: value.is_multiple_of(7)).
// chars 低位在前（index 0 = 最低位）；从最高位向最低位折叠取模，全程 mod 7 防溢出
// （等价于 128 位整体求值后再 mod 7，(a*34+b)%7 ≡ 逐步折叠）。
static int codeMod7(const QList<QChar>& chars) {
    int v = 0;
    for (int i = chars.size() - 1; i >= 0; --i)
        v = (v * 34 + charToValue(chars[i])) % 7;
    return v;
}

static QChar randomChar() {
    int idx = QRandomGenerator::global()->bounded(kCharsetSize);
    return QChar::fromLatin1(kCharset[idx]);
}

static Parts buildParts(const QList<QChar>& chars)
{
    Parts p;
    p.displayCode = QStringLiteral("U/");
    p.networkName = QStringLiteral("scaffolding-mc-");
    p.networkKey = QString();
    for (int i = 0; i < kCodeChars; ++i) {
        if (i == 4 || i == 8 || i == 12)
            p.displayCode += QLatin1Char('-');
        p.displayCode += chars[i];
        if (i < 8) {
            if (i == 4)
                p.networkName += QLatin1Char('-');
            p.networkName += chars[i];
        } else {
            if (i == 12)
                p.networkKey += QLatin1Char('-');
            p.networkKey += chars[i];
        }
    }
    return p;
}

Parts generate()
{
    // 随机 16 字符直到整体 value % 7 == 0（平均 7 次尝试，代价可忽略）
    // 与陶瓦 create_room 的 "value -= value % 7" 产出同一集合
    QList<QChar> chars;
    do {
        chars.clear();
        for (int i = 0; i < kCodeChars; ++i)
            chars.append(randomChar());
    } while (codeMod7(chars) != 0);
    return buildParts(chars);
}

std::optional<Parts> parse(const QString& code)
{
    // 结构: U/XXXX-XXXX-XXXX-XXXX（大小写不敏感；[0-9A-Z] 含 I/O，charToValue 兼容映射）
    static QRegularExpression rx(
        QStringLiteral("^U/([0-9A-Z]{4})-([0-9A-Z]{4})-([0-9A-Z]{4})-([0-9A-Z]{4})$"),
        QRegularExpression::CaseInsensitiveOption
    );
    auto m = rx.match(code.trimmed());
    if (!m.hasMatch())
        return std::nullopt;

    QList<QChar> chars;
    for (int i = 1; i <= 4; ++i) {
        for (QChar c : m.captured(i)) {
            if (charToValue(c) < 0)
                return std::nullopt;
            chars.append(c.toUpper());
        }
    }
    if (codeMod7(chars) != 0)
        return std::nullopt;

    return buildParts(chars);
}

bool isValidFormat(const QString& code)
{
    return parse(code).has_value();
}

} // namespace RoomCode
