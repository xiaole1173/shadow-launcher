// SPDX-License-Identifier: AGPL-3.0-or-later
// Secure memory wipe utilities — Windows SecureZeroMemory backed.
#pragma once
#include <QString>
#include <QByteArray>
#include <windows.h>

inline void secureWipe(QString &s)
{
    if (s.isEmpty()) return;
    // data() detaches if shared; we own the buffer after this
    SecureZeroMemory(s.data(), s.size() * sizeof(QChar));
    // Secondary fill to catch any Qt-internal buffer copies
    s.fill(QChar(0));
    s.clear();
}

inline void secureWipe(QByteArray &b)
{
    if (b.isEmpty()) return;
    SecureZeroMemory(b.data(), b.size());
    b.fill(0);
    b.clear();
}
