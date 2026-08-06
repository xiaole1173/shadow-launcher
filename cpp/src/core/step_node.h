// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>

namespace ShadowLauncher {

// ── 步骤状态枚举 ──
enum class StepStatus {
    Pending,
    Active,
    Completed,
    Failed,
    Skipped
};

// ── StepNode — 步骤节点基类 ──
// 每个步骤是一个独立 QObject，自管状态/进度/字节计数
// 通过 Q_PROPERTY + NOTIFY 信号实现 QML 定向绑定
class StepNode : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString key READ key CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString detail READ detail NOTIFY detailChanged)
    Q_PROPERTY(int statusInt READ statusInt NOTIFY statusChanged)
    Q_PROPERTY(int percentage READ percentage NOTIFY progressChanged)
    Q_PROPERTY(qint64 bytesRecv READ bytesRecv NOTIFY progressChanged)
    Q_PROPERTY(qint64 bytesTotal READ bytesTotal NOTIFY progressChanged)
    Q_PROPERTY(bool hidden READ isHidden NOTIFY hiddenChanged)
public:
    explicit StepNode(const QString& key, const QString& displayName,
                      qreal weight = 1.0, QObject* parent = nullptr);

    QString key() const { return m_key; }
    QString name() const { return m_name; }
    qreal weight() const { return m_weight; }

    // ── 状态管理 ──
    StepStatus status() const { return m_status; }
    int statusInt() const { return static_cast<int>(m_status); }
    void setStatus(StepStatus s);
    void setStatus(const QString& s);  // convenience: "pending"|"active"|"completed"|"failed"

    void setActive() { setStatus(StepStatus::Active); }
    void setCompleted() { setStatus(StepStatus::Completed); if (m_percentage < 100) setPercentage(100); }
    void setFailed(const QString& error = {});
    void setSkipped() { setStatus(StepStatus::Skipped); }

    // ── 进度管理 ──
    int percentage() const { return m_percentage; }
    void setPercentage(int pct);

    qint64 bytesRecv() const { return m_bytesRecv; }
    qint64 bytesTotal() const { return m_bytesTotal; }
    void setByteProgress(qint64 recv, qint64 total);

    // ── 显隐管理 ──
    bool isHidden() const { return m_hidden; }
    void setHidden(bool h);

    QString errorMessage() const { return m_errorMsg; }

    // 步骤详情文字（如“剩余 x 个文件”），QML 步骤行右侧显示
    QString detail() const { return m_detail; }
    void setDetail(const QString& d);

    // ── 取消 ──
    virtual void cancel() {}

signals:
    void statusChanged();
    void progressChanged();
    void hiddenChanged();
    void detailChanged();

private:
    QString m_key;
    QString m_name;
    QString m_detail;
    qreal m_weight = 1.0;
    StepStatus m_status = StepStatus::Pending;
    int m_percentage = 0;
    qint64 m_bytesRecv = 0;
    qint64 m_bytesTotal = 0;
    bool m_hidden = false;
    QString m_errorMsg;
};

} // namespace ShadowLauncher
