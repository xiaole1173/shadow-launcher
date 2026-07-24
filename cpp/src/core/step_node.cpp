// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "step_node.h"

namespace ShadowLauncher {

StepNode::StepNode(const QString& key, const QString& displayName,
                   qreal weight, QObject* parent)
    : QObject(parent)
    , m_key(key)
    , m_name(displayName)
    , m_weight(qMax(0.01, weight))
{
}

void StepNode::setStatus(StepStatus s) {
    if (m_status == s) return;
    m_status = s;
    emit statusChanged();
}

void StepNode::setFailed(const QString& error) {
    m_errorMsg = error;
    setStatus(StepStatus::Failed);
}

void StepNode::setPercentage(int pct) {
    pct = qBound(0, pct, 100);
    if (m_percentage == pct) return;
    m_percentage = pct;
    emit progressChanged();
}

void StepNode::setByteProgress(qint64 recv, qint64 total) {
    if (m_bytesRecv == recv && m_bytesTotal == total) return;
    m_bytesRecv = recv;
    m_bytesTotal = total;
    if (total > 0) {
        int pct = static_cast<int>(recv * 100 / total);
        if (pct != m_percentage) {
            m_percentage = pct;
        }
    }
    emit progressChanged();
}

void StepNode::setHidden(bool h) {
    if (m_hidden == h) return;
    m_hidden = h;
    emit hiddenChanged();
}

} // namespace ShadowLauncher
