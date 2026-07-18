// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// UV layout: skinview3d-compatible — direct vanilla Minecraft skin mapping
// Each face builds 2 triangles with explicit position + normal + UV
#include "minecraft_box_geometry.h"

#include <QByteArray>

MinecraftBoxGeometry::MinecraftBoxGeometry(QQuick3DObject *parent)
    : QQuick3DGeometry(parent) { buildGeometry(); }

#define SET(f, field) \
    void MinecraftBoxGeometry::set##f(float v) { \
        if (qFuzzyCompare(m_##field, v)) return; \
        m_##field = v; buildGeometry(); emit geometryChanged(); }
#define SETI(f, field) \
    void MinecraftBoxGeometry::set##f(int v) { \
        if (m_##field == v) return; \
        m_##field = v; buildGeometry(); emit geometryChanged(); }

SET(W,w) SET(H,h) SET(D,d)
SETI(UvU,uvU) SETI(UvV,uvV) SETI(UvW,uvW) SETI(UvH,uvH) SETI(UvD,uvD) SETI(TexW,texW) SETI(TexH,texH)
#undef SET
#undef SETI

struct Vertex { float x,y,z, nx,ny,nz, u,v; };
static_assert(sizeof(Vertex) == 32);

// Write one triangle (3 vertices) to buffer
static inline void tri(Vertex*& vp,
    float x1,float y1,float z1, float x2,float y2,float z2, float x3,float y3,float z3,
    float nx,float ny,float nz,
    float u1,float v1, float u2,float v2, float u3,float v3)
{
    *vp++ = {x1,y1,z1, nx,ny,nz, u1,v1};
    *vp++ = {x2,y2,z2, nx,ny,nz, u2,v2};
    *vp++ = {x3,y3,z3, nx,ny,nz, u3,v3};
}

void MinecraftBoxGeometry::buildGeometry()
{
    clear();

    const float hw = m_w * 0.5f, hh = m_h * 0.5f, hd = m_d * 0.5f;
    const float invW = 1.0f / float(m_texW);
    const float invH = 1.0f / float(m_texH);

    // Pixel-space UV region (vanilla MC layout)
    // Y-flip: OpenGL UV has v=0 at bottom, image has y=0 at top
    int u  = m_uvU, v  = m_uvV;
    int w  = m_uvW, h  = m_uvH, d = m_uvD;

    // Front  (+Z): pixels (u+d, v+d) → (u+d+w, v+d+h)
    float f_l = (u + d)     * invW,  f_r = (u + d + w) * invW;
    float f_b = 1.0f - (v + d + h) * invH, f_t = 1.0f - (v + d) * invH;
    // Right  (+X): pixels (u+w+d, v+d) → (u+w+2d, v+d+h)
    float r_l = (u + w + d) * invW,     r_r = (u + w + 2*d) * invW;
    float r_b = 1.0f - (v + d + h) * invH, r_t = 1.0f - (v + d) * invH;
    // Back   (-Z): pixels (u+w+2d, v+d) → (u+w*2+2d, v+d+h)
    float b_l = (u + w + 2*d) * invW,   b_r = (u + w*2 + 2*d) * invW;
    float b_b = 1.0f - (v + d + h) * invH, b_t = 1.0f - (v + d) * invH;
    // Left   (-X): pixels (u, v+d) → (u+d, v+d+h)
    float l_l = u * invW,               l_r = (u + d) * invW;
    float l_b = 1.0f - (v + d + h) * invH, l_t = 1.0f - (v + d) * invH;
    // Top    (+Y): pixels (u+d, v) → (u+d+w, v+d)
    float t_l = (u + d) * invW,         t_r = (u + d + w) * invW;
    float t_b = 1.0f - (v + d) * invH,  t_t = 1.0f - v * invH;
    // Bottom (-Y): pixels (u+d+w, v) → (u+d+w*2, v+d)
    float bo_l = (u + d + w) * invW,    bo_r = (u + d + w*2) * invW;
    float bo_b = 1.0f - (v + d) * invH, bo_t = 1.0f - v * invH;

    // 36 vertices = 6 faces × 2 triangles × 3 vertices
    QByteArray vd; vd.resize(sizeof(Vertex) * 36);
    auto *vp = reinterpret_cast<Vertex*>(vd.data());

    // ═══ TOP face (+Y) ═══
    // Three.js vertices: v0=P6(-,-), v1=P7(+,-), v2=P3(-,+), v3=P2(+,+)
    // UV order: [top[3]=bott-left, top[2]=bott-right, top[0]=top-left, top[1]=top-right]
    tri(vp, -hw,hh,-hd, +hw,hh,-hd, -hw,hh,+hd, 0,1,0,  t_l,t_t, t_r,t_t, t_l,t_b);
    tri(vp, -hw,hh,-hd, -hw,hh,+hd, +hw,hh,+hd, 0,1,0,  t_l,t_t, t_l,t_b, t_r,t_b);

    // ═══ BOTTOM face (-Y) ═══
    // Three.js vertices: v0=P4(-,-), v1=P5(+,-), v2=P0(-,+), v3=P1(+,+)
    // UV order is special: [bot[0]=top-left, bot[1]=top-right, bot[3]=bot-left, bot[2]=bot-right]
    tri(vp, -hw,-hh,-hd, +hw,-hh,-hd, -hw,-hh,+hd, 0,-1,0,  bo_l,bo_t, bo_r,bo_t, bo_l,bo_b);
    tri(vp, -hw,-hh,-hd, -hw,-hh,+hd, +hw,-hh,+hd, 0,-1,0,  bo_l,bo_t, bo_l,bo_b, bo_r,bo_b);

    // ═══ FRONT face (+Z): looking from +Z ═══
    // P0(-,-,+) left-bottom, P1(+,-,+) right-bottom, P2(+,+,+) right-top, P3(-,+,+) left-top
    tri(vp, -hw,-hh,+hd, +hw,-hh,+hd, +hw,+hh,+hd, 0,0,1,  f_l,f_b, f_r,f_b, f_r,f_t);
    tri(vp, -hw,-hh,+hd, +hw,+hh,+hd, -hw,+hh,+hd, 0,0,1,  f_l,f_b, f_r,f_t, f_l,f_t);

    // ═══ BACK face (-Z): looking from -Z ═══
    // Three.js vertices: v0=P5(+,-), v1=P4(-,-), v2=P7(+,+), v3=P6(-,+)
    // UV order: [back[3]=bot-left, back[2]=bot-right, back[0]=top-left, back[1]=top-right]
    tri(vp, +hw,-hh,-hd, -hw,-hh,-hd, +hw,+hh,-hd, 0,0,-1,  b_l,b_t, b_r,b_t, b_l,b_b);
    tri(vp, +hw,-hh,-hd, +hw,+hh,-hd, -hw,+hh,-hd, 0,0,-1,  b_l,b_t, b_l,b_b, b_r,b_b);

    // ═══ LEFT face (-X): looking from -X ═══
    // P4(-,-,-) left-bottom, P0(-,-,+) right-bottom, P3(-,+,+) right-top, P6(-,+,-) left-top
    tri(vp, -hw,-hh,-hd, -hw,-hh,+hd, -hw,+hh,+hd, -1,0,0,  l_l,l_b, l_r,l_b, l_r,l_t);
    tri(vp, -hw,-hh,-hd, -hw,+hh,+hd, -hw,+hh,-hd, -1,0,0,  l_l,l_b, l_r,l_t, l_l,l_t);

    // ═══ RIGHT face (+X): looking from +X ═══
    // P1(+,-,+) left-bottom, P5(+,-,-) right-bottom, P7(+,+,-) right-top, P2(+,+,+) left-top
    tri(vp, +hw,-hh,+hd, +hw,-hh,-hd, +hw,+hh,-hd, 1,0,0,  r_l,r_b, r_r,r_b, r_r,r_t);
    tri(vp, +hw,-hh,+hd, +hw,+hh,-hd, +hw,+hh,+hd, 1,0,0,  r_l,r_b, r_r,r_t, r_l,r_t);

    QByteArray idx; idx.resize(36 * sizeof(quint32));
    auto *ix = reinterpret_cast<quint32*>(idx.data());
    for (int i = 0; i < 36; ++i) ix[i] = i;

    setStride(sizeof(Vertex));
    setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
    setVertexData(vd);
    setIndexData(idx);
    addAttribute(QQuick3DGeometry::Attribute::PositionSemantic, 0,
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::NormalSemantic,
                 offsetof(Vertex, nx), QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::TexCoordSemantic,
                 offsetof(Vertex, u), QQuick3DGeometry::Attribute::F32Type);
    setBounds({-hw,-hh,-hd}, {hw,hh,hd});
}
