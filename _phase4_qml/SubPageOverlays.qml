// ═══════════════════════════════════════════════════════════
//  SUB-PAGE OVERLAY — 在 pageContainer 外面，覆盖整个右侧区域
// ═══════════════════════════════════════════════════════════

// ── VersionSelectPage ─────────────────────
Loader {
    id: versionSelectLoader
    anchors.fill: pageContainer
    z: 5
    source: showVersionSelect ? "VersionSelectPage.qml" : ""
    visible: showVersionSelect
    onLoaded: {
        versionSelectLoader.item.backend = backend
        versionSelectLoader.item.toastManager = toastManager
        versionSelectLoader.item.goBack.connect(function() { showVersionSelect = false })
        versionSelectLoader.item.versionSelected.connect(function(vid) {
            currentSelectedVersion = vid
            showVersionSelect = false
            currentStackIndex = 0
        })
    }
    // Opaque background to prevent page underneath showing through
    Rectangle { anchors.fill: parent; color: "#121418"; z: -1 }
}

// ── VersionSettingsPage ────────────────────
Loader {
    id: versionSettingsLoader
    anchors.fill: pageContainer
    z: 5
    source: showVersionSettings ? "VersionSettingsPage.qml" : ""
    visible: showVersionSettings
    onLoaded: {
        versionSettingsLoader.item.backend = backend
        versionSettingsLoader.item.goBack.connect(function() { showVersionSettings = false })
    }
    Rectangle { anchors.fill: parent; color: "#121418"; z: -1 }
}

// ── SettingsGeneralPage ────────────────────
Loader {
    id: settingsGeneralLoader
    anchors.fill: pageContainer
    z: 5
    source: showGeneralSettings ? "SettingsGeneralPage.qml" : ""
    visible: showGeneralSettings
    onLoaded: {
        settingsGeneralLoader.item.backend = backend
        settingsGeneralLoader.item.goBack.connect(function() { showGeneralSettings = false })
    }
    Rectangle { anchors.fill: parent; color: "#121418"; z: -1 }
}

// ── SettingsJavaPage ───────────────────────
Loader {
    id: settingsJavaLoader
    anchors.fill: pageContainer
    z: 5
    source: showJavaSettings ? "SettingsJavaPage.qml" : ""
    visible: showJavaSettings
    onLoaded: {
        settingsJavaLoader.item.backend = backend
        settingsJavaLoader.item.goBack.connect(function() { showJavaSettings = false })
    }
    Rectangle { anchors.fill: parent; color: "#121418"; z: -1 }
}

// ── SettingsMemoryPage ─────────────────────
Loader {
    id: settingsMemoryLoader
    anchors.fill: pageContainer
    z: 5
    source: showMemorySettings ? "SettingsMemoryPage.qml" : ""
    visible: showMemorySettings
    onLoaded: {
        settingsMemoryLoader.item.backend = backend
        settingsMemoryLoader.item.goBack.connect(function() { showMemorySettings = false })
    }
    Rectangle { anchors.fill: parent; color: "#121418"; z: -1 }
}
