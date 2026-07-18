// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick3D

// 3D Minecraft Head Avatar
// Displays a low-poly Steve/Alex head with the player's skin texture
View3D {
    id: root
    property url skinSource: ""
    property real headRotation: 25  // slight angle for a 3/4 view

    // Transparent background
    environment: SceneEnvironment {
        clearColor: "transparent"
        backgroundMode: SceneEnvironment.Transparent
    }

    // ── Head Model ──
    Model {
        id: headModel
        source: "models/steve_head.obj"

        materials: DefaultMaterial {
            id: skinMaterial
            diffuseMap: Texture {
                id: skinTexture
                source: root.skinSource
                minFilter: Texture.Nearest  // pixel-art sharpness
                magFilter: Texture.Nearest
                mipFilter: Texture.None
            }
        }

        // Start at a nice 3/4 view angle
        eulerRotation.x: -15
        eulerRotation.y: root.headRotation
    }

    // ── Camera ──
    PerspectiveCamera {
        id: cam
        position: Qt.vector3d(0, 0, 18)
        clipNear: 0.1
        clipFar: 100
    }

    // ── Lighting ──
    DirectionalLight {
        eulerRotation.x: -30
        eulerRotation.y: -45
        brightness: 1.2
    }
    DirectionalLight {
        eulerRotation.x: 30
        eulerRotation.y: 45
        brightness: 0.4
    }
}
