# 功能完整性审计报告（版本选择 / 版本设置 / 设置）

> 审计日期：2026-08-03 ｜ 审计对象：Shadow Launcher（Windows）
> 范围：「版本选择」「版本设置」「设置」各功能的完整性与可用性；设置-实验性功能**不在**审计范围内。
> 方法：QML↔C++ 绑定链核对 + 全量断链扫描（QML 调用的 backend 方法是否真实存在）+ 运行时 eval 实证。

---

## 一、结论摘要

| 区域 | 结论 |
|---|---|
| 游戏完整性校验 + 一键修复（重点） | ✅ **完整可用**（链路验证见 §2） |
| 版本选择（VersionSelectOverlay） | ✅ 基本完整；1 个未实现功能（见 §4-②） |
| 版本设置（VersionSettingsOverlay） | ✅ 完整；审计中发现 1 个隐藏 bug + 3 个断链，均已修复 |
| 设置（SettingsPage 5 分区） | ✅ 结构完整，无占位/空壳 |
| 断链扫描（backend.xxx 调用） | ✅ 修复 3 处真断链后为 0（见 §3） |
| 死代码 | ⚠️ 5 个 QML 文件无引用（见 §4-④） |

---

## 二、重点链路：游戏完整性校验 + 修复（验证通过）

**UI（VersionSettingsOverlay 工具与维护分区）**：
- 「开始校验」→ `backend.verifyVersion(版本)`；校验中按钮禁用 + 进度条（done/total + 百分比）
- 失败通知（红色横幅"检测到 N 个文件异常"）
- 「一键修复」→ `backend.repairVersion(版本)`；「[详情] 查看异常详情」→ `backend.openVerifyReport()`

**C++（version_backend.cpp）**：
- `verifyVersion`（L3420）：线程化（worker）校验；收集 SHA1 期望值（client.jar + libraries + assets 索引）→ 逐文件比对 → 进度信号（verifyStarted/verifyProgress/verifyFinished/verifyFailedFiles）
- `cancelVerify`（L3926）/ `cleanCorruptVersion`（L3968）
- `repairVersion`（L4028）：从失败缓存 `m_failedPathsCache` 构建修复目标（BMCLAPI 主源 + Mojang 官方降级 + SHA1）→ 下载重写
- 自动修复两阶段：安装/下载失败 → verify 找出损坏 → repair 下载修复 → `installFinished`

**信号转发（shadow_backend.cpp）**：verify* 四个信号全部转发到 QML；`verifyFailedFiles` 同时生成报告文件 `{gameDir}/verify_reports/verify_failed_{时间戳}.txt`。

**eval 实测**：`versionDetails` 正常输出（含 loaderType/versionType）；校验链路无断链。✅

---

## 三、断链扫描结果（自动检查）

QML 全部 `backend.xxx(...)` 调用（139 个方法名）与 shadow_backend.h 的 Q_INVOKABLE 比对：

| 方法 | 状态 | 说明 |
|---|---|---|
| `installOptifineJar` | 🔧 **已修复** | 只存在于 VersionBackend，ShadowBackend 缺转发 → InstallPage 调用必报错 |
| `setPendingUserDataImport` | 🔧 **已修复** | 同上（整合包导入-用户数据迁移） |
| `cancelPendingUserDataImport` | 🔧 **已修复** | 同上 |
| `logMessage` | ✅ 正常 | shadow_backend 的信号（QML 可发射） |
| 其余 135 个 | ✅ 正常 | 含全部子后端属性访问（account.capes / yggdrasil.loggedIn 等）均核实存在 |

修复：shadow_backend.{h,cpp} 新增 3 个转发方法（已编译验证）。

---

## 四、发现问题清单

| # | 问题 | 状态 |
|---|---|---|
| ① | 概览"Mod 文件夹"按钮可见性用 `backend.isModdedVersion()`（C++ stub 恒 false）→ 永远隐藏 | ✅ 已修（改本地判断） |
| ② | 版本选择"+ 添加文件夹"→ 提示"功能开发中"（无添加游戏目录 UI） | ⚠️ 未实现，需产品决策 |
| ③ | 概览"迁移目录"按钮禁用态（注释 disabled until implemented） | ⚠️ 未实现，需产品决策 |
| ④ | 死代码 5 文件：SubPageOverlays / VersionSelectPage / VersionSettingsPage / SettingsGeneralPage / SettingsMemoryPage（无任何引用，被 MainWindow Overlay 形态取代；VersionSettingsPage 内含 TODO 空壳按钮，用户不可见） | ⚠️ 建议清理或归档标注 |
| ⑤ | 概览"config 文件夹"按钮：名不副实（openConfigFolder 打开 .minecraft 根目录）+ 语义不清 | ✅ 已移除 |
| ⑥ | 概览"光影包"按钮：加载器≠有光影加载器，判定繁琐 | ✅ 已移除 |
| ⑦ | 概览 Mod 按钮可见性：readonly property 中转不生效（QML 绑定追踪陷阱） | ✅ 已修（内联表达式） |

---

## 五、各界面功能核验明细

### 5.1 版本选择（VersionSelectOverlay.qml，实际生效）
- ✅ 游戏目录列表：切换（左键）、右键移除（index 0 打开目录）、磁盘空间条
- ⚠️ 添加文件夹：开发中占位（见 §4-②）
- ✅ 导入整合包 → modpackImportOverlay
- ✅ 已安装版本列表：搜索 / 排序（版本↓/版本↑/大小/模组数）/ 加载器过滤（全部/原版/Forge/Fabric/NeoForge/Quilt）/ 图标按类型 / 左键选择 / 右键进版本设置 / 空状态提示
- ✅ 安装新版本 → 下载页

### 5.2 版本设置（VersionSettingsOverlay.qml，7 分区）
- **概览**：快捷入口（文件夹 4 + 日志 3 + 其他 1，Mod 文件夹仅加载器版）、加载器标签、顶部启动
- **启动配置**：VersionLaunchSection（Java/参数/GPU 分版本配置）
- **内存设置**：VersionMemorySection（分版本内存）
- **Mod 管理**（仅加载器版显示）：异步列表、搜索、拖拽导入、刷新、删除、打开文件夹
- **资源包管理**：异步列表、搜索、拖拽导入、刷新
- **存档管理**：异步列表、删除
- **工具与维护**：完整性校验 / 一键修复 / 查看报告；克隆版本、重命名版本、导出用户数据（进度态）；迁移目录（禁用）

### 5.3 设置（SettingsPage.qml，5 分区）
- **通用设置**：内联组件（下载源/线程/限速、主题、语言、游戏目录、协议等）
- **Java 设置**：SettingsJavaPage.qml（列表/扫描/选择）
- **内存设置**：内联组件
- **实验性功能**：SettingsExperimentalPage.qml（**审计范围外**）
- **关于**：内联组件（版本号/开源链接/日志目录）

---

## 六、审计方法备忘（可复用）

1. **断链扫描**：正则提取 QML `backend.(\w+)\(` 与 shadow_backend.h Q_INVOKABLE 比对；子后端 `backend.obj.(\w+)` 与各类 header 比对（注意区分 Q_PROPERTY/信号）
2. **eval 实证**：Release 构建的 ScreenshotServer 也监听 9999（非 NDEBUG stub），`POST /eval {qml: expr}` 可查运行时状态（versionDetails/属性/对象树）
3. 占位/空壳扫描：搜"开发中/TODO/敬请期待"与 onClicked 空实现
