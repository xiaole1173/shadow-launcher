// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QNetworkAccessManager>

namespace ShadowLauncher {

/// Lightweight GeoIP service.
/// Uses ip-api.com (free, no API key needed).
/// Caches result in memory + QSettings for 24h.
class GeoIpService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString cachedRegion READ cachedRegion NOTIFY regionChanged)
    Q_PROPERTY(bool ready READ isReady NOTIFY regionChanged)

public:
    explicit GeoIpService(QObject* parent = nullptr);

    /// Trigger an async region detection.
    /// If cache is fresh (<24h), emits regionDetected immediately.
    /// Otherwise sends HTTP request to ip-api.com.
    Q_INVOKABLE void detectRegion();

    /// Cached region code (ISO 3166-1 alpha-2, e.g. "CN", "HK", "TW").
    /// Empty string if not yet detected.
    QString cachedRegion() const { return m_cachedRegion; }

    /// True if at least one successful detection has been made (cached).
    bool isReady() const { return !m_cachedRegion.isEmpty(); }

signals:
    /// Region successfully detected (or loaded from cache).
    void regionDetected(const QString& regionCode);

    /// Detection failed (network error or unexpected response).
    void detectionFailed(const QString& error);

    /// Emitted when cachedRegion or ready changes.
    void regionChanged();

private slots:
    void onReplyFinished();

private:
    void loadCache();
    void saveCache(const QString& region);

    QString m_cachedRegion;
    QDateTime m_cacheTime;
    QNetworkAccessManager* m_nam = nullptr;
};

} // namespace ShadowLauncher
