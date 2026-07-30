// SPDX-License-Identifier: AGPL-3.0-or-later
// 联机状态指示灯 — 根据 role/state 显示颜色圆点 + 呼吸 + 颜色过渡
import QtQuick

Rectangle {
    id: root
    width: 10; height: 10
    radius: StyleTokens.radiusSm

    property int state: 0
    property int role: 0

    // Smooth color transitions when state changes
    Behavior on color {
        ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.colorEasing }
    }

    // State colors aligned with C++ enum:
    // 0=Idle, 1=CreatingRoom, 2=JoiningNetwork, 3=Discovering, 4=Connecting,
    // 5=Connected, 6=WaitingForGuests, 7=WaitingForMcServer,
    // 8=VerifyingConnection, 9=Error
    color: {
        if (state === 0 || role === 0) return StyleTokens.textMuted
        if (state === 9) return StyleTokens.error
        if (state === 5 || state === 6) return StyleTokens.success
        if (state === 7 || state === 8) return StyleTokens.warning
        return StyleTokens.info
    }

    // Breathing opacity for in-progress states
    SequentialAnimation on opacity {
        running: (root.state >= 1 && root.state <= 4) || root.state === 7 || root.state === 8
        loops: Animation.Infinite
        NumberAnimation { to: 0.3; duration: 900; easing.type: Easing.InOutSine }
        NumberAnimation { to: 0.7; duration: 900; easing.type: Easing.InOutSine }
    }
}
