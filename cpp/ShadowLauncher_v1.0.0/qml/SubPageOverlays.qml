// ============================================================
//  SUB-PAGE OVERLAY
// ============================================================
import QtQuick

Item {
    id: root

    // --- VersionSelectPage ---
    Loader {
        id: versionSelectLoader
        anchors.fill: parent
        z: 5
        source: showVersionSelect ? "VersionSelectPage.qml" : ""
        visible: showVersionSelect
        onLoaded: {
            versionSelectLoader.item.backend = backend
            versionSelectLoader.item.toastManager = toastManager
            versionSelectLoader.item.goBack.connect(function() { showVersionSelect = false })
            versionSelectLoader.item.versionSelected.connect(function(vid) {
                showVersionSelect = false
            })
        }
    }

    // --- VersionSettingsPage ---
    Loader {
        id: versionSettingsLoader
        anchors.fill: parent
        z: 5
        source: showVersionSettings ? "VersionSettingsPage.qml" : ""
        visible: showVersionSettings
        onLoaded: {
            versionSettingsLoader.item.backend = backend
            versionSettingsLoader.item.goBack.connect(function() { showVersionSettings = false })
        }
    }

    // --- SettingsGeneralPage ---
    Loader {
        id: settingsGeneralLoader
        anchors.fill: parent
        z: 5
        source: showGeneralSettings ? "SettingsGeneralPage.qml" : ""
        visible: showGeneralSettings
        onLoaded: {
            settingsGeneralLoader.item.backend = backend
            settingsGeneralLoader.item.goBack.connect(function() { showGeneralSettings = false })
        }
    }

    // --- SettingsJavaPage ---
    Loader {
        id: settingsJavaLoader
        anchors.fill: parent
        z: 5
        source: showJavaSettings ? "SettingsJavaPage.qml" : ""
        visible: showJavaSettings
        onLoaded: {
            settingsJavaLoader.item.backend = backend
            settingsJavaLoader.item.goBack.connect(function() { showJavaSettings = false })
        }
    }

    // --- SettingsMemoryPage ---
    Loader {
        id: settingsMemoryLoader
        anchors.fill: parent
        z: 5
        source: showMemorySettings ? "SettingsMemoryPage.qml" : ""
        visible: showMemorySettings
        onLoaded: {
            settingsMemoryLoader.item.backend = backend
            settingsMemoryLoader.item.goBack.connect(function() { showMemorySettings = false })
        }
    }
}
