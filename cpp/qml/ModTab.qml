import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: modTab
    anchors.fill: parent
    property var backend
    property var mainWindow: null
    signal triggerDownloadBall(real sourceX, real sourceY)
    property string modSearchQuery: ""
    property string modLoader: "fabric"
    property var modSearchResults: []
    property bool modResultsReady: false
    property bool installingMod: false
    property string installingModName: ""
    property string complianceNotice: ""

    property bool modSearching: false
    property var modDownloadingSlugs: []



        ColumnLayout {

            spacing: 8



            // ── Header row ──

            RowLayout {

                Layout.fillWidth: true

                spacing: 8



                Text {

                    text: "热门 Mod 推荐"

                    color: "#d0d4e0"; font.pixelSize: 14; font.bold: true

                    Layout.fillWidth: true

                }



                // Loader selector

                Rectangle {

                    id: modLoaderPill

                    Layout.preferredWidth: 100; height: 28; radius: 5

                    color: modLdrHov.hovered ? "#1a2848" : "#0c0e14"

                    border.color: "#2a3040"; border.width: 1



                    RowLayout {

                        Text {

                            Layout.fillWidth: true

                            text: root.modLoader === "fabric" ? "Fabric" : "Forge"

                            color: "#5068c8"; font.pixelSize: 11; font.bold: true

                        }

                        Text { text: "▾"; color: "#505468"; font.pixelSize: 8 }

                    }

                    MouseArea {

                        id: modLdrHov; anchors.fill: parent

                        hoverEnabled: true; cursorShape: Qt.PointingHandCursor

                        onClicked: { if (modLoaderMenu.visible) modLoaderMenu.close(); else modLoaderMenu.open() }

                    }



                    Popup {

                        id: modLoaderMenu; closePolicy: Popup.NoAutoClose

                        y: parent.height + 4; width: 100

                        padding: 4

                        background: Rectangle { color: "#151922"; radius: 8; border.color: "#1e2230" }

                        ColumnLayout {

                            width: parent.width - 8; spacing: 2

                            Repeater {

                                model: ["fabric", "forge"]

                                Rectangle {

                                    Layout.fillWidth: true; height: 26; radius: 4

                                    color: modelData === root.modLoader ? "#1a2848" : "transparent"

                                    Text {

                                        text: modelData === "fabric" ? "Fabric" : "Forge"

                                        color: modelData === root.modLoader ? "#5068c8" : "#9094a8"; font.pixelSize: 11

                                        font.weight: modelData === root.modLoader ? Font.Bold : Font.Normal

                                    }

                                    MouseArea {

                                        onClicked: {

                                            root.modLoader = modelData

                                            modLoaderMenu.close()

                                            modResultsModel.clear()

                                            root.modResultsReady = false

                                            loadModResults()

                                            root.modResultsReady = true

                                            console.log("[mod-ui] loader switched to " + modelData)

                                        }

                                    }

                                }

                            }

                        }

                    }

                }

            }



            // ── Search bar ──

            RowLayout {

                Layout.fillWidth: true

                spacing: 6



                Rectangle {

                    Layout.fillWidth: true; height: 30; radius: 5

                    color: "#0c0e14"

                    border.color: modSearchInput.activeFocus ? "#5068c8" : "#2a3040"

                    border.width: 1



                    TextInput {

                        id: modSearchInput

                        color: "#d0d4e0"; verticalAlignment: TextInput.AlignVCenter; font.pixelSize: 12



                        Text {

                            text: "搜索 Mod..."; color: "#505468"; font.pixelSize: 11

                            visible: !modSearchInput.text

                        }

                    }

                }



                Rectangle {

                    Layout.preferredWidth: 72; height: 30; radius: 5

                    color: modSearchBtn.hovered ? "#5a78e0" : "#5068c8"



                    Text {

                        text: "搜索"

                        color: "#ffffff"; font.pixelSize: 12; font.bold: true

                    }

                    MouseArea {

                        id: modSearchBtn; anchors.fill: parent

                        hoverEnabled: true; cursorShape: Qt.PointingHandCursor

                        onClicked: {

                            var q = modSearchInput.text.trim()

                            if (!q) return

                            console.log("[mod-ui] search query=\"" + q + "\" loader=" + root.modLoader)

                            root.modSearching = true

                            backend.searchMods(q, root.modLoader)

                        }

                    }

                }

            }



            // ── Scrollable results area ──

            Item {

                Layout.fillWidth: true

                Layout.fillHeight: true



                Flickable {

                    id: modFlick

                    contentHeight: modGrid.implicitHeight

                    clip: true

                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }



                    ColumnLayout {

                        id: modGrid

                        width: parent.width

                        spacing: 6



                        // Loading state

                        Text {

                            visible: root.modSearching

                            Layout.fillWidth: true

                            text: "搜索中…"

                            color: "#606478"; font.pixelSize: 12

                            horizontalAlignment: Qt.AlignHCenter

                            Layout.topMargin: 20

                        }



                        // Empty state

                        Text {

                            visible: !root.modSearching && modResultsModel.count === 0

                            Layout.fillWidth: true

                            text: "未找到匹配的Mod"

                            color: "#606478"; font.pixelSize: 12

                            horizontalAlignment: Qt.AlignHCenter

                            Layout.topMargin: 20

                        }



                        // Results cards

                        Repeater {

                            model: modResultsModel



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

                                            text: "🔧"

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



                                    // Install button

                                    Rectangle {

                                        Layout.preferredWidth: 60; height: 26; radius: 5

                                        color: {

                                            if (root.modDownloadingSlugs[model.slug])

                                                return "#2a3040"

                                            if (installBtn.containsMouse)

                                                return "#5a78e0"

                                            return "#5068c8"

                                        }



                                        Text {

                                            text: root.modDownloadingSlugs[model.slug] ? "下载中…" : "安装"

                                            color: root.modDownloadingSlugs[model.slug] ? "#606478" : "#ffffff"

                                            font.pixelSize: 11

                                        }

                                        MouseArea {

                                            id: installBtn

                                            hoverEnabled: true

                                            cursorShape: Qt.PointingHandCursor

                                            onClicked: {

                                                if (root.modDownloadingSlugs[model.slug]) return

                                                console.log("[mod-ui] download slug=\"" + model.slug + "\" loader=" + root.modLoader)

                                                root.modDownloadingSlugs[model.slug] = true

                                                root.modDownloadingSlugs = Object.assign({}, root.modDownloadingSlugs)

                                                backend.downloadMod(model.slug, root.modLoader)

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
