// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "geoip_service.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QDebug>

namespace ShadowLauncher {

static constexpr int kCacheDurationSecs = 86400;  // 24h
static const QString kSettingsKey = QStringLiteral("general/geoRegion");
static const QString kSettingsTimeKey = QStringLiteral("general/geoRegionTime");

GeoIpService::GeoIpService(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    loadCache();
}

void GeoIpService::detectRegion()
{
    // If cache is fresh, use it immediately
    if (!m_cachedRegion.isEmpty() && m_cacheTime.isValid()) {
        qint64 elapsed = m_cacheTime.secsTo(QDateTime::currentDateTimeUtc());
        if (elapsed < kCacheDurationSecs) {
            qDebug().noquote() << "[GeoIP] Cache hit, region:" << m_cachedRegion
                               << "age:" << elapsed << "s";
            emit regionDetected(m_cachedRegion);
            return;
        }
        qDebug().noquote() << "[GeoIP] Cache expired, age:" << elapsed << "s";
    }

    QNetworkRequest req(QUrl(QStringLiteral("http://ip-api.com/json/")));
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    req.setTransferTimeout(5000);

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, &GeoIpService::onReplyFinished);
}

void GeoIpService::onReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning().noquote() << "[GeoIP] Request failed:" << reply->errorString();
        emit detectionFailed(reply->errorString());
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qWarning().noquote() << "[GeoIP] Invalid JSON response";
        emit detectionFailed(QStringLiteral("Invalid JSON response"));
        return;
    }

    QJsonObject obj = doc.object();
    QString status = obj[QStringLiteral("status")].toString();
    if (status != QStringLiteral("success")) {
        QString msg = obj[QStringLiteral("message")].toString(QStringLiteral("unknown error"));
        qWarning().noquote() << "[GeoIP] API error:" << msg;
        emit detectionFailed(msg);
        return;
    }

    QString region = obj[QStringLiteral("countryCode")].toString().toUpper().trimmed();
    if (region.isEmpty()) {
        qWarning().noquote() << "[GeoIP] Empty countryCode in response";
        emit detectionFailed(QStringLiteral("Empty countryCode"));
        return;
    }

    qDebug().noquote() << "[GeoIP] Detected region:" << region;
    m_cachedRegion = region;
    m_cacheTime = QDateTime::currentDateTimeUtc();
    saveCache(region);
    emit regionDetected(region);
    emit regionChanged();
}

void GeoIpService::loadCache()
{
    QSettings s;
    m_cachedRegion = s.value(kSettingsKey).toString();
    m_cacheTime = s.value(kSettingsTimeKey).toDateTime();
    if (!m_cachedRegion.isEmpty() && m_cacheTime.isValid()) {
        qint64 elapsed = m_cacheTime.secsTo(QDateTime::currentDateTimeUtc());
        qDebug().noquote() << "[GeoIP] Loaded from cache, region:" << m_cachedRegion
                           << "age:" << elapsed << "s";
    }
}

void GeoIpService::saveCache(const QString& region)
{
    QSettings s;
    s.setValue(kSettingsKey, region);
    s.setValue(kSettingsTimeKey, QDateTime::currentDateTimeUtc());
}

} // namespace ShadowLauncher
