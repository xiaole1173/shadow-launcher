// SPDX-License-Identifier: AGPL-3.0-or-later
// 联机帮助说明面板 — 组件化可折叠 + 平滑展开/折叠动画
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    implicitHeight: expanded ? col.implicitHeight + 28 : headerHeight + 16
    clip: true
    color: StyleTokens.bgPrimary
    border.color: StyleTokens.bgElevated; border.width: 1
    radius: StyleTokens.radiusLg
    Behavior on implicitHeight {
        NumberAnimation { duration: AnimationTokens.panelEnterDuration; easing.type: AnimationTokens.panelEnterEasing }
    }

    property bool expanded: false
    readonly property int headerHeight: 36

    // Clickable header
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.expanded = !root.expanded
    }

    ColumnLayout {
        id: col
        width: parent.width - 28
        x: 14; y: expanded ? 14 : 6
        spacing: 10

        // ── Header: text stays upright, only icon rotates ──
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "联机帮助"
                font.pixelSize: StyleTokens.fontSizeMd
                font.bold: true
                color: StyleTokens.textSecondary
            }
            Item { Layout.fillWidth: true }

            // Static text — no rotation
            Text {
                id: expandLabel
                text: root.expanded ? "收起" : "展开"
                font.pixelSize: StyleTokens.fontSizeSm
                color: StyleTokens.textSubtle
            }

            // Animated chevron icon — rotates 180° when expanded
            Image {
                id: chevronIcon
                source: "icons/lucide/chevron-down.svg"
                width: 16; height: 16
                sourceSize.width: 16; sourceSize.height: 16
                Layout.alignment: Qt.AlignVCenter
                rotation: root.expanded ? 180 : 0
                Behavior on rotation {
                    NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing }
                }
            }
        }

        // ── Expandable content with fade + slide ──
        ColumnLayout {
            spacing: 10
            visible: root.expanded
            opacity: root.expanded ? 1 : 0
            Behavior on opacity {
                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
            }

            Section { title: "联机架构" }
            Text {
                text: "本启动器集成了「陶瓦联机」（Terracotta）。陶瓦联机使用 P2P 技术，联机成功后房间内用户之间将直接连接，不会使用第三方服务器对您的流量进行转发。最终联机体验和参与联机者的网络情况有较大关系。"
                wrapMode: Text.WordWrap; Layout.fillWidth: true
                color: StyleTokens.textTertiary; font.pixelSize: StyleTokens.fontSizeMd; lineHeight: 1.4
            }

            Section { title: "房主操作" }
            Text {
                text: "1. 点击「创建房间」按钮\n2. 系统会为 EasyTier 请求管理员权限（虚拟组网所需；启动器本身不会提权重启）\n3. 打开游戏进入单人世界，在游戏内点击「对局域网开放」\n4. 启动器会自动识别房间并生成「房间码」\n5. 将「房间码」复制并分享给好友"
                wrapMode: Text.WordWrap; Layout.fillWidth: true
                color: StyleTokens.textTertiary; font.pixelSize: StyleTokens.fontSizeMd; lineHeight: 1.6
            }

            Section { title: "访客操作" }
            Text {
                text: "1. 点击「加入房间」\n2. 粘贴房主分享的房间码（格式: U/XXXX-XXXX-XXXX-XXXX）\n3. 系统会为 EasyTier 请求管理员权限\n4. 自动发现房主 → 连接 → 获取 MC 服务器地址\n5. 打开 MC，在多人游戏 -> 局域网列表中就会看到「联机 MC服务器」，点击即可加入"
                wrapMode: Text.WordWrap; Layout.fillWidth: true
                color: StyleTokens.textTertiary; font.pixelSize: StyleTokens.fontSizeMd; lineHeight: 1.6
            }

            Section { title: "NAT 类型说明" }
            Text {
                text: "连接难度反映了您的网络环境：\n-  直连 — 公网 IP（Open Internet），最佳体验\n-  简单 — NoPAT / FullCone NAT，多数公网环境\n-  中等 — 受限 NAT（Restricted / PortRestricted），可能需要更长时间建立连接\n-  困难 — 对称 NAT，连接可能不稳定或需要中继\n\nEasyTier 会自动选择最优路径。如果连接有问题，可以尝试关闭防火墙或使用手机热点作为备选网络。"
                wrapMode: Text.WordWrap; Layout.fillWidth: true
                color: StyleTokens.textTertiary; font.pixelSize: StyleTokens.fontSizeMd; lineHeight: 1.6
            }

            Section { title: "端口说明" }
            Text {
                text: "联机功能会占用以下本地端口：\n-  EasyTier RPC: 由房间码确定性生成（15880-65535）\n-  Scaffolding 联机端口: 20000-30000（由房间码派生）\n-  MC 服务器端口: 房主本机 MC 服务器的端口（启动器自动扫描检测，与房间码无关）\n\n端口转发通过 EasyTier 内建功能自动建立，无需手动配置路由器。"
                wrapMode: Text.WordWrap; Layout.fillWidth: true
                color: StyleTokens.textTertiary; font.pixelSize: StyleTokens.fontSizeMd; lineHeight: 1.6
            }

            Section { title: "常见问题排查" }
            Text {
                text: "Q: 加入房间后一直卡在「发现中心」\nA: 请确认房主已创建房间且网络畅通。如超过 30 秒仍未连接，可能是 NAT 穿透遇到困难或无法访问公共中继节点，尝试双方重启软件重试。\n\nQ: MC 列表中没有显示联机服务器\nA: 访客连接成功后，MC 的多人游戏界面的局域网联机列表中会出现服务器（名为「联机 MC服务器」）。如有多个 LAN 服务器，可以手动输入 127.0.0.1:{端口号} 直接连接，端口号可在「网络监控」面板的 MC端口 处查看。\n\nQ: 提示「EasyTier 提权启动失败」或「提权失败」\nA: 请在 UAC 弹窗中点击「是」授权 EasyTier；若被安全软件拦截请临时放行。启动器本身无需以管理员身份运行。\n\nQ: 连接后延迟很高\nA: 检查 NAT 类型。如果需要中转，延迟会高于直连。对称 NAT 环境下建议使用 5G 手机热点规避。"
                wrapMode: Text.WordWrap; Layout.fillWidth: true
                color: StyleTokens.textTertiary; font.pixelSize: StyleTokens.fontSizeMd; lineHeight: 1.6
            }
        }
    }

    // Helper component for section headers — bold colored text
    component Section: Text {
        property string title
        text: title
        font.pixelSize: StyleTokens.fontSizeSm
        font.bold: true
        color: StyleTokens.info
        Layout.fillWidth: true
        wrapMode: Text.NoWrap
    }
}
