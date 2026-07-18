// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once
#include <QObject>
#include <atomic>
#include <QTimer>

// ────────────────────────────────────────────────────────────
// ProgressTracker — unified download progress & speed engine
// Replaces 3 scattered EWMA instances with a single 200ms-tick
// timer-driven loop. All download sources (MC / Forge / Fabric
// / OptiFine / Fabric API) increment the same atomic counter,
// and the timer computes speed + progress on a stable cadence.
// ────────────────────────────────────────────────────────────

class ProgressTracker : public QObject {
    Q_OBJECT

public:
    explicit ProgressTracker(QObject *parent = nullptr);
    ~ProgressTracker() override = default;

    // ── Byte accumulation (called from any thread / callback) ──
    // O(1) atomic add — no computation on the hot path
    void addBytes(qint64 delta);
    void setTotalBytes(qint64 total);

    // ── Phase control ──
    enum Phase { Download, Verify, Install, Idle };
    void setPhase(Phase phase);
    Phase phase() const { return m_phase.load(); }

    // ── Query interface (UI layer) ──
    qreal smoothProgress() const { return m_smoothProgress.load(); }
    qint64 smoothSpeed() const { return m_smoothSpeed.load(); }
    qint64 downloadedBytes() const { return m_downloadedBytes.load(); }
    qint64 totalBytes() const { return m_totalBytes.load(); }
    bool isStalled() const { return m_stalled.load(); }

    // ── Reset for next install ──
    void reset();

signals:
    void progressChanged(qreal progress);
    void speedChanged(qint64 speed);
    void stalledChanged(bool stalled);

private slots:
    void onTick();

private:
    QTimer *m_timer;

    // Atomic accumulators (thread-safe for callback hot path)
    std::atomic<qint64> m_downloadedBytes{0};
    std::atomic<qint64> m_totalBytes{0};
    std::atomic<Phase> m_phase{Idle};

    // Tick-state (single-threaded, timer runs on main thread)
    qint64 m_lastSampleBytes = 0;
    qint64 m_lastSampleTimeMs = 0;
    int m_stalledTicks = 0;

    // Smoothed outputs (atomic for UI reads)
    std::atomic<qreal> m_smoothProgress{0.0};
    std::atomic<qint64> m_smoothSpeed{0};
    std::atomic<bool> m_stalled{false};

    static constexpr int kSampleIntervalMs = 200;
    static constexpr int kStallThresholdTicks = 3;   // 600ms → forced 0
    static constexpr int kMaxGapMs = 2000;            // pause-resume guard
};
