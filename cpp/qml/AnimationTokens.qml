// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
pragma Singleton
import QtQuick 2.15

// ═══════════════════════════════════════
// AnimationTokens — 从 animation_tokens.json 加载动画参数
// 用法: AnimationTokens.buttonHover(target)  返回 Behavior
// ═══════════════════════════════════════

QtObject {
    id: root

    // ── 从 JSON 加载的原始数据 ──
    property var tokens: ({})

    Component.onCompleted: {
        var xhr = new XMLHttpRequest()
        xhr.open("GET", "animation_tokens.json", false)
        xhr.send()
        if (xhr.status === 200) {
            tokens = JSON.parse(xhr.responseText)
            console.log("[animation] Tokens loaded, elements:", Object.keys(tokens.elements || {}).length)
        } else {
            console.warn("[animation] Failed to load tokens, using defaults")
            loadDefaults()
        }
    }

    function loadDefaults() {
        tokens = {
            timing: { fast: 150, normal: 300, slow: 500, page_enter: 450 },
            easing: { default: "OutCubic", bounce: "OutBack", smooth: "InOutSine" }
        }
    }

    // ── Helpers ──

    function _ms(key) {
        return (tokens.timing && tokens.timing[key]) || 300
    }

    function _easing(key) {
        var name = (tokens.easing && tokens.easing[key]) || "OutCubic"
        switch (name) {
            case "OutCubic":   return Easing.OutCubic
            case "OutBack":    return Easing.OutBack
            case "OutElastic": return Easing.OutElastic
            case "InOutSine":  return Easing.InOutSine
            case "OutQuad":    return Easing.OutQuad
            case "InOutCubic": return Easing.InOutCubic
            default:           return Easing.OutCubic
        }
    }

    function _spring(key) {
        if (!tokens.spring || !tokens.spring[key]) return null
        return tokens.spring[key]
    }

    // ── NumberAnimation 工厂 ──

    function numberAnim(target, property, fromVal, toVal, durationKey, easingKey) {
        return Qt.createQmlObject(
            'import QtQuick 2.15; NumberAnimation { property: "' + property +
            '"; from: ' + fromVal + '; to: ' + toVal +
            '; duration: ' + _ms(durationKey) +
            '; easing.type: Easing.' + (tokens.easing ? tokens.easing[easingKey] || "OutCubic" : "OutCubic") + ' }',
            target
        )
    }

    // ── SmoothedAnimation 工厂 ──

    function smoothedAnim(target, property, toVal, velocity, durationKey) {
        return Qt.createQmlObject(
            'import QtQuick 2.15; SmoothedAnimation { property: "' + property +
            '"; to: ' + toVal +
            '; velocity: ' + (velocity || 200) +
            '; duration: ' + _ms(durationKey) + ' }',
            target
        )
    }

    // ── SpringAnimation 工厂 ──

    function springAnim(target, property, toVal, springKey) {
        var s = _spring(springKey)
        if (!s) {
            console.warn("[animation] Spring config not found:", springKey)
            return numberAnim(target, property, target[property], toVal, "normal", "default")
        }
        return Qt.createQmlObject(
            'import QtQuick 2.15; SpringAnimation { property: "' + property +
            '"; to: ' + toVal +
            '; velocity: ' + (s.velocity || 0) +
            '; damping: ' + (s.damping || 10) +
            '; mass: ' + (s.mass || 1.0) +
            '; stiffness: ' + (s.stiffness || 200) + ' }',
            target
        )
    }

    // ── 高级 Behavior 工厂（从 elements 配置创建完整 Behavior） ──

    function createBehavior(elementKey, target) {
        var el = tokens.elements ? tokens.elements[elementKey] : null
        if (!el) {
            console.warn("[animation] Unknown element:", elementKey)
            return null
        }

        var compStr = 'import QtQuick 2.15; Behavior { property: "' + el.target + '"; '

        if (el.spring) {
            var s = _spring(el.spring) || {}
            compStr += 'SpringAnimation { velocity: ' + (s.velocity || 0) +
                       '; damping: ' + (s.damping || 10) +
                       '; mass: ' + (s.mass || 1.0) +
                       '; stiffness: ' + (s.stiffness || 200) + ' }'
        } else {
            compStr += 'NumberAnimation { duration: ' + _ms(el.duration) +
                       '; easing.type: Easing.' + (tokens.easing ? tokens.easing[el.easing] || "OutCubic" : "OutCubic") + ' }'
        }

        compStr += '}'

        return Qt.createQmlObject(compStr, target)
    }

    // ── 快捷预设 ──

    function buttonHover(target)  { return createBehavior("button_hover", target) }
    function buttonPress(target)  { return createBehavior("button_press", target) }
    function buttonColor(target)  { return createBehavior("button_color", target) }
    function itemFadeIn(target)   { return createBehavior("list_item_enter", target) }
    function pageTransition(target) { return createBehavior("page_transition", target) }
    function menuBar(target)      { return createBehavior("menu_select_bar", target) }
    function toastSlideIn(target) { return createBehavior("toast_slide_in", target) }

    // ── 属性别名（可被 qmlcachegen 编译，适合 Behavior on X { } 内联引用） ──
    // 用法:
    //   Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
    //   Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
    //   Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeInDuration; easing.type: AnimationTokens.itemFadeInEasing } }
    //   Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }

    readonly property int buttonDuration: _ms("fast")
    readonly property int buttonEasing: Easing.OutCubic
    readonly property int itemFadeInDuration: _ms("normal")
    readonly property int itemFadeInEasing: Easing.OutCubic
    readonly property int itemFadeOutDuration: _ms("fast")
    readonly property int itemFadeOutEasing: Easing.OutCubic
    readonly property int colorDuration: _ms("fast")
    readonly property int colorEasing: Easing.OutCubic
    readonly property int pageDuration: _ms("page_enter")
    readonly property int pageEasing: Easing.OutBack

}
