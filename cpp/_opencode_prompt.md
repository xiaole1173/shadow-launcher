## 任务：替换 Settings 页面残留 ComboBox 为 ShadowDropdown + ShadowDropdown 增强

### 具体问题

**一、ShadowDropdown.qml 需要补充两个功能（之前被回滚了）**

1. 添加 `property bool enabled: true` — 在 `placeholderText` 属性后面
2. 在列表项文字渲染处（约第 190 行），当 `root._itemValue(modelData) === ""` 时显示 `root.placeholderText` 而不是空白

**二、SettingsJavaPage.qml ComboBox 替换**

第 140 行附近：`ComboBox { id: javaListCombo` — 替换为 ShadowDropdown
- 替换后保持 `Layout.fillWidth: true`
- 去掉 `textRole`/`valueRole`，使用 `valueKey: "path"`、`labelKey: "label"`
- model 保持不变（对象数组含 label/path/major）
- `currentValue` 绑定改为：`root._selectedJavaIndex >= 0 && root._javaList ? root._javaList[root._selectedJavaIndex].path : ""`
- `onValueSelected` 处理 `__browse__` 路径的逻辑
- 去掉旧的 background/contentItem/indicator/delegate/popup 块
- 去掉旧的 `onCurrentIndexChanged` 处理
- 同步调整右侧刷新按钮尺寸从 34x34 改为 28x28

**三、SettingsExperimentalPage.qml 两个 ComboBox 替换**

1. 第 75 行附近：`ComboBox { id: langCombo`（语言选择）
   - model 是字符串数组，直接传入
   - 用 `onValueSelected` 替换 `onActivated`
   - 去掉所有旧样式代码

2. 第 170 行附近：`ComboBox { id: langModeCombo`（游戏语言自动调整）
   - 同上处理

**四、SettingsGeneralPage.qml 检查**

已有 2 个 ShadowDropdown，确认样式一致即可，不需大改。

### 参考样式规范

所有替换的 ShadowDropdown 使用默认样式（不需要传 labelFn、placeholderText 等额外属性，除非需要）。样式统一为 ShadowDropdown 标准：
- 背景 `#0c0e14` / 悬停 `#1e3260`
- 边框 `borderLight` / 悬停 `#5078e0`
- 文字选中 `accentLink` / 未选中 `#788090`

### 安全规则
- 只改 qml/ 目录下的 .qml 文件
- 不动 CMakeLists.txt、C++、token、密码

### 完成后自行检查
- git diff --stat
- 检查语法错误、旧引用、死代码
- 确保 `onValueSelected` 信号正确绑定
- 删除旧的 background/contentItem/delegate/popup 代码

### 输出清单（必须）
- 修改了哪些文件
- 每个文件改了什么
- 行数统计
