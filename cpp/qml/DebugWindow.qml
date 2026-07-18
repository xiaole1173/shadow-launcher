import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: root
    title: "Shadow Launcher 调试"
    width: 420; height: 520
    minimumWidth: 320; minimumHeight: 300
    color: StyleTokens.bgPrimary
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowMinimizeButtonHint | Qt.WindowStaysOnTopHint

    property alias debugLog: logPanel
    property var logLines: []
    property int maxLines: 3000

    function toggle() {
        if (root.visible) root.hide(); else root.show()
    }

    function _append(color, tag, msg) {
        var ts = new Date().toTimeString().split(' ')[0] + '.' + String(Date.now() % 1000).padStart(3, '0')
        var line = '<span style="color:#666">[' + ts + ']</span> '
                 + '<span style="color:' + color + '">[' + tag + ']</span> ' + msg + '<br>'
        logLines.push(line)
        while (logLines.length > maxLines) logLines.shift()
        entryCount = logLines.length
    }

    function info(msg)  { _append("#aaa", "信息", msg) }
    function ok(msg)    { _append("#4caf50", "完成", msg) }
    function warn(msg)  { _append("#ff9800", "警告", msg) }
    function error(msg) { _append("#f44336", "错误", msg) }
    function net(url, status, sizebytes, timems) {
        var s = sizebytes < 1024 ? sizebytes + "B" :
                sizebytes < 1048576 ? (sizebytes/1024).toFixed(1) + "KB" :
                (sizebytes/1048576).toFixed(1) + "MB"
        var spd = timems > 0 ? (sizebytes*1000/timems/1024).toFixed(0) + "KB/s" : "\u2014"
        if (status === "OK")
            _append("#4caf50", "网络", "\u2705 " + s + "  " + spd + "  " + timems + "ms  " + url)
        else
            _append("#f44336", "网络", "\u274c " + timems + "ms  " + status + "  " + url)
    }

    property int entryCount: 0

    ColumnLayout {
        id: logPanel
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        // Header bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: StyleTokens.bgPrimary
            border.color: StyleTokens.surfaceLight

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 6
                spacing: 8

                Text {
                    text: "\ud83d\udd0d \u8c03\u8bd5\u65e5\u5fd7"
                    color: "#aaa"
                    font.pixelSize: StyleTokens.fontSizeSm
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: root.entryCount + "\u6761"
                    color: StyleTokens.textMuted
                    font.pixelSize: StyleTokens.fontSizeXs
                }
                Text {
                    text: "\u6e05\u7a7a"
                    color: StyleTokens.error
                    font.pixelSize: StyleTokens.fontSizeXs
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: { root.logLines = []; root.entryCount = 0 }
                    }
                }
                Item { width: 6 }
                Text {
                    text: "\u2715"
                    color: "#666"
                    font.pixelSize: StyleTokens.fontSizeMd
                    font.weight: Font.Bold
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.hide()
                    }
                }
            }
        }

        // Log content
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn

            TextEdit {
                id: logView
                width: parent.availableWidth
                readOnly: true
                textFormat: TextEdit.RichText
                wrapMode: TextEdit.WrapAnywhere
                color: "#ccc"
                font.family: "Consolas, 'Microsoft YaHei', monospace"
                font.pixelSize: StyleTokens.fontSizeSm
                selectByMouse: true
                padding: 8
                text: root.logLines.join('')
                onTextChanged: {
                    Qt.callLater(function() {
                        var bar = parent.parent.ScrollBar.vertical
                        bar.position = 1.0 - bar.size
                    })
                }
            }
        }

        // Footer bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            color: StyleTokens.bgPrimary
            border.color: StyleTokens.surfaceLight

            Row {
                anchors.centerIn: parent
                spacing: 16
                Text { text: "\u2705\u7f51\u7edc"; color: "#4caf50"; font.pixelSize: StyleTokens.fontSizeXs }
                Text { text: "\u26a0\u8b66\u544a"; color: "#ff9800"; font.pixelSize: StyleTokens.fontSizeXs }
                Text { text: "\u274c\u9519\u8bef"; color: "#f44336"; font.pixelSize: StyleTokens.fontSizeXs }
            }
        }
    }

    // Timer to flush logText to view
    Timer {
        interval: 200
        running: true
        repeat: true
        onTriggered: {
            var text = logLines.join('')
            if (logView.text !== text) logView.text = text
        }
    }
}
