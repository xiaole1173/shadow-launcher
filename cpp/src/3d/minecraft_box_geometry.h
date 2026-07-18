// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QQuick3DGeometry>
#include <QVector3D>
#include <QVector2D>

// ═══ Minecraft 方块几何体 — skinview3d UV 映射 ═══
// 生成带正确 UV 坐标的 Box 几何体，UV 布局匹配 setSkinUVs/setCapeUVs
// 注册为 QML 类型，可在 Model.geometry 中直接使用
class MinecraftBoxGeometry : public QQuick3DGeometry {
    Q_OBJECT
    Q_PROPERTY(float w READ w WRITE setW NOTIFY geometryChanged)
    Q_PROPERTY(float h READ h WRITE setH NOTIFY geometryChanged)
    Q_PROPERTY(float d READ d WRITE setD NOTIFY geometryChanged)
    Q_PROPERTY(int uvU READ uvU WRITE setUvU NOTIFY geometryChanged)   // UV 起始 X (像素)
    Q_PROPERTY(int uvV READ uvV WRITE setUvV NOTIFY geometryChanged)   // UV 起始 Y (像素)
    Q_PROPERTY(int uvW READ uvW WRITE setUvW NOTIFY geometryChanged)   // UV width  (像素)
    Q_PROPERTY(int uvH READ uvH WRITE setUvH NOTIFY geometryChanged)   // UV height (像素)
    Q_PROPERTY(int uvD READ uvD WRITE setUvD NOTIFY geometryChanged)   // UV depth  (像素)
    Q_PROPERTY(int texW READ texW WRITE setTexW NOTIFY geometryChanged) // 纹理总宽
    Q_PROPERTY(int texH READ texH WRITE setTexH NOTIFY geometryChanged) // 纹理总高
    QML_ELEMENT
    Q_CLASSINFO("ParentProperty", "geometry")

public:
    explicit MinecraftBoxGeometry(QQuick3DObject *parent = nullptr);

    float w() const { return m_w; }
    void setW(float val);

    float h() const { return m_h; }
    void setH(float val);

    float d() const { return m_d; }
    void setD(float val);

    int uvU() const { return m_uvU; }
    void setUvU(int val);

    int uvV() const { return m_uvV; }
    void setUvV(int val);

    int uvW() const { return m_uvW; }
    void setUvW(int val);

    int uvH() const { return m_uvH; }
    void setUvH(int val);

    int uvD() const { return m_uvD; }
    void setUvD(int val);

    int texW() const { return m_texW; }
    void setTexW(int val);

    int texH() const { return m_texH; }
    void setTexH(int val);

signals:
    void geometryChanged();

private:
    void buildGeometry();

    float m_w = 8, m_h = 8, m_d = 8;
    int m_uvU = 0, m_uvV = 0;
    int m_uvW = 8, m_uvH = 8, m_uvD = 8;
    int m_texW = 64, m_texH = 64;
};
