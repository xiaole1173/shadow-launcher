## 任务：将所有内联刷新按钮替换为统一 RefreshButton 组件

### 具体问题

项目已有 `qml/RefreshButton.qml` 统一刷新按钮组件（30×30，SVG refresh-cw 图标，`#141820`→`#222a3a` 悬停），但以下页面仍在使用各自内联的刷新按钮：

### 需要替换的内联刷新按钮

**1. `qml/SettingsJavaPage.qml`** 第 182 行附近
```
Rectangle {
    id: refreshBtn
    width: 28; height: 28; radius: StyleTokens.radiusMd
    color: refreshHover.hovered ? StyleTokens.accentSubtle : "transparent"
    border.color: StyleTokens.bgHover
    ...
    Text { text: "↻" }  ← 文本图标，不是 SVG
}
```
→ 替换为 `RefreshButton { onClicked: { ... } }`
- `onClicked` 内容保持原样（扫描 Java 环境）
- 原 `enabled: root._mode === 1`（如果有）要保留，但 RefreshButton 没有 enabled 属性，所以去掉 enabled 条件

**2. `qml/VersionLaunchSection.qml`** 第 425 行附近
```
Rectangle {
    id: refreshBtn
    width: 30; height: 30; radius: StyleTokens.radiusMd
    color: refreshJavaHover.hovered ? "#222a3a" : "#141820"
    ...
    Image { source: "icons/lucide/refresh-cw.svg"; width: 16; height: 16 }
    ...
    enabled: root._mode === 1
}
```
→ 替换为 `RefreshButton { onClicked: { ... } }`
- 保留 onClicked 内容（重新扫描 Java）
- RefreshButton 没有 `enabled` 属性，去掉 `enabled: root._mode === 1`

**3. `qml/VersionSettingsOverlay.qml`** 第 567 行、第 848 行、第 1087 行附近（3处）
```
Rectangle {
    width: 30; height: 30; radius: StyleTokens.radiusMd
    color: X.hovered ? "#222a3a" : "#141820"
    ...
    Image { source: "icons/lucide/refresh-cw.svg"; width: 16; height: 16 }
    ...
}
```
→ 替换为 `RefreshButton { onClicked: { ... } }`

**4. `qml/VersionSelectOverlay.qml`** 第 220 行附近
```
Rectangle {
    id: verRefreshBtn
    width: 30; height: 30; radius: StyleTokens.radiusMd
    ...
    Image { source: "icons/lucide/refresh-cw.svg"; width: 16; height: 16 }
    ...
    property bool _syncVerRefresh: false  ← 自定义属性
    ...
}
```
→ 替换为 `RefreshButton { onClicked: { ... } }`
- 注意保留 `verRefreshPressed` 逻辑（如果有）

**5. `qml/HomePage.qml`** 第 1037 行附近（皮肤刷新按钮）
```
Rectangle {
    Layout.fillWidth: true; Layout.preferredHeight: 34
    ...
    Text { text: "刷新皮肤" }
    ...
}
```
→ 这是一整行按钮，不是图标按钮，检查是否需要替换。如果只是文字按钮"刷新皮肤"，保留原样不改。

### 替换规范
- `RefreshButton` 标准用法：`RefreshButton { onClicked: ... }`
- 替换后去掉旧的：id、background Rectangle、border、Image(source=refresh-cw.svg)、HoverHandler、MouseArea、Behavior on color/scale
- `onClicked` 内容直接放到 RefreshButton 的 `onClicked` 信号
- 如果原有 `Behavior on scale` 或 `Behavior on color`，全部去掉（RefreshButton 自带）
- 原有的 `id: refreshBtn` 如果有其他地方引用，需要同步处理

### 完成后自行检查
- git diff --stat
- 检查语法错误
- 确保所有 RefreshButton 的 `onClicked` 正确绑定
- 确保无残旧代码（旧的 Rectangle、Image refresh-cw.svg、HoverHandler、MouseArea）
- 检查 `Import` 是否有 `import QtQuick.Controls.Basic`（RefreshButton.qml 内自带 import，使用方不需要额外 import）

### 输出清单
- 修改了哪些文件
- 每个文件改了多少处
- 行数统计
