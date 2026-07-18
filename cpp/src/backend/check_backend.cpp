// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "check_backend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <QFile>
#endif

namespace ShadowLauncher {

// ────────────────────────────────────────────────────────────
// Constructor
// ────────────────────────────────────────────────────────────

CheckBackend::CheckBackend(QObject *parent)
    : QObject(parent)
{
}

// ────────────────────────────────────────────────────────────
// P0-1: Java 可执行文件校验 + 架构检测
// ────────────────────────────────────────────────────────────

QVariantMap CheckBackend::checkJava(const QString &javaPath)
{
    QVariantMap result;
    result[QStringLiteral("passed")] = false;
    result[QStringLiteral("arch")] = QString();
    result[QStringLiteral("error")] = QString();
    result[QStringLiteral("warning")] = QString();

    // ── 文件是否存在 ──
    QFileInfo fi(javaPath);
    if (!fi.exists() || !fi.isFile()) {
        result[QStringLiteral("error")] =
            QStringLiteral("Java executable not found: ") + javaPath;
        emit logMessage(result[QStringLiteral("error")].toString());
        return result;
    }

    // ── 是否可执行 ──
    QProcess proc;
    proc.setProgram(javaPath);
    proc.setArguments({QStringLiteral("-version")});
    proc.start();
    if (!proc.waitForFinished(5000)) {
        proc.kill();
        proc.waitForFinished(1000);
        result[QStringLiteral("error")] =
            QStringLiteral("Java executable timed out or failed: ") + javaPath;
        emit logMessage(result[QStringLiteral("error")].toString());
        return result;
    }
    if (proc.exitCode() != 0) {
        result[QStringLiteral("error")] =
            QStringLiteral("java -version returned non-zero exit code: ")
            + QString::number(proc.exitCode());
        emit logMessage(result[QStringLiteral("error")].toString());
        return result;
    }

    // ── 架构检测 ──
    bool is64bit = false;
    QString archStr;
    if (checkJavaArch(javaPath, is64bit, archStr)) {
        result[QStringLiteral("arch")] = archStr;
        result[QStringLiteral("passed")] = true;

        if (!is64bit) {
            result[QStringLiteral("warning")] =
                QStringLiteral("32-bit Java detected. Limited to ~2GB heap memory. "
                               "64-bit Java is recommended for better performance.");
            emit logMessage(result[QStringLiteral("warning")].toString());
        } else {
            emit logMessage(QStringLiteral("Java check passed (") + archStr
                            + QStringLiteral(")"));
        }
    } else {
        // 架构未知但可执行 → 通过，标注未知
        result[QStringLiteral("arch")] = QStringLiteral("unknown");
        result[QStringLiteral("passed")] = true;
        result[QStringLiteral("warning")] =
            QStringLiteral("Could not detect Java architecture. Assuming 64-bit.");
        emit logMessage(result[QStringLiteral("warning")].toString());
    }

    return result;
}

// ────────────────────────────────────────────────────────────
// Java 架构检测：PE 头 → 回退到 java -version
// ────────────────────────────────────────────────────────────

bool CheckBackend::checkJavaArch(const QString &javaPath,
                                 bool &is64bit,
                                 QString &archStr)
{
    // ── 首选：PE 头检测（零耗时）──
    QFile file(javaPath);
    if (file.open(QIODevice::ReadOnly)) {
        // 读取 PE 签名偏移（offset 60）
        if (file.seek(60)) {
            char buf[4];
            if (file.read(buf, 4) == 4) {
                quint32 peOffset = (quint32)(unsigned char)buf[0]
                                 | ((quint32)(unsigned char)buf[1] << 8)
                                 | ((quint32)(unsigned char)buf[2] << 16)
                                 | ((quint32)(unsigned char)buf[3] << 24);

                // 读取 machine type（PE signature + 4 bytes magic）
                if (file.seek(peOffset + 4)) {
                    char mBuf[2];
                    if (file.read(mBuf, 2) == 2) {
                        quint16 machine = (quint16)(unsigned char)mBuf[0]
                                        | ((quint16)(unsigned char)mBuf[1] << 8);

                        if (machine == 0x8664) {
                            is64bit = true;
                            archStr = QStringLiteral("64-bit");
                            return true;
                        } else if (machine == 0x014C) {
                            is64bit = false;
                            archStr = QStringLiteral("32-bit");
                            return true;
                        } else if (machine == 0xAA64) {
                            is64bit = true;
                            archStr = QStringLiteral("ARM64");
                            return true;
                        }
                    }
                }
            }
        }
        file.close();
    }

    // ── 回退：java -version 输出解析 ──
    QProcess proc;
    proc.setProgram(javaPath);
    proc.setArguments({QStringLiteral("-version")});
    proc.start();
    if (proc.waitForFinished(5000)) {
        QByteArray output = proc.readAllStandardError() + proc.readAllStandardOutput();
        QString out = QString::fromUtf8(output).toLower();

        if (out.contains(QStringLiteral("64-bit"))) {
            is64bit = true;
            archStr = QStringLiteral("64-bit");
            return true;
        } else if (out.contains(QStringLiteral("32-bit"))) {
            is64bit = false;
            archStr = QStringLiteral("32-bit");
            return true;
        }
    }

    return false; // unable to detect
}

// ────────────────────────────────────────────────────────────
// P0-2a: 核心 Jar 文件存在性
// ────────────────────────────────────────────────────────────

QVariantMap CheckBackend::checkVersionJar(const QString &versionId,
                                          const QString &gameDir)
{
    QVariantMap result;
    result[QStringLiteral("passed")] = false;
    result[QStringLiteral("path")] = QVariant();
    result[QStringLiteral("error")] = QVariant();

    QString jarPath = gameDir + QStringLiteral("/versions/") + versionId
                      + QStringLiteral("/") + versionId + QStringLiteral(".jar");

    QFileInfo fi(jarPath);
    if (!fi.exists() || !fi.isFile()) {
        result[QStringLiteral("error")] =
            QStringLiteral("Version jar not found: ") + jarPath;
        emit logMessage(result[QStringLiteral("error")].toString());
        return result;
    }

    if (fi.size() <= 0) {
        result[QStringLiteral("error")] =
            QStringLiteral("Version jar is empty: ") + jarPath;
        emit logMessage(result[QStringLiteral("error")].toString());
        return result;
    }

    result[QStringLiteral("passed")] = true;
    result[QStringLiteral("path")] = jarPath;
    emit logMessage(QStringLiteral("Version jar OK: ") + jarPath);
    return result;
}

// ────────────────────────────────────────────────────────────
// P0-2b: version.json 存在且格式正确
// ────────────────────────────────────────────────────────────

QVariantMap CheckBackend::checkVersionJson(const QString &versionId,
                                           const QString &gameDir)
{
    QVariantMap result;
    result[QStringLiteral("passed")] = false;
    result[QStringLiteral("error")] = QVariant();

    QString jsonPath = gameDir + QStringLiteral("/versions/") + versionId
                       + QStringLiteral("/") + versionId + QStringLiteral(".json");

    QFileInfo fi(jsonPath);
    if (!fi.exists() || !fi.isFile()) {
        result[QStringLiteral("error")] =
            QStringLiteral("Version json not found: ") + jsonPath;
        emit logMessage(result[QStringLiteral("error")].toString());
        return result;
    }

    // 尝试解析 JSON
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        result[QStringLiteral("error")] =
            QStringLiteral("Cannot read version json: ") + jsonPath;
        emit logMessage(result[QStringLiteral("error")].toString());
        return result;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        result[QStringLiteral("error")] =
            QStringLiteral("Invalid JSON in ") + jsonPath
            + QStringLiteral(": ") + parseError.errorString();
        emit logMessage(result[QStringLiteral("error")].toString());
        return result;
    }

    if (!doc.isObject()) {
        result[QStringLiteral("error")] =
            QStringLiteral("Version json is not a JSON object: ") + jsonPath;
        emit logMessage(result[QStringLiteral("error")].toString());
        return result;
    }

    QJsonObject obj = doc.object();
    if (!obj.contains(QStringLiteral("id"))) {
        result[QStringLiteral("error")] =
            QStringLiteral("Version json missing 'id' field: ") + jsonPath;
        emit logMessage(result[QStringLiteral("error")].toString());
        return result;
    }

    result[QStringLiteral("passed")] = true;
    emit logMessage(QStringLiteral("Version json OK: ") + jsonPath);
    return result;
}

// ────────────────────────────────────────────────────────────
// P0-3: 内存分配检查
// ────────────────────────────────────────────────────────────

qint64 CheckBackend::getAvailableMemoryMB()
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memStatus)) {
        return static_cast<qint64>(memStatus.ullAvailPhys / (1024 * 1024));
    }
    return -1;
#else
    // Linux/Mac: 尝试读取 /proc/meminfo
    QFile meminfo(QStringLiteral("/proc/meminfo"));
    if (meminfo.open(QIODevice::ReadOnly)) {
        QByteArray content = meminfo.readAll();
        meminfo.close();

        // 解析 MemAvailable (kB)
        for (const QByteArray &line : content.split('\n')) {
            if (line.startsWith("MemAvailable:")) {
                QList<QByteArray> parts = line.simplified().split(' ');
                if (parts.size() >= 2) {
                    bool ok = false;
                    qint64 kb = parts.at(1).toLongLong(&ok);
                    if (ok) return kb / 1024;
                }
            }
        }

        // 回退：MemFree + Buffers + Cached
        qint64 memFree = -1, buffers = -1, cached = -1;
        for (const QByteArray &line : content.split('\n')) {
            if (line.startsWith("MemFree:")) {
                QList<QByteArray> parts = line.simplified().split(' ');
                if (parts.size() >= 2) memFree = parts.at(1).toLongLong();
            } else if (line.startsWith("Buffers:")) {
                QList<QByteArray> parts = line.simplified().split(' ');
                if (parts.size() >= 2) buffers = parts.at(1).toLongLong();
            } else if (line.startsWith("Cached:")) {
                QList<QByteArray> parts = line.simplified().split(' ');
                if (parts.size() >= 2) cached = parts.at(1).toLongLong();
            }
        }
        if (memFree >= 0) {
            qint64 total = memFree + (buffers > 0 ? buffers : 0)
                                  + (cached > 0 ? cached : 0);
            return total / 1024;
        }
    }
    return -1;
#endif
}

QVariantMap CheckBackend::checkMemory(int maxMemoryMB)
{
    QVariantMap result;
    result[QStringLiteral("passed")] = false;
    result[QStringLiteral("available")] = 0;
    result[QStringLiteral("error")] = QVariant();
    result[QStringLiteral("warning")] = QVariant();

    qint64 availMB = getAvailableMemoryMB();
    result[QStringLiteral("available")] = static_cast<int>(availMB);

    if (availMB < 0) {
        result[QStringLiteral("passed")] = true;
        result[QStringLiteral("warning")] =
            QStringLiteral("Could not detect available memory. Proceeding anyway.");
        emit logMessage(result[QStringLiteral("warning")].toString());
        return result;
    }

    // 最低 512MB
    if (maxMemoryMB < 512) {
        result[QStringLiteral("error")] =
            QStringLiteral("Allocated memory %1MB is too low (minimum 512MB).")
                .arg(maxMemoryMB);
        emit logMessage(result[QStringLiteral("error")].toString());
        return result;
    }

    // 不能超过可用内存
    if (maxMemoryMB > availMB) {
        result[QStringLiteral("error")] =
            QStringLiteral("Allocated memory %1MB exceeds available memory %2MB.")
                .arg(maxMemoryMB)
                .arg(availMB);
        emit logMessage(result[QStringLiteral("error")].toString());
        return result;
    }

    // 接近上限警告（超过 90%）
    if (maxMemoryMB > availMB * 0.9) {
        result[QStringLiteral("passed")] = true;
        result[QStringLiteral("warning")] =
            QStringLiteral("Allocated memory %1MB is close to system available %2MB; "
                           "system may lag.")
                .arg(maxMemoryMB)
                .arg(availMB);
        emit logMessage(result[QStringLiteral("warning")].toString());
        return result;
    }

    result[QStringLiteral("passed")] = true;
    emit logMessage(
        QStringLiteral("Memory OK: %1MB / %2MB available").arg(maxMemoryMB).arg(availMB));
    return result;
}

// ────────────────────────────────────────────────────────────
// P0-C: 综合校验（checkAll）
// ────────────────────────────────────────────────────────────

QVariantMap CheckBackend::checkAll(const QString &versionId,
                                   const QString &javaPath,
                                   int maxMemoryMB,
                                   const QString &gameDir)
{
    QStringList errors;
    QStringList warnings;

    // ── Java 检查 ──
    QVariantMap javaResult = checkJava(javaPath);
    if (!javaResult[QStringLiteral("passed")].toBool()) {
        errors.append(QStringLiteral("[Java] ")
                      + javaResult[QStringLiteral("error")].toString());
    }
    if (!javaResult[QStringLiteral("warning")].toString().isEmpty()) {
        warnings.append(QStringLiteral("[Java] ")
                        + javaResult[QStringLiteral("warning")].toString());
    }

    // ── Version Jar ──
    QVariantMap jarResult = checkVersionJar(versionId, gameDir);
    if (!jarResult[QStringLiteral("passed")].toBool()) {
        errors.append(QStringLiteral("[Jar] ")
                      + jarResult[QStringLiteral("error")].toString());
    }

    // ── Version JSON ──
    QVariantMap jsonResult = checkVersionJson(versionId, gameDir);
    if (!jsonResult[QStringLiteral("passed")].toBool()) {
        errors.append(QStringLiteral("[Json] ")
                      + jsonResult[QStringLiteral("error")].toString());
    }

    // ── Memory ──
    QVariantMap memResult = checkMemory(maxMemoryMB);
    if (!memResult[QStringLiteral("passed")].toBool()) {
        errors.append(QStringLiteral("[Memory] ")
                      + memResult[QStringLiteral("error")].toString());
    }
    if (!memResult[QStringLiteral("warning")].toString().isEmpty()) {
        warnings.append(QStringLiteral("[Memory] ")
                        + memResult[QStringLiteral("warning")].toString());
    }

    QVariantMap result;
    result[QStringLiteral("passed")] = errors.isEmpty();
    result[QStringLiteral("errors")] = errors;
    result[QStringLiteral("warnings")] = warnings;

    emit logMessage(errors.isEmpty()
        ? QStringLiteral("All pre-launch checks passed.")
        : QStringLiteral("Pre-launch checks failed: %1 error(s), %2 warning(s)")
              .arg(errors.size()).arg(warnings.size()));

    return result;
}

} // namespace ShadowLauncher
