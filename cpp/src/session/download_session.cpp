// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "download_session.h"
#include <algorithm>
#include <limits>

namespace ShadowLauncher {

// ══════════════════════════════════════════════
// Construction / Destruction
// ══════════════════════════════════════════════

DownloadSession::DownloadSession(const QString& versionId, QObject* parent)
    : QObject(parent)
    , m_versionId(versionId)
    , m_sessionId(versionId)
{
    m_pipeline = new StepPipeline(this);
    m_sessionTimer.start();
}

DownloadSession::~DownloadSession() = default;

// ══════════════════════════════════════════════
// Progress / Speed (3-second sliding window)
// ══════════════════════════════════════════════

qreal DownloadSession::totalProgress() const {
    return m_pipeline ? m_pipeline->weightedProgress() : 0.0;
}

void DownloadSession::recordBytes(qint64 bytesRecv, qint64 bytesTotal) {
    qint64 now = m_sessionTimer.elapsed();

    // ── 瞬时速度: delta / elapsed ──
    qint64 deltaBytes = bytesRecv - m_lastRecvBytes;
    qint64 deltaMs = now - m_lastRecvTime;
    m_lastRecvBytes = bytesRecv;
    m_lastRecvTime = now;

    if (deltaBytes > 0 && deltaMs > 0) {
        // 瞬时速度: 不用 qMax — 速度可以自然升降
        m_speed = (deltaBytes * 1000) / deltaMs;
    } else if (deltaBytes == 0 && deltaMs > 30000) {
        // 30 秒无数据 → 速度归零
        m_speed = 0;
    }
    // deltaBytes == 0 && within 30s: 保持上次速度不变

    emit progressUpdated();
}

void DownloadSession::resetSpeed() {
    m_speed = 0;
    m_lastRecvBytes = 0;
    m_lastRecvTime = 0;
}

// ══════════════════════════════════════════════
// Control
// ══════════════════════════════════════════════

void DownloadSession::startPipeline() {
    if (m_pipeline && m_pipeline->currentStepIndex() < 0) {
        m_pipeline->start();
    }
}

void DownloadSession::cancel() {
    if (m_pipeline) {
        m_pipeline->cancel();
    }
    m_failed = false;
    m_speed = 0;
    m_lastRecvBytes = 0;
    m_lastRecvTime = 0;
}

void DownloadSession::markFailed(const QString& err) {
    m_failed = true;
    m_error = err;
    emit progressUpdated();
}

void DownloadSession::reset() {
    m_failed = false;
    m_error.clear();
    m_speed = 0;
    m_lastRecvBytes = 0;
    m_lastRecvTime = 0;
    m_isMerged = false;

    // Reset old struct fields
    mcDownloadDone = false;
    for (int i = 0; i < 3; ++i) mcStepDone[i] = mcStepTotal[i] = 0;
    mcFileAdded.clear();
    mcBytesDl = mcBytesAll = 0;

    loaderDownloadReady = false;
    loaderDownloadData.clear();
    loaderVerifyStep = -1;
    loaderStepIdx = -1;
    loaderType.clear();
    loaderVer.clear();
    hasPendingLoader = false;
    pendingLoaderName.clear();
    pendingLoaderVer.clear();
    pendingLoaderWeight = 0.0;
    pendingLoaderMc.clear();
    pendingLoaderType.clear();
    forgeInstallerSha1.clear();
    fabricApiVersion.clear();
    fabricApiUrl.clear();
    fabricApiSavePath.clear();
    fabricApiFinalPath.clear();
    fabricApiPending = false;
    loaderFinishedWaitingMC = false;
    optifineJarParallel = false;
    optifineJarDone = false;
    optifineInstallTriggered = false;
    optifineJarData.clear();
    bmclType.clear();
    bmclPatch.clear();

    mlBytesDl = mlBytesAll = mlBytesDone = 0;
    
    mlFileTotal = 0;

    smoothProgress = 0.0;
    mcVersion.clear();
    steps.clear();
    m_rawTotalProgress = 0.0;

    hasImportPending = false;
    importArchivePath.clear();
    importFailedAtMs = 0;

    loadedStep = 0;
}

void DownloadSession::appendError(const QString& err) {
    if (m_error.isEmpty()) {
        m_error = err;
    } else {
        m_error += QStringLiteral("\n") + err;
    }
}

// ══════════════════════════════════════════════
// Card data export
// ══════════════════════════════════════════════

InstallCard DownloadSession::toCard(const QString& iid, const QString& name, const QString& type) const {
    InstallCard card;
    card.iid = iid;
    card.name = name;
    card.type = type;
    card.progress = totalProgress();
    card.speed = m_speed;
    card.failed = m_failed;
    card.error = m_error;
    card.canCancel = true;
    card.totalProgressVisible = true;
    card.steps = steps;
    card.phase = m_pipeline && m_pipeline->currentStepIndex() >= 0
                     ? (m_pipeline->currentStep() ? m_pipeline->currentStep()->name() : QString{})
                     : QString{};
    return card;
}

} // namespace ShadowLauncher
