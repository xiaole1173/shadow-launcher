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

void StepModel::notifyStatus(int row) {
    if (row < 0 || row >= m_nodes.size()) return;
    emit dataChanged(index(row), index(row), {StatusIntRole});
}

void StepModel::notifyProgress(int row) {
    if (row < 0 || row >= m_nodes.size()) return;
    emit dataChanged(index(row), index(row), {PercentageRole, BytesRecvRole, BytesTotalRole});
}

void StepModel::notifyHidden(int row) {
    if (row < 0 || row >= m_nodes.size()) return;
    emit dataChanged(index(row), index(row), {HiddenRole});
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

    // 监听完成信号
    connect(node, &StepNode::statusChanged, this, [this, key]() {
        auto* n = step(key);
        if (n && n->status() == StepStatus::Completed) {
            advance();
        }
    });

    return node;
}

StepNode* StepPipeline::insertAfter(const QString& afterKey, const QString& key,
                                     const QString& name, qreal weight) {
    if (m_keyIndex.contains(key)) return nullptr;
    auto it = m_keyIndex.constFind(afterKey);
    if (it == m_keyIndex.constEnd()) return nullptr;

    int idx = it.value() + 1;
    auto* node = new StepNode(key, name, weight, this);
    m_model->insertStep(idx, node);

    // 更新 m_keyIndex 中所有索引 ≥ idx 的条目
    for (auto kit = m_keyIndex.begin(); kit != m_keyIndex.end(); ++kit) {
        if (kit.value() >= idx) kit.value()++;
    }
    m_keyIndex[key] = idx;

    connect(node, &StepNode::statusChanged, this, [this, key]() {
        auto* n = step(key);
        if (n && n->status() == StepStatus::Completed) {
            advance();
        }
    });

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
