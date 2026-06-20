import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: shaderTab
    anchors.fill: parent
    property var backend
    property var mainWindow: null
    signal triggerDownloadBall(real sourceX, real sourceY)
    property bool shaderResultsReady: false
    property bool shaderSearching: false
    property var shaderDownloadingSlugs: []


        ColumnLayout {
            spacing: 8

            // ── Header ──
            Text {
                Layout.fillWidth: true
                text: "热门光影推荐"
                color: "#d0d4e0"; font.pixelSize: 14; font.bold: true
            }

            // ── Search bar ──
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Rectangle {
                    Layout.fillWidth: true; height: 30; radius: 5
                    color: "#0c0e14"
                    border.color: shaderSearchInput.activeFocus ? "#5068c8" : "#2a3040"
                    border.width: 1

                    TextInput {
                        id: shaderSearchInput
                        color: "#d0d4e0"; verticalAlignment: TextInput.AlignVCenter; font.pixelSize: 12

                        Text {
                            text: "搜索光影..."; color: "#505468"; font.pixelSize: 11
                            visible: !shaderSearchInput.text
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 72; height: 30; radius: 5
                    color: shaderSearchBtn.hovered ? "#5a78e0" : "#5068c8"

                    Text {
                        text: "搜索"
                        color: "#ffffff"; font.pixelSize: 12; font.bold: true
                    }
                    MouseArea {
                        id: shaderSearchBtn; anchors.fill: parent
                        hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var q = shaderSearchInput.text.trim()
                            if (!q) return
                            console.log("[shader-ui] search query=\"" + q + "\"")
                            root.shaderSearching = true
                            if (backend.modManager && backend.modManager.searchMods)
                                backend.modManager.searchMods(q, "")
                        }
                    }
                }
            }

            // ── Results area ──
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Flickable {
                    id: shaderFlick
                    contentHeight: shaderGrid.implicitHeight
                    clip: true
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    ColumnLayout {
                        id: shaderGrid
                        width: parent.width
                        spacing: 6

                        // Loading state
                        Text {
                            visible: root.shaderSearching
                            Layout.fillWidth: true
                            text: "搜索中…"
                            color: "#606478"; font.pixelSize: 12
                            horizontalAlignment: Qt.AlignHCenter
                            Layout.topMargin: 20
                        }

                        // Empty state
                        Text {
                            visible: !root.shaderSearching && shaderResultsModel.count === 0
                            Layout.fillWidth: true
                            text: "未找到匹配的光影"
                            color: "#606478"; font.pixelSize: 12
                            horizontalAlignment: Qt.AlignHCenter
                            Layout.topMargin: 20
                        }

                        // Results cards
                        Repeater {
                            model: shaderResultsModel

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 56
                                radius: 8
                                color: "#161922"
                                border.color: "#1e2230"

                                RowLayout {
                                    spacing: 10

                                    // Icon
                                    Rectangle {
                                        Layout.preferredWidth: 32; Layout.preferredHeight: 32; radius: 6
                                        color: "#0c0e14"
                                        Text {
                                            text: "🌄"
                                            font.pixelSize: 16
                                        }
                                    }

                                    // Info
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        spacing: 2
                                        Layout.alignment: Qt.AlignVCenter

                                        Text {
                                            text: model.title || "Unknown"
                                            color: "#d0d4e0"; font.pixelSize: 13; font.bold: true
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: model.desc || ""
                                            color: "#606478"; font.pixelSize: 11
                                            elide: Text.ElideRight
                                            maximumLineCount: 1
                                        }
                                        Text {
                                            text: "下载: " + (model.downloads || 0)
                                            color: "#5068c8"; font.pixelSize: 10
                                        }
                                    }

                                    // Download button
                                    Rectangle {
                                        Layout.preferredWidth: 60; height: 26; radius: 5
                                        color: {
                                            if (root.shaderDownloadingSlugs[model.slug])
                                                return "#2a3040"
                                            if (shaderDlBtn.containsMouse)
                                                return "#5a78e0"
                                            return "#5068c8"
                                        }

                                        Text {
                                            text: root.shaderDownloadingSlugs[model.slug] ? "下载中…" : "下载"
                                            color: root.shaderDownloadingSlugs[model.slug] ? "#606478" : "#ffffff"
                                            font.pixelSize: 11
                                        }
                                        MouseArea {
                                            id: shaderDlBtn
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (root.shaderDownloadingSlugs[model.slug]) return
                                                console.log("[shader-ui] download slug=\"" + model.slug + "\"")
                                                root.shaderDownloadingSlugs[model.slug] = true
                                                root.shaderDownloadingSlugs = Object.assign({}, root.shaderDownloadingSlugs)
                                                backend.downloadShader(model.slug)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
}
