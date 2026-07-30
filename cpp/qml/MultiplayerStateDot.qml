// SPDX-License-Identifier: AGPL-3.0-or-later
// 联机状态指示灯 — 根据 role/state 显示颜色圆点
import QtQuick

Rectangle {
    id: root
    width: 10; height: 10
    radius: StyleTokens.radiusSm

    // 联机状态枚举: 0=Idle 1=CreatingRoom 2=JoiningNetwork 3=Discovering
    // 4=Connecting 5=Connected 6=WaitingForGuests 7=VerifyingConnection 8=Error
    property int state: 0
    property int role: 0  // 0=None 1=Host 2=Guest

    color: {
        if (state === 0 || role === 0) return StyleTokens.textMuted          // Idle/None → 灰
        if (state === 8) return StyleTokens.error                            // Error → 红
        if (state === 5 || state === 6) return StyleTokens.success           // Connected/Waiting → 绿
        if (state === 7) return StyleTokens.warning                          // Verifying → 黄
        return StyleTokens.info                                              // 其他进行中 → 蓝
    }

    // 呼吸动画（进行中状态）
    opacity: (state >= 1 && state <= 4) || state === 7 ? 0.6 : 1.0
    Behavior on opacity { NumberAnimation { duration: 800; easing.type: Easing.InOutSine } }

    SequentialAnimation on opacity {
        running: (root.state >= 1 && root.state <= 4) || root.state === 7
        loops: Animation.Infinite
        NumberAnimation { to: 0.3; duration: 800; easing.type: Easing.InOutSine }
        NumberAnimation { to: 0.6; duration: 800; easing.type: Easing.InOutSine }
    }
}
