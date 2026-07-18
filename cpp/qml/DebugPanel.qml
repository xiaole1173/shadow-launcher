import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: StyleTokens.bgPrimary
    border.color: StyleTokens.surfaceOverlay
    border.width: 1
    visible: debugVisible

    property bool debugVisible: false
    property var logLines: []
    property int maxLines: 2000

    function toggle() {
        debugVisible = !debugVisible
    }

    function _append(color, tag, msg) {
        var ts = new Date().toTimeString().split(' ')[0] + '.' + String(Date.now() % 1000).padStart(3, '0')
        var line = '<span style="color:#666">[' + ts + ']</span> '
                 + '<span style="color:' + color + '">[' + tag + ']</span> ' + msg + '<br>'
        logLines.push(line)
        while (logLines.length > maxLines) logLines.shift()
        logText = logLines.join('')
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
        var spd = timems > 0 ? (sizebytes*1000/timems/1024).toFixed(0) + "KB/s" : "—"
        if (status === "OK")
            _append("#4caf50", "网络", "[完成] " + s + "  " + spd + "  " + timems + "ms  " + url)
        else
            _append("#f44336", "网络", "[失败] " + timems + "ms  " + status + "  " + url)
    }

    property string logText: ""
    property int entryCount: 0

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: StyleTokens.bgSecondary
            border.color: StyleTokens.surfaceOverlay
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 8
                spacing: 8

                Text {
                    text: "\ud83d\udd0d \u8c03\u8bd5\u9762\u677f"
                    color: "#aaa"
                    font.pixelSize: StyleTokens.fontSizeSm
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: root.entryCount + "\u6761"
                    color: "#666"
                    font.pixelSize: StyleTokens.fontSizeXs
                }
                Text {
                    text: "\u6e05\u7a7a"
                    color: StyleTokens.error
                    font.pixelSize: StyleTokens.fontSizeXs
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: { root.logLines = []; root.logText = ""; root.entryCount = 0 }
                    }
                }
            }
        }

        ScrollView {
            id: scrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn

            TextEdit {
                id: logView
                width: scrollView.availableWidth
                readOnly: true
                textFormat: TextEdit.RichText
                wrapMode: TextEdit.WrapAnywhere
                color: "#ccc"
                font.family: "Consolas, 'Microsoft YaHei', monospace"
                font.pixelSize: StyleTokens.fontSizeSm
                selectByMouse: true
                padding: 8
                text: root.logText
                onTextChanged: {
                    Qt.callLater(function() {
                        scrollView.ScrollBar.vertical.position = 1.0 - scrollView.ScrollBar.vertical.size
                    })
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            color: StyleTokens.bgSecondary
            border.color: StyleTokens.surfaceOverlay
            Row {
                anchors.centerIn: parent
                spacing: 16
                Text { text: "\u7f51\u7edc"; color: "#4caf50"; font.pixelSize: StyleTokens.fontSizeXs }
                Text { text: "\u8fdb\u5ea6"; color: "#ff9800"; font.pixelSize: StyleTokens.fontSizeXs }
                Text { text: "\u9519\u8bef"; color: "#f44336"; font.pixelSize: StyleTokens.fontSizeXs }
            }
        }
    }
}
