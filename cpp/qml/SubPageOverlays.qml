// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// ============================================================
//  SUB-PAGE OVERLAY
// ============================================================
import QtQuick

Item {
    id: root

    // --- VersionSelectPage ---
    Item {
        id: versionSelectContainer
        anchors.fill: parent
        z: 5
        visible: showVersionSelect || versionSelectFadeOut.running

        Loader {
            id: versionSelectLoader
            anchors.fill: parent
            source: showVersionSelect ? "VersionSelectPage.qml" : ""
            onLoaded: {
                versionSelectLoader.item.backend = backend
                versionSelectLoader.item.toastManager = toastManager
                versionSelectLoader.item.mainWindow = appWindow
                versionSelectLoader.item.goBack.connect(function() { showVersionSelect = false })
                versionSelectLoader.item.versionSelected.connect(function(vid) {
                    showVersionSelect = false
                })
            }
        }

        opacity: showVersionSelect ? 1 : 0
        Behavior on opacity { NumberAnimation { id: versionSelectFadeOut; duration: 200; easing.type: Easing.OutCubic } }
        scale: showVersionSelect ? 1 : 0.97
        Behavior on scale { NumberAnimation { duration: 250; easing.type: Easing.OutBack; easing.overshoot: 0.1 } }
    }

    // --- VersionSettingsPage ---
    Item {
        id: versionSettingsContainer
        anchors.fill: parent
        z: 5
        visible: showVersionSettings || versionSettingsFadeOut.running

        Loader {
            id: versionSettingsLoader
            anchors.fill: parent
            source: showVersionSettings ? "VersionSettingsPage.qml" : ""
            onLoaded: {
                versionSettingsLoader.item.backend = backend
                versionSettingsLoader.item.currentVersionId = currentSelectedVersion
                versionSettingsLoader.item.goBack.connect(function() { showVersionSettings = false })
            }
        }

        opacity: showVersionSettings ? 1 : 0
        Behavior on opacity { NumberAnimation { id: versionSettingsFadeOut; duration: 200; easing.type: Easing.OutCubic } }
        scale: showVersionSettings ? 1 : 0.97
        Behavior on scale { NumberAnimation { duration: 250; easing.type: Easing.OutBack; easing.overshoot: 0.1 } }
    }

    // --- SettingsGeneralPage ---
    Item {
        id: settingsGeneralContainer
        anchors.fill: parent
        z: 5
        visible: showGeneralSettings || settingsGeneralFadeOut.running

        Loader {
            id: settingsGeneralLoader
            anchors.fill: parent
            source: showGeneralSettings ? "SettingsGeneralPage.qml" : ""
            onLoaded: {
                settingsGeneralLoader.item.backend = backend
                settingsGeneralLoader.item.goBack.connect(function() { showGeneralSettings = false })
            }
        }

        opacity: showGeneralSettings ? 1 : 0
        Behavior on opacity { NumberAnimation { id: settingsGeneralFadeOut; duration: 200; easing.type: Easing.OutCubic } }
        scale: showGeneralSettings ? 1 : 0.97
        Behavior on scale { NumberAnimation { duration: 250; easing.type: Easing.OutBack; easing.overshoot: 0.1 } }
    }

    // --- SettingsJavaPage ---
    Item {
        id: settingsJavaContainer
        anchors.fill: parent
        z: 5
        visible: showJavaSettings || settingsJavaFadeOut.running

        Loader {
            id: settingsJavaLoader
            anchors.fill: parent
            source: showJavaSettings ? "SettingsJavaPage.qml" : ""
            onLoaded: {
                settingsJavaLoader.item.backend = backend
                settingsJavaLoader.item.toastManager = toastManager
                settingsJavaLoader.item.goBack.connect(function() { showJavaSettings = false })
            }
        }

        opacity: showJavaSettings ? 1 : 0
        Behavior on opacity { NumberAnimation { id: settingsJavaFadeOut; duration: 200; easing.type: Easing.OutCubic } }
        scale: showJavaSettings ? 1 : 0.97
        Behavior on scale { NumberAnimation { duration: 250; easing.type: Easing.OutBack; easing.overshoot: 0.1 } }
    }

    // --- SettingsMemoryPage ---
    Item {
        id: settingsMemoryContainer
        anchors.fill: parent
        z: 5
        visible: showMemorySettings || settingsMemoryFadeOut.running

        Loader {
            id: settingsMemoryLoader
            anchors.fill: parent
            source: showMemorySettings ? "SettingsMemoryPage.qml" : ""
            onLoaded: {
                settingsMemoryLoader.item.backend = backend
                if (settingsMemoryLoader.item.refreshAll) settingsMemoryLoader.item.refreshAll()
                settingsMemoryLoader.item.goBack.connect(function() { showMemorySettings = false })
            }
        }

        opacity: showMemorySettings ? 1 : 0
        Behavior on opacity { NumberAnimation { id: settingsMemoryFadeOut; duration: 200; easing.type: Easing.OutCubic } }
        scale: showMemorySettings ? 1 : 0.97
        Behavior on scale { NumberAnimation { duration: 250; easing.type: Easing.OutBack; easing.overshoot: 0.1 } }
    }
}
