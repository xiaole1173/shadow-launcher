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

        // ── Header ──
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "🛈 联机帮助"
                font.pixelSize: StyleTokens.fontSizeMd
                font.bold: true
                color: StyleTokens.textSecondary
            }
            Item { Layout.fillWidth: true }
            Text {
                text: expanded ? "收起 ▲" : "展开 ▼"
                font.pixelSize: StyleTokens.fontSizeSm
                color: StyleTokens.textSubtle
                // Rotate chevron indicator
                rotation: expanded ? 0 : 180
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
            Text { text: "本启动器使用 EasyTier 虚拟组网 + Scaffolding 协议实现 NAT 穿透联机。房主创建虚拟局域网后，访客通过 EasyTier 网络连接到房主，实现类似局域网联机的体验。"; wrapMode: Text.WordWrap; color: StyleTokens.textTertiary; font.pixelSize: StyleTokens.fontSizeMd; lineHeight: 1.4 }

            Section { title: "房主操作" }
            Text { text: "1. 点击「创建房间」按钮\n2. 系统会要求提权（管理员权限）—— 这是因为 EasyTier 需要创建虚拟网络接口\n3. 提权后等待「虚拟网络就绪」\n4. 将自动生成的「房间码」复制并分享给好友\n5. 房主需要先启动 MC 服务器（本地或服务端均可），启动器会自动将 MC 服务器暴露到联机网络"; wrapMode: Text.WordWrap; color: StyleTokens.textTertiary; font.pixelSize: StyleTokens.fontSizeMd; lineHeight: 1.6 }

            Section { title: "访客操作" }
            Text { text: "1. 点击「加入房间」\n2. 粘贴房主分享的房间码（格式: U/XXXX-XXXX-XXXX-XXXX）\n3. 系统会要求提权\n4. 自动发现房主 → 连接 → 获取 MC 服务器地址\n5. 启动器会自动在本地创建端口转发\n6. 打开 MC，在多人游戏 -> 局域网列表中就会看到「联机 MC服务器」，点击即可加入"; wrapMode: Text.WordWrap; color: StyleTokens.textTertiary; font.pixelSize: StyleTokens.fontSizeMd; lineHeight: 1.6 }

            Section { title: "NAT 类型说明" }
            Text { text: "连接难度反映了您的网络环境：\n• 直连 — 公网 IP 或 FullCone NAT，最佳体验\n• 简单 — 中等 NAT 类型，大部分网络环境\n• 中等 — 受限 NAT，可能需要更长时间建立连接\n• 困难 — 对称 NAT，连接可能不稳定或需要中继\n\nEasyTier 会自动选择最优路径。如果连接有问题，可以尝试关闭防火墙或使用手机热点作为备选网络。"; wrapMode: Text.WordWrap; color: StyleTokens.textTertiary; font.pixelSize: StyleTokens.fontSizeMd; lineHeight: 1.6 }

            Section { title: "端口说明" }
            Text { text: "联机功能会占用以下本地端口：\n• EasyTier RPC: 动态分配（15880-65535）\n• Scaffolding 联机端口: 20000-30000\n• MC 服务器端口: 参考每个房间码生成\n\n端口转发通过 EasyTier 内建功能自动建立，无需手动配置路由器。"; wrapMode: Text.WordWrap; color: StyleTokens.textTertiary; font.pixelSize: StyleTokens.fontSizeMd; lineHeight: 1.6 }

            Section { title: "常见问题排查" }
            Text { text: "Q: 加入房间后一直卡在「发现中心」\nA: 请确认房主已创建房间且网络通畅。如超过 60 秒仍未连接，可能是 NAT 穿透遇到困难，尝试双方重启软件重试。\n\nQ: MC 列表中没有显示联机服务器\nA: 访客连接成功后，MC 的多人游戏界面的局域网联机列表中会出现服务器。如有多个 LAN 服务器，可以手动输入 127.0.0.1:{端口号} 直接连接。\n\nQ: 提示「提权失败」\nA: 请以管理员身份运行启动器，或关闭杀毒软件后重试。\n\nQ: 连接后延迟很高\nA: 检查 NAT 类型。如果需要中转，延迟会高于直连。对称 NAT 环境下建议使用 5G 手机热点规避。"; wrapMode: Text.WordWrap; color: StyleTokens.textTertiary; font.pixelSize: StyleTokens.fontSizeMd; lineHeight: 1.6 }
        }
    }

    // Helper component for section headers
    component Section: Text {
        property string title
        text: "▸ " + title
        font.pixelSize: StyleTokens.fontSizeSm
        font.bold: true
        color: StyleTokens.info
    }
}
