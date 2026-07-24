// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QAbstractListModel>
#include <QVector>
#include <QHash>
#include <memory>

#include "step_node.h"

namespace ShadowLauncher {

// ── StepModel — QML 可绑定的步骤列表模型 ──
// 每个步骤独立通知，非全量更新
class StepModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        KeyRole = Qt::UserRole + 1,
        NameRole,
        StatusIntRole,
        PercentageRole,
        BytesRecvRole,
        BytesTotalRole,
        HiddenRole
    };

    explicit StepModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // ── 增量更新接口 ──
    void appendStep(StepNode* step);
    void insertStep(int index, StepNode* step);
    void removeStep(int index);
    StepNode* stepAt(int index) const;
    int stepCount() const { return m_nodes.size(); }

    // ── 通知 QML 单行/单角色更新 ──
    void notifyStatus(int row);
    void notifyProgress(int row);
    void notifyHidden(int row);

private:
    QVector<StepNode*> m_nodes;  // 非 owning
};

// ── StepPipeline — 步骤管线 ──
// 管理一组 StepNode 的增删查
class StepPipeline : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* model CONSTANT)  // QML uses QObject* pattern
public:
    explicit StepPipeline(QObject* parent = nullptr);
    ~StepPipeline() override;

    // Q_PROPERTY accessor (QObject* for QML compatibility)
    QObject* model() const { return m_model; }
    // C++ typed accessor
    StepModel* stepModel() const { return m_model; }

    // ── 管线构建 ──
    StepNode* addStep(const QString& key, const QString& name, qreal weight = 1.0);
    StepNode* insertAfter(const QString& afterKey, const QString& key,
                          const QString& name, qreal weight = 1.0);
    void removeStep(const QString& key);
    void clearSteps();

    // ── 步骤访问 ──
    StepNode* step(const QString& key);
    StepNode* stepNode(int index) const { return (index >=0 && index < m_model->stepCount()) ? m_model->stepAt(index) : nullptr; }
    int indexOf(const QString& key) const;
    int totalSteps() const { return m_model->stepCount(); }
    int currentStepIndex() const { return m_currentIndex; }
    StepNode* currentStep() const { return m_currentIndex >= 0 ? m_model->stepAt(m_currentIndex) : nullptr; }

    // ── 进度 ──
    qreal weightedProgress() const;

    // ── 控制 ──
    void start();   // 激活第 0 步
    bool advance(); // 当前步完成 → 激活下一步
    void cancel();  // 取消所有步骤

signals:
    void pipelineStarted();
    void pipelineFinished(bool success);
    void stepActivated(const QString& key, int index);

private:
    StepModel* m_model = nullptr;
    QHash<QString, int> m_keyIndex;  // key → index
    int m_currentIndex = -1;
};

} // namespace ShadowLauncher
