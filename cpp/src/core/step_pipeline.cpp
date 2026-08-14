// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "step_pipeline.h"
#include <algorithm>

namespace ShadowLauncher {

// ── StepModel ──

StepModel::StepModel(QObject* parent) : QAbstractListModel(parent) {}

int StepModel::rowCount(const QModelIndex&) const {
    return m_nodes.size();
}

QVariant StepModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_nodes.size())
        return {};

    auto* node = m_nodes[index.row()];
    if (!node) return {};

    switch (role) {
    case KeyRole:          return node->key();
    case NameRole:         return node->name();
    case StatusIntRole:    return node->statusInt();
    case PercentageRole:   return node->percentage();
    case BytesRecvRole:    return node->bytesRecv();
    case BytesTotalRole:   return node->bytesTotal();
    case HiddenRole:       return node->isHidden();
    default:               return {};
    }
}

QHash<int, QByteArray> StepModel::roleNames() const {
    return {
        {KeyRole,         "stepKey"},
        {NameRole,        "name"},
        {StatusIntRole,   "statusInt"},
        {PercentageRole,  "percentage"},
        {BytesRecvRole,   "bytesRecv"},
        {BytesTotalRole,  "bytesTotal"},
        {HiddenRole,      "hidden"}
    };
}

void StepModel::appendStep(StepNode* step) {
    if (!step) return;
    int row = m_nodes.size();
    beginInsertRows({}, row, row);
    m_nodes.append(step);
    endInsertRows();
}

void StepModel::insertStep(int index, StepNode* step) {
    if (!step || index < 0 || index > m_nodes.size()) return;
    beginInsertRows({}, index, index);
    m_nodes.insert(index, step);
    endInsertRows();
}

void StepModel::removeStep(int index) {
    if (index < 0 || index >= m_nodes.size()) return;
    beginRemoveRows({}, index, index);
    m_nodes.removeAt(index);
    endRemoveRows();
}

StepNode* StepModel::stepAt(int index) const {
    if (index < 0 || index >= m_nodes.size()) return nullptr;
    return m_nodes[index];
}

// ── StepPipeline ──

StepPipeline::StepPipeline(QObject* parent)
    : QObject(parent)
{
    m_model = new StepModel(this);
}

StepPipeline::~StepPipeline() = default;

StepNode* StepPipeline::addStep(const QString& key, const QString& name, qreal weight) {
    if (m_keyIndex.contains(key)) return nullptr;  // key 已存在

    auto* node = new StepNode(key, name, weight, this);
    int idx = m_model->stepCount();
    m_model->appendStep(node);
    m_keyIndex[key] = idx;

    // ── 2026-08-15：移除自动推进钩子 ──
    // 原实现监听每个 StepNode 的 statusChanged：任意步骤 Completed → advance()
    // → 把 m_currentIndex+1 的步骤无条件 setActive()。
    // 这在顺序执行场景（纯 MC 下载）下成立，但 merged 安装是**并行**的：
    // MC 下载与 NeoForge 主文件/安装器库同时进行、乱序完成。例如：
    //   · "下载原版支持库文件"完成后 advance → 误把"下载NeoForge主文件"
    //     StepNode setActive（用户实测：主文件被转回进行态，进度 100%）
    //   · "下载原版资源文件"完成后 advance → 误把"校验NeoForge完整性"
    //     StepNode setActive（同样 100% 却冒进行态）
    //   · 主文件/校验提前完成后 advance 顺序错位，MC 校验(idx=3)被误激活
    // advance() 只改 StepNode 不改 ds->steps → UI(pipeline) 显示 active 而
    // 日志(ds->steps)显示 completed —— 正是"100% 不转完成态"的根源。
    // 修复：步骤状态完全由 updateStep/showStep 显式驱动（merged 与纯 MC
    // 的 updateStep 都是显式的）；weightedProgress() 遍历全节点不受影响；
    // QML phase 由 steps 状态 derivePhase 驱动，不依赖 currentIndex。
    // advance()/currentStepIndex() 保留为显式 API（调用方主动推进时使用）。

    return node;
}

void StepPipeline::removeStep(const QString& key) {
    auto it = m_keyIndex.find(key);
    if (it == m_keyIndex.end()) return;

    int idx = it.value();
    m_model->removeStep(idx);
    m_keyIndex.erase(it);

    // 更新 m_keyIndex
    for (auto kit = m_keyIndex.begin(); kit != m_keyIndex.end(); ++kit) {
        if (kit.value() > idx) kit.value()--;
    }
}

void StepPipeline::clearSteps() {
    m_keyIndex.clear();
    m_currentIndex = -1;
    while (m_model->stepCount() > 0)
        m_model->removeStep(0);
}

StepNode* StepPipeline::step(const QString& key) {
    auto it = m_keyIndex.constFind(key);
    if (it == m_keyIndex.constEnd()) return nullptr;
    return m_model->stepAt(it.value());
}

int StepPipeline::indexOf(const QString& key) const {
    auto it = m_keyIndex.constFind(key);
    return it != m_keyIndex.constEnd() ? it.value() : -1;
}

qreal StepPipeline::weightedProgress() const {
    qreal totalWeight = 0;
    qreal weightedSum = 0;
    int count = m_model->stepCount();

    for (int i = 0; i < count; ++i) {
        auto* node = m_model->stepAt(i);
        if (!node || node->isHidden()) continue;
        qreal w = node->weight();
        totalWeight += w;
        weightedSum += w * node->percentage() / 100.0;
    }

    return totalWeight > 0 ? weightedSum / totalWeight : 0.0;
}

void StepPipeline::start() {
    if (m_model->stepCount() == 0) return;
    m_currentIndex = 0;
    auto* node = m_model->stepAt(0);
    if (node) {
        node->setActive();
        emit stepActivated(node->key(), 0);
    }
    emit pipelineStarted();
}

bool StepPipeline::advance() {
    if (m_currentIndex < 0) return false;

    int nextIdx = m_currentIndex + 1;
    if (nextIdx >= m_model->stepCount()) {
        // 所有步骤完成
        emit pipelineFinished(true);
        return false;
    }

    // 跳过 hidden 步骤
    while (nextIdx < m_model->stepCount()) {
        auto* node = m_model->stepAt(nextIdx);
        if (node && !node->isHidden()) break;
        nextIdx++;
    }

    if (nextIdx >= m_model->stepCount()) {
        emit pipelineFinished(true);
        return false;
    }

    m_currentIndex = nextIdx;
    auto* node = m_model->stepAt(nextIdx);
    if (node) {
        node->setActive();
        emit stepActivated(node->key(), nextIdx);
    }
    return true;
}

void StepPipeline::cancel() {
    for (int i = 0; i < m_model->stepCount(); ++i) {
        auto* node = m_model->stepAt(i);
        if (node) node->cancel();
    }
    m_currentIndex = -1;
    emit pipelineFinished(false);
}

} // namespace ShadowLauncher
