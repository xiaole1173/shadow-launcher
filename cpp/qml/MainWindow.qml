with open('D:\\latest-code\\cpp\\qml\\MainWindow.qml', 'r', encoding='utf-8') as f:
    c = f.read()

# Replace the FAB opacity/scale with proper fade pattern
old = '''        opacity: (backend && backend.installCardsModel && backend.installCardsModel.count > 0) ? 1 : 0
        scale: opacity  // opacity drives scale too via binding
        enabled: opacity > 0.5

        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }'''

new = '''        // ── Fade in/out: opacity drives animation, visible hides render tree ──
        readonly property bool _hasDownloads: backend && backend.installCardsModel && backend.installCardsModel.count > 0
        opacity: _hasDownloads ? 1 : 0
        scale: 0.8 + opacity * 0.2  // scale 0.8→1.0 as opacity goes 0→1
        visible: opacity > 0.01  // keep in render tree during fade-out
        enabled: opacity > 0.5

        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }'''

assert old in c, 'Old FAB text not found!'
c = c.replace(old, new)

# Fix pulse animation to check _hasDownloads instead of parent.visible
c = c.replace('running: parent.visible && !downloadPanel.expanded',
              'running: _hasDownloads && !downloadPanel.expanded')

# Clean up old comment
c = c.replace('// Floating FAB + side panel replaced the old full-page DownloadProgressPage',
              '// Floating FAB + side panel')

with open('D:\\latest-code\\cpp\\qml\\MainWindow.qml', 'w', encoding='utf-8') as f:
    f.write(c)
print('Done')
