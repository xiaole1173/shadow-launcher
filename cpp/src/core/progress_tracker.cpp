// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "progress_tracker.h"
#include "utils/logger.h"
#include <QDateTime>
#include <QElapsedTimer>
#include <QDebug>

ProgressTracker::ProgressTracker(QObject *parent) : QObject(parent) {
    m_timer = new QTimer(this);
    m_timer->setInterval(kSampleIntervalMs);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &ProgressTracker::onTick);
}

void ProgressTracker::addBytes(qint64 delta) {
    m_downloadedBytes.fetch_add(delta);
}

void ProgressTracker::setTotalBytes(qint64 total) {
    m_totalBytes.store(total);
}

void ProgressTracker::setPhase(Phase phase) {
    Phase old = m_phase.exchange(phase);
    if (phase != old) {
        if (phase == Download) {
            m_lastSampleBytes = m_downloadedBytes.load();
            m_lastSampleTimeMs = QDateTime::currentMSecsSinceEpoch();
            m_timer->start();
        } else if (phase == Idle) {
            m_timer->stop();
            m_smoothSpeed.store(0);
            m_stalled.store(false);
            emit speedChanged(0);
        }
    }
}

void ProgressTracker::reset() {
    m_timer->stop();
    m_downloadedBytes.store(0);
    m_totalBytes.store(0);
    m_phase.store(Idle);
    m_lastSampleBytes = 0;
    m_lastSampleTimeMs = 0;
    m_stalledTicks = 0;
    m_smoothProgress.store(0.0);
    m_smoothSpeed.store(0);
    m_stalled.store(false);
}

void ProgressTracker::onTick() {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 currentBytes = m_downloadedBytes.load();

    qint64 dt = now - m_lastSampleTimeMs;
    qint64 delta = currentBytes - m_lastSampleBytes;

    // ── Phase guard: only compute speed during Download ──
    if (m_phase.load() != Download) {
        m_lastSampleBytes = currentBytes;
        m_lastSampleTimeMs = now;
        return;
    }

    // ── Pause-resume guard: gap too large → discard cumulative delta ──
    if (dt > kMaxGapMs) {
        m_lastSampleBytes = currentBytes;
        m_lastSampleTimeMs = now;
        return;
    }

    // ── Progress calculation (byte-weighted, clipped) ──
    qint64 total = m_totalBytes.load();
    if (total > 0) {
        qreal raw = qBound(0.0, (qreal)currentBytes / total, 1.0);
        qreal current = m_smoothProgress.load();
        qreal next = current * 0.7f + raw * 0.3f;
        m_smoothProgress.store(next);
        emit progressChanged(next);
    }

    // ── Speed: EWMA (α=0.3) with stall detection ──
    if (delta <= 0) {
        m_stalledTicks++;
        if (m_stalledTicks >= kStallThresholdTicks) {
            if (!m_stalled.exchange(true)) {
                emit stalledChanged(true);
            }
            m_smoothSpeed.store(0);
            emit speedChanged(0);
        }
    } else {
        m_stalledTicks = 0;
        if (m_stalled.exchange(false)) {
            emit stalledChanged(false);
        }
        // min dt = tick interval to cap burst spikes
        qint64 effDt = qMax(dt, (qint64)kSampleIntervalMs);
        qint64 instant = delta * 1000 / effDt;
        qint64 oldSpeed = m_smoothSpeed.load();
        qint64 newSpeed = (oldSpeed > 0)
            ? (oldSpeed * 7 + instant * 3) / 10
            : instant * 3 / 10;
        m_smoothSpeed.store(newSpeed);
        emit speedChanged(newSpeed);
    }

    m_lastSampleBytes = currentBytes;
    m_lastSampleTimeMs = now;
}
