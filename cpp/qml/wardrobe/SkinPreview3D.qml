// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick3D
import ShadowLauncher 1.0

// ═══ Minecraft 玩家 3D 预览 (skinview3d 精确 UV 映射) ═══
// 每个部件使用 MinecraftBoxGeometry 生成带正确 UV 坐标的方块几何体
// UV 参数与 skinview3d/src/model.ts 完全一致
View3D {
    id: skinView
    anchors.fill: parent

    property url skinSource: ""
    property url capeSource: ""

    environment: SceneEnvironment {
        clearColor: "#1a2030"
        backgroundMode: SceneEnvironment.Color
    }

    PerspectiveCamera {
        id: camera
        position: Qt.vector3d(0, 0, 40)
        clipNear: 1; clipFar: 100
    }

    DirectionalLight {
        eulerRotation.x: -30; eulerRotation.y: 30
        brightness: 1.5
    }
    DirectionalLight {
        eulerRotation.x: -30; eulerRotation.y: -150
        brightness: 0.8
    }

    Texture {
        id: skinTex
        source: skinView.skinSource
        minFilter: Texture.Nearest; magFilter: Texture.Nearest
    }
    Texture {
        id: capeTex
        source: skinView.capeSource
        minFilter: Texture.Nearest; magFilter: Texture.Nearest
    }

    readonly property bool hasSkin: skinTex.source.toString() !== ""

    // ═══ 皮肤材质 (复用) ═══
    property Material skinMat: DefaultMaterial {
        diffuseMap: hasSkin ? skinTex : null
        diffuseColor: hasSkin ? "#ffffff" : "#e0b080"
    }

    // ═══ 玩家根节点 ═══
    Node {
        id: playerRoot
        y: 8

        // glTF 模型
        Model {
            source: "alex.glb"
            materials: skinMat
        }

        // ── 披风 10×16×1 ──
        Node {
            y: 8; z: -2
            eulerRotation.x: 10.8
            visible: skinView.capeSource.toString() !== ""
            Model {
                y: -8; z: 0.5
                eulerRotation.y: 180
                geometry: MinecraftBoxGeometry { w: 10; h: 16; d: 1; uvU: 0; uvV: 0; uvW: 10; uvH: 16; uvD: 1; texW: 64; texH: 32 }
                materials: DefaultMaterial {
                    diffuseMap: capeTex
                    diffuseColor: "#ffffff"
                    opacity: 0.95
                }
            }
        }
    }
}
