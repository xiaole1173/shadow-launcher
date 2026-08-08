# Shadow Launcher 代码功能归档（CODE INDEX）

> 本文档是 Shadow Launcher（C++17 / Qt 6.8 / QML，Windows）**全部代码文件的功能查询索引**。
> 用途：①让不读代码的人也能快速知道每个文件是干什么的；②找功能代码时**先查本表**再动手，避免大海捞针。
>
> **维护铁律**（必须遵守）：
> 1. 每次新增/修改/删除代码文件，**必须同步更新本文档**；
> 2. 编辑只能使用 `edit` 或增量追加，**绝对禁止整文件覆盖重写**；
> 3. 本文档不含敏感信息（密钥/令牌/地址均为机制描述），可安全提交仓库。
>
> 更新日志见文末「文档更新记录」。

---

## 〇、速查：按功能找文件

| 想找的功能 | 看这里 |
|---|---|
| 2026-08-05 | **下载引擎全面对齐 主流启动器语义 + 进度/速度修复 + 尾程加速**（详见速查表下方新增「下载引擎 2026-08-05 大修」章节）：源策略（哈希混合分流废除→顺序 fallback）、速度门限（256KB/s→4MB/s 瞬时差分）、镜像不分片+限速、自适应超时、per-file 源禁用+Retried、无数据超时修复、分片阈值 1MB、山海经调度/大小降序、进度失真 4 处（总量丢失/分片进度/初始100%/超额虚高）、**尾程慢速分片看门狗**（慢分片一分为二并跑）、**分片截断字节二次扣减修正**。 |
| 主程序入口 / 启动流程 | `src/main_release.cpp`（发布）、`src/main.cpp`（开发变体） |
| QML 主界面唯一入口对象 `backend` | `src/backend/shadow_backend.{h,cpp}` |
| 版本列表/安装/删除/校验/修复/隔离 | `src/backend/version_backend.{h,cpp}` |
| 启动游戏（进程/参数/Token） | `src/backend/launch_backend.{h,cpp}`、`src/core/launcher.{h,cpp}` |
| 账号（离线/微软正版/皮肤/披风） | `src/backend/account_backend.{h,cpp}`、`src/core/microsoft_auth.*` |
| 外置登录（Yggdrasil/authlib-injector） | `src/backend/yggdrasil_backend.*`、`src/core/yggdrasil_auth.*` |
| 设置持久化 | `src/backend/settings_backend.{h,cpp}` |
| Java 扫描/选择 | `src/backend/java_backend.{h,cpp}` |
| 游戏时长统计 | `src/backend/stats_backend.{h,cpp}` |
| 启动前检查（Java/版本文件/内存） | `src/backend/check_backend.{h,cpp}` |
| 游戏完整性校验/修复 | `version_backend.cpp` L3420 `verifyVersion` / L3926 `cancelVerify` / L3968 `cleanCorruptVersion` / L4028 `repairVersion` |
| 下载中心：Mod/资源包/光影/整合包搜索 | `src/backend/resource_backend.{h,cpp}`、`src/core/resource_fetch_engine.*`（司南）、`src/core/cf_api.*`（CF） |
| 下载引擎（通用批量） | `src/core/file_downloader.*`（夸父）、`src/core/downloader.*`（旧单文件） |
| 下载引擎（assets 资源） | `src/core/asset_downloader.*`（山海经） |
| 版本安装管线（client+libs+assets） | `src/core/version_downloader.*`（盘古） |
| Mod 批量下载 | `src/core/modpack/mod_download_engine.*`（精卫） |
| 整合包下载/解析/安装 | `modpack/modpack_downloader.*`（女娲）、`modpack_parser.*`、`modpack_install_task.*`、`modpack_importer.*` |
| Forge/NeoForge/OptiFine 安装 | `src/core/mod_loader_installer.{h,cpp}`（三分支：Legacy2/1/Bootstrapper，Legacy3 已删 2026-08-08） |
| 本地 Mod/资源包/存档管理 | `src/core/local_mod_manager.{h,cpp}` |
| 联机（陶瓦 Terracotta 兼容） | `src/multiplayer/multiplayer_manager.{h,cpp}`（核心） |
| EasyTier 进程/TOML/白名单 | `src/multiplayer/easytier_process.{h,cpp}` |
| 房码算法 | `src/multiplayer/room_code.{h,cpp}` |
| Scaffolding 协议 | `src/multiplayer/scaffolding_protocol.{h,cpp}` |
| UAC 提权接力 | `src/multiplayer/elevated_session.{h,cpp}` |
| 更新检查/安装 | `src/core/update_checker.*`、`src/core/update_manager.*`、`src/update/SLUpdater.cpp`（更新器进程） |
| 引擎雅名体系（盘古/夸父…） | `src/core/engine_identity.h` |
| 安装步骤管线（进度 UI 模型） | `src/core/step_pipeline.*`、`src/core/step_node.*` |
| 主窗口/全局路由 | `qml/MainWindow.qml` |
| 主页 | `qml/HomePage.qml` |
| 版本选择 | `qml/VersionSelectPage.qml`、`qml/VersionSelectOverlay.qml` |
| 版本设置 | `qml/VersionSettingsPage.qml`、`qml/VersionSettingsOverlay.qml` |
| 设置 | `qml/SettingsPage.qml` + `SettingsGeneral/Java/Memory/ExperimentalPage.qml` |
| 统计 | `qml/StatsPage.qml` |
| 联机 | `qml/MultiplayerPage.qml` + `Multiplayer*` 组件族 |
| 设计令牌（颜色/字号/圆角） | `qml/StyleTokens.qml`、`qml/AnimationTokens.qml` |
| Toast 通知 | `qml/ToastManager.qml` |
| 通用按钮/输入框/下拉/开关 | `qml/ShadowButton/ShadowIconButton/ShadowSwitch/ShadowDropdown/InputBox/SearchBox` |

## 下载引擎 2026-08-05 大修（对齐 主流启动器语义）

> 背景：用户实测下载速度曲线异常（前快中慢后停摆）+ 进度失真 + 超额下载浪费，
> 连续 7 轮对照 主流启动器（ModNet.vb / ModDownload.vb）逐行核对修正。
> 涉及：`file_downloader.*`（夸父）、`asset_downloader.*`（山海经）、`version_downloader.*`（盘古）、`version_backend.cpp`（进度计算）。

### 核心语义修正（对照 主流启动器）
| 项 | 旧（错误） | 新（主流启动器语义） | 位置 |
|---|---|---|---|
| 源策略 | 哈希 65/35 官方+镜像混合分流 | **顺序 fallback**：源列表=[首选组,备选组]，从源 0 找第一个可用；首选组全禁用才用备选组 | file_downloader.cpp tryStartFirstThread/tryAddThread、asset_downloader.cpp fireNext |
| 速度门限 | 3s EMA ≥256KB/s 不加分片（官方单连接 300KB/s 永不触发） | **瞬时差分 m_instantBps ≥4MB/s** 才不加分片（低于期望速度就分片补偿） | file_downloader.h kSpeedLimitLowBps=4MB、speedTick 更新 m_instantBps |
| 源禁用 | 官方试 2 次就切镜像 | **per-file FailCount≥5 且无进度才禁用**（sourceFailCounts）；全禁用重置重试一轮（retriedOnce） | file_downloader.h FileDownload.sourceFailCounts/retriedOnce |
| 全局降级 | 3 次连续失败就全局拉黑 host | **10 次**（接近完全停摆才全局降级，防偶发失败架空官方优先） | recordHostResult |
| 镜像处理 | 镜像参与分片 | **镜像禁止分片**（isMirrorUrl 检查选中源）+ 每线程请求限速 100ms | tryAddThread、runWorker |
| 超时 | 固定 12s/30s | **自适应**：min(max(avg,15s)×(1+fails),30s)（同主流启动器）；无数据超时 readyRead 重置（防误杀大文件） | runWorker、asset_downloader fireNext |
| 调度节拍 | 50ms | **20ms**（同主流启动器 StartManager） | file_downloader.h kManagerTickMs |
| 分片阈值 | FilePieceLimit 512KB / isNoSplit 50MB | **256KB / 1MB**（同主流启动器） | tryAddThread、addFile |

### 进度显示修复（version_backend.cpp + 引擎层）
1. **总量丢失**：阶段 B startAssets 不再重置 m_categoryTotalBytes[1]（libs 总量被 assets 任务 collectTasks 清 0 → 支持库 0/0KB）
2. **分片进度缺失**：fileProgress 上报文件级总进度（file->totalDone()/fileSize），不再报单分片 received（上层按 savePath 去重只计第一个分片）
3. **初始 100% 闪**：catBytesTotal<=0 时不再用全局 bytesDl 臆断 completed（pending 0%）
4. **超额虚高**：downloadDone 封顶线程范围（服务器返回全文件时进度不虚高）
5. 文件完成补发满字节帧（进度精确到 100%）

### 超额下载浪费修复
- **根因**：fileSize 被分片响应（206 contentLen=分片大小）缩小 → isNoSplit 翻转 → 后续无 Range → 服务器返回全文件 200（23.9MB 只留 132KB）
- **修复**：fileSize 更新只允许补差（actualSize>fileSize），不允许缩小
- 200 全文件响应：isFullFile 标志整文件直接使用（mergeFile 优先），不截断浪费
- 实测验证：curl + Qt 最小程序确认官方 CDN 正确返回 206 精确分片（排除服务器/Qt 问题）

### 山海经（assets）调度修复
- PhaseCooldown 误触发（小文件间隙速度归零 → 并发砍半 → 90% 后暴跌）：冷却条件收紧 pending>4
- 任务大小降序（startDownload/appendTasks）：大文件先下（87 个 >1MB，最大 the_end.ogg 11MB），消除收尾停摆
- 超时加 15s 下限（主流启动器 max(avg,15s)）

### 尾程加速：慢速分片看门狗（2026-08-05 追加）
- **背景**：大文件收尾只剩 1-2 个分片在跑，碰上慢边缘连接（实测 fastutil 尾部 195KB 以 16KB/s 爬 12s，前 12s 还整体无数据超时）→ 整批下载最后一步龟速。分片只在剩余 ≥256KB 时切分（主流启动器 FilePieceLimit），尾部小分片即使龟速也永不重分。
- **实现（file_downloader.{h,cpp}）**：
  1. managerTick 每 500ms 扫描活动分片（state=2），滚动基线算瞬时 bps，持续 2s <128KB/s → `trySplitSlowThread` 将剩余范围中点一分为二并开新连接并跑；
  2. **切分后中止老连接**（关键）：老请求还在按旧 range 龟速流，不中止则分片白切（老线程要等整条旧响应流完才截断）。worker 侧 500ms 轮询 QTimer 消费主线程置的 `watchdogAbort` 原子标志，自己 abort 自己的 reply（线程安全），重试时以缩小后的范围换新连接（有机会命中快边缘）；
  3. **中止不算失败**：不走 FailCount 累计，且跳过“快速失败守卫”（attempt≥2 且 <5.5s 即 break，否则看门狗中止会把重试链掐断）；每线程中止预算 2 次（kMaxWatchdogAborts，防 attempt 耗尽）；
  4. 约束：并发上限 maxThreads（MC 64/模组 12）；镜像源不分片；剩余 <32KB 不再切；老分片响应超范围数据由既有截断逻辑裁剪。
- **配套字节修正**：分片中段被切分后 downloadProgress 已按范围封顶计数，但完成时旧代码无条件扣 excess → 超额部分二次扣减 → m_downloadedBytes 欠计（尾部速度/进度显示被拉低）；改为只扣“已计入但超出最终范围”的部分（`counted - finalDone`），未计入超额由新分片另行计数，总量自洽。
- **本地实测**（FDTest + 自建慢边缘服务器 _slow_tail_server.py：尾部 30% range 16KB/s，其余 2MB/s）：2.5MB 文件 18.3s → 10.4s（MC 模式 64 线程）/ 10.3s（modpack 24 线程），SHA1 校验一致；**迭代教训**：①QList append 扩容使引用/迭代器失效崩溃 → 下标遍历+原始指针；②轮询定时器曾误装进最终兜底块（无 sha1 时永不执行）→ 主请求循环；③看门狗中止触发快速失败守卫 break → prevWatchdogAbort 绕过。

### 超额下载浪费修复（2026-08-07，file_downloader.cpp，commit 7ec05bd + d8098b2）

- **问题**：日志实锤 476 次截断，预期 707MB 实际收到 1968MB，白白浪费 1261MB（178%）——全部是 206 状态但服务器返回远超请求范围的数据
- **根因**：tryAddThread 加速切分只缩小 th->downloadEnd，但 in-flight HTTP 请求仍按旧 range 拉数据（首线程请求全文件被切分后服务器仍发完整文件，如 26.2.jar 起始=0 预期=12.9MB 实际=39MB）→ worker 等整条旧响应收完再截断丢弃
- **修复（对齐主流启动器实现 ModNet.vb 流式语义：DownloadUndone=0 即断开）**：worker 在 downloadProgress 里检测 206 响应已收满本线程范围（received > downloadEnd-downloadStart，严格大于避免正常 206 误触发）→ 立即 abort 止损；数据前缀完整（截断写盘逻辑裁剪到范围），不丢进度、不需重试、不计 FailCount；200 全文件响应（isFullFile 优化）不触发
- **⚠️ 数据丢失修复（d8098b2）**：实测 QNetworkReply::abort() 会清空未读缓冲（abort 后 readAll 返回 0，FDTest 只合并出 916KB/12MB）→ 必须 abort 前先 readAll 保存到 rangeData，主流程统一用 rangeData
- **效果**：超额从整个旧 range（可达全文件）降到约一个网络包；本地 4 场景验证 SHA1 全部匹配

### 双引擎架构（保留）
- 支持库 >1MB → 夸父（分片加速）；≤1MB → 山海经（独立并发）；阶段 B assets 追加到山海经（appendTasks）
- 完成判定：m_assetTasksDone 仅由 allFinished 置位（山海经小库+assets 全完成才算）

### 卡片总进度统一为步骤管线加权（2026-08-05 追加，version_backend.cpp）
- **背景**：总进度三套口径打架——①纯 MC 卡片=步骤加权（JSON1.0 隐藏/支持库3.0/资源5.0/校验1.0）；②merged 卡片=字节 EMA，且 `updateDownloadProgress` 里对同一 session 的 smoothProgress **写两次、两套公式**（公式 A：mcRaw=0.5×支持库+0.5×资源，漏 cat0；公式 B：grandTotal 字节加权，无加载器 0.5 封顶），主版本/次版本口径不一致；③安装页主进度 m_installBytesDl/Total 注释宣称“下载 0-90%/校验 90-100%”实际下载段爬到 100% 后校验段从 0 重新开始 → 进度条回跳。
- **修复**：
  1. merged 卡片 progress 全部改用 `totalProgress()`（步骤管线加权，与纯 MC 同构）；删除公式 A/B 的 smoothProgress 写入（mcBytesDl/mcBytesAll 保留）；canCancel/dismissAllCompleted/installProgressOf 同步改 totalProgress 口径
  2. **client.jar 澄清**：collectTasks 分类 cat0=version JSON（50KB，HTTP race 下载），cat1=client.jar+libraries，cat2=assets——client.jar 归支持库，本就计入；merged JSON 步骤（权重 3.0）因 cat0 不走分类统计会永远 pending → 下载有字节流即标 completed
  3. syncPrimaryProgress 真两段式：下载 ×0.9 + 校验 ×0.1 折算 0-100 刻度（字段无 QML 消费者，仅保留语义）；删 updateDownloadProgress 里的重复写入
- 效果：纯 MC 下载完成 88.9%（8/9）→ 校验 100%；merged 下载完成 83.5% → 校验 86.2% → 加载器 100%，无回跳、无提前绿卡

---

## 一、C++ 源码（`src/`）

### 1.1 入口与主程序

| 文件 | 行数 | 功能 |
|---|---|---|
| `src/main_release.cpp` | 814 | **发布版入口（CMake 实际编译）**。初始化 QML 引擎、统一后端聚合、Beta 内测密钥闸门（无保存密钥 → 先弹 `BetaKeyDialog`，`betaVerified` 后加载 MainWindow）、`TaskbarMinimizeFilter`（最小化到托盘相关）、崩溃/日志初始化。始终从 qrc 预编译资源加载 QML。窗口层：`setColor(transparent)` + **禁用 Win11 DWM 系统圆角（DWMWCP_DONOTROUND）**——防浅色主题下四角露出系统背景色白角（2026-08-03 修）。截图模式 `--navigate settings:about` 支持设置页 section 导航（0-4）。 |
| `src/main.cpp` | 854 | **开发变体入口（未编入 CMake）**。与 main_release 逻辑相同，但支持 `SHADOW_DEV` 环境变量：从文件系统路径加载 QML 便于热调试；同样含透明背景 + DWM 圆角禁用。 |

### 1.2 后端聚合层（`src/backend/`，QML 通过 `backend` 单对象访问）

| 文件 | 行数 | 功能 |
|---|---|---|
| `shadow_backend.h/.cpp` | 857 / 4450 | **总聚合后端，QML 的 `backend` 对象**。聚合全部子后端（account/version/launch/resource/settings/java/stats/userdata/check/yggdrasil/multiplayer/modManager…），转发数百个 Q_PROPERTY/Q_INVOKABLE；也含少量自有逻辑：GeoIP 地区离线限制（`isOfflineRestricted`）、Beta 密钥校验落盘、自定义背景、Toast/UI 消息通道、`checkAll` 启动检查汇总；崩溃分析信号转发（`crashAnalysisStarted`/`crashAnalysisReady`）+ `analyzeCrashNow`/`exportCrashLogs`/`openPath`；**Java 一键安装完成 → 自动刷新设置-Java 列表 + 一键安装卡片前置检测**。 |
| `version_backend.h/.cpp` | 466 / 8287 | **版本管理大后端**：版本清单拉取/刷新（release/snapshot/old/aprilfool）、安装（走 VersionDownloader）、删除/重命名/克隆/迁移隔离、`verifyVersion`（游戏完整性校验）/`cancelVerify`/`cleanCorruptVersion`/`repairVersion`（修复，基于下载器 SHA1 校验重下缺失/损坏文件）、版本详情（Mod/资源包/存档列表异步）、installCards 模型、merged 安装上下文。 |
| `launch_backend.h/.cpp` | 161 / 1480 | **启动后端**：组装 JVM/游戏参数、Token 刷新决策（`msTokenValid`/`shouldRefresh`）、进程启停（`launch`/`cancelLaunch`/`killGame*`）、在线/离线模式路由；**崩溃分析异步链路**：启动失败 → `crashAnalysisStarted` → `runCrashAnalysis`（QTimer 异步）→ `crashAnalysisReady`；`analyzeCrashNow`/`exportCrashLogs`/`openPath` Q_INVOKABLE。 |
| `account_backend.h/.cpp` | 156 / 1207 | **账号后端**：离线登录（用户名/UUID/历史）、微软正版登录（MicrosoftAuth 封装：token 管理/后台刷新/过期判断）、皮肤下载/上传/缓存、披风（CapeInfo）、3D 头像渲染触发、离线皮肤。 |
| `resource_backend.h/.cpp` | 247 / 1772 | **资源中心后端（下载页）**：Mod/资源包/光影/整合包搜索与详情（Modrinth+CurseForge 双源，分页池架构）、分类、版本列表、依赖解析、下载任务管理（下载队列/进度/取消/暂停/重试）、图标批量缓存。 |
| `settings_backend.h/.cpp` | 245 / 1195 | **设置后端**：全部设置项读写（QSettings）、下载源/线程/限速、主题、语言、游戏目录、Java 默认、JVM/游戏参数、内存自动分配、背景图、协议同意状态等；scanJavaInstallations 完成回调保证每次扫描只 emit 一次 javaScanFinished；findAllJava 额外扫描启动器 java_cache（一键安装的便携 Java 可在设置-Java 选用）。 |
| `java_backend.h/.cpp` | 136 / 438 | **Java 后端**：扫描系统 Java、版本检测（`java -version` 解析主版本）、自动选择、指定路径管理；Tuna Adoptium 目录浏览（版本/类型/架构/OS/文件五级）；**一键安装所需 Java**（转发 JavaRuntimeInstaller：前置检测 + 8/17/25 JRE 顺序安装 + 步骤/下载进度/速度信号）。 |
| `java_runtime_installer.h/.cpp` | 155 / 900 | **一键安装 Java 运行时**：Tuna Adoptium ZIP 下载+解压到 java_cache/{ver}/（便携式不写注册表，同 主流启动器/主流启动器）；架构检测（x64/x32/aarch64/arm，ARM64 降级 x64 模拟）；**版本策略 8/17/25 全部 JRE**；**完整前置检测**（后台线程递归扫描，javac.exe 判定 JDK/JRE，已有同 major 任意类型→跳过）；**异步状态机安装**（列目录→downloadWithReply 实时进度→**后台线程解压**（UI 不冻结）→校验）；**完整性校验 isJavaComplete**（-version 能跑 ≠ 完整：检查 lib/modules(9+)/rt.jar(8)/jvm.dll/release）；安装链状态存成员（修复悬空引用崩溃）；**失败自愈**：残缺检测删除重装、自动重试 1 次、启动清理残留；路径统一 applicationDirPath。 |
| `stats_backend.h/.cpp` | 53 / 142 | **统计后端**：游戏时长统计（按版本聚合，读取启动记录）。 |
| `userdata_backend.h/.cpp` | 99 / 503 | **用户数据后端**：用户目录数据管理（皮肤缓存、头像、可迁移数据）。 |
| `check_backend.h/.cpp` | 38 / 442 | **启动前 P0 检查**（同步快速）：Java 架构 32/64 位、版本 client.jar 存在性、version.json 合法性、可用内存；`checkAll` 汇总。 |
| `yggdrasil_backend.h/.cpp` | 132 / 553 | **外置登录后端**：Yggdrasil/authlib-injector 认证（服务器地址、登录/登出、UUID/皮肤）。 |
| `yggdrasil_skin_fetcher.h/.cpp` | 49 / 201 | **外置皮肤拉取**：从 Yggdrasil 服务器下载皮肤/披风并缓存。 |
| `app_backend.h/.cpp` | 59 / 93 | **应用信息后端**：版本号、应用名等静态信息。 |
| `debug_logger.h/.cpp` | 53 / 102 | **调试日志**：调试构建下的日志辅助。 |

### 1.3 核心引擎（`src/core/`）

| 文件 | 行数 | 功能 |
|---|---|---|
| `engine_identity.h` | 58 | **下载引擎雅名注册表**：盘古(VersionDownloader)/夸父(FileDownloader)/山海经(AssetDownloader)/精卫(ModDownloadEngine)/女娲(ModpackDownloader)/驿道(HttpClient)/司南(ResourceFetchEngine)；`engineTag`/`engineBanner` 日志前缀。 |
| `http_client.h/.cpp` | 175 / 802 | **HTTP 传输底座（驿道）**：全引擎共用的 QNetworkAccessManager 封装；v2 起支持 >4MB 文件 Range 多线程分片、分片探测、限速、连接池、重试。 |
| `file_downloader.h/.cpp` | 240 / 1180 | **通用批量文件下载引擎（夸父）**：QThreadPool 并发分块加速、SHA1 校验、断点续传、主机健康、缓存命中。**缓存命中修复（2026-08-04，commit 25fc4d5）**：addFile 只累计 m_cacheHits/m_cacheBytes，start() 统一入账（避免进度条从高完成度开始）；全缓存命中（m_files 空）时 start() 立即 allFinished（否则卡死）。 |
| `asset_downloader.h/.cpp` | 240 / 1181 | **assets 专项下载（山海经）**：异步 SHA1 预检（IO 池）、异步 DNS、多镜像降级、objects 索引解析。 |
| `version_downloader.h/.cpp` | 232 / 1331 | **版本安装管线（盘古）**：下载 client.jar + libraries + assets 到版本目录（merged 任务中可指向 UUID 临时目录）、JSON 解析、文件清单生成、校验。 |
| `mod_manager.h/.cpp` | 264 / 1651 | **Mod 下载管理**：下载任务（驿道 downloadWithReply 断点续传，2026-08-02 自夸父回退）、Modpack 模式。⚠️ **用户 WIP（勿改勿提交）**。 |
| `local_mod_manager.h/.cpp` | 104 / 782 | **本地 Mod/资源包管理**：扫描 mods/ 目录、解析 JAR（读取 mods.toml/fabric.mod.json 元数据）、Mod 列表/过滤、删除、导入复制。 |
| `mod_loader_installer.h/.cpp` | 283 / 4016 | **Forge/NeoForge/OptiFine 安装器**：三分支（Legacy2/1 安装器 + Bootstrapper 模式，Legacy3 已删 2026-08-08）；Forge install_profile 处理、处理器列表、FART/srgutils、版本 JSON 生成；含 Java 自动下载（Tuna Adoptium 镜像 JRE——安装器只 java -cp 跑 jar 无需 JDK，与一键安装共享 java_cache）；java_cache 路径统一 applicationDirPath；toastMessage 信号（自动下载 Java 前/完成时弹全局 Toast）。**安装收尾重活已搬离主线程（2026-08-06，commit 0ee14f6）**：runInstallTask（QtConcurrent worker + 回主线程 onDone + 取消/失败标志 m_installWorkerFailed）；forgeStep3 拆 壳/prepareImpl(worker)/route(三分支)；bootstrapper 前置（JAR 剥离/mappings 预下载/TSRG 转换）→ worker；fabricStep3 拆 壳/Impl(worker)。遗留主线程：Java 自动下载解压、Legacy2/1、finalizeBootstrapperInstall、OptiFine 整目录复制。 |
| `launcher.h/.cpp` | 105 / 1710 | **游戏启动核心**：实际启动 Minecraft 进程（参数组装、natives 解压、JVM 启动、进程监控、退出码处理）、服务器属性准备；**输出环形缓冲**（recentOutput，最近 600 行原始输出供崩溃分析）。 |
| `microsoft_auth.h/.cpp` | 64 / 357 | **微软 OAuth 认证**：设备码/浏览器流程、XBL→XSTS→Minecraft→Profile 四步链、token 获取与刷新。 |
| `yggdrasil_auth.h/.cpp` | 87 / 266 | **Yggdrasil 认证**：外置登录协议实现（与服务器握手、校验、token）。 |
| `cf_api.h/.cpp` | 73 / 370 | **CurseForge API 适配器**：搜索/分类/版本/依赖请求（走镜像 /curseforge/v1/）。 |
| `cf_api_key_local.h` | 13 | CF API Key 本地源（占位/注入点）。 |
| `cf_key_crypto.h` | 171 | **CF API Key 解密**：与 tools/encrypt_cf_key.py 对应的 AES-256-GCM 解密运行时。 |
| `resource_fetch_engine.h/.cpp` | 110 / 293 | **资源拉取引擎（司南）**：统一调度 Modrinth/CurseForge 搜索 API 与图标拉取，三层缓存 + 本地缩略图 + 严格并发控制。 |
| `geoip_service.h/.cpp` | 59 / 126 | **IP 地区检测**：ip-api.com，24h 缓存（QSettings），失败 5 分钟自动重试；供离线登录限制（非 CN 未正版登录禁止离线）与语言/版本区域适配。 |
| `icon_cache.h/.cpp` | 46 / 105 | **图标缓存**：网络图标（webp）→ 本地 PNG 缓存。 |
| `mc_language.h/.cpp` | 30 / 122 | **MC 语言映射**：地区码 → Minecraft 语言/region 设置（options.txt）。 |
| `crash_detector.h/.cpp` | 141 / 1150 | **崩溃分析引擎（v2 完整版）**：主流启动器 式 51 条正则规则库（OpenJ9/内存/Mod冲突/Mixin/OptiFine兼容等）+ 堆栈关键词黑名单分析 + 日志收集（崩溃报告/hs_err/latest.log/debug.log）+ 一键导出 + Markdown 报告生成（过长自动落盘）。`analyzeCrash(gameDir, latestOutput, launcherLog)` 全链路入口；**无崩溃报告但有 JVM 输出时也用输出做规则分析**（同主流启动器，覆盖 Java 不匹配/启动即退）；CrashReport 新增 jvmOutput 字段供 UI 展示。 |
| `screenshot_server.h/.cpp` | 79 / 361 | **调试截图服务器**（Debug 构建）：/eval + /screenshot 远程调试接口。 |
| `step_node.h/.cpp` | 85 / 61 | **步骤节点**：安装/下载步骤的状态/进度/字节计数 QObject（Q_PROPERTY+NOTIFY 供 QML 绑定）。 |
| `step_pipeline.h/.cpp` | 95 / 202 | **步骤管线**：StepModel（QAbstractListModel）+ StepPipeline（加权进度/推进/取消），驱动安装进度 UI。 |
| `update_checker.h/.cpp` | 48 / 128 | **更新检查**：查询最新版本（版本 JSON）。 |
| `update_manager.h/.cpp` | 119 / 496 | **更新管理**：下载更新包、校验、解压替换、重启更新器。 |
| `version_manager.h/.cpp` | 86 / 320 | **版本清单**：从 Mojang manifest 拉取版本列表（release/snapshot/old）。 |
| `version_isolation.h/.cpp` | 42 / 212 | **版本隔离**：独立 .minecraft 目录的创建/迁移/删除。 |
| `render_head_util.h` | 68 | **3D 头像渲染工具**：皮肤 → 头像渲染辅助。 |
| `modpack_importer.h/.cpp` | 135 / 249 | **整合包导入**：本地 zip/mrpack 导入入口（校验、落盘标记 `.shadow_modpack`）。 |
| `cef_login_app.h/.cpp` | 19 / 9 | CEF 登录辅助（占位/启动壳，微软登录嵌入式浏览器用）。 |
| `logger.h/.cpp` | 33 / 129 | 日志分类定义与输出（logApp/logLaunch/logLoader/logNet…）。 |

### 1.4 整合包子域（`src/core/modpack/`）

| 文件 | 行数 | 功能 |
|---|---|---|
| `modpack_common.h/.cpp` | 74 / 108 | 整合包公共常量/工具。 |
| `modpack_parser.h/.cpp` | 34 / 276 | **整合包 manifest 解析**（CF/Modrinth/通用格式）。 |
| `modpack_downloader.h/.cpp` | 144 / 831 | **整合包编排下载（女娲）**：manifest → 文件清单 → 并行下载 → 覆盖备份钩子。 |
| `mod_download_engine.h/.cpp` | 135 / 532 | **批量模组小文件下载（精卫）**：多源自降级、SHA1/大小校验、EMA 网速统计、**任务大小降序 + 慢速看门狗（500ms 扫描，<128KB/s 持续 2s → 换源，单源/全源慢时放弃看门狗宁慢不误判）**（2026-08-06，commit 待提交）。 |
| `modpack_install_task.h/.cpp` | 181 / 1280 | **整合包安装任务**：合并安装上下文（MergedInstallContext，主流启动器 式 UUID 临时目录隔离）、加载器安装编排、步骤管线、取消清理。 |
| `zip_archive.h/.cpp` | 71 / 283 | ZIP 解压封装（QZipReader 包装）。 |

### 1.5 联机子域（`src/multiplayer/`，陶瓦 Terracotta 兼容）

| 文件 | 行数 | 功能 |
|---|---|---|
| `multiplayer_manager.h/.cpp` | 276 / 2152 | **联机核心**：建房/加入房间状态机（Idle/CreatingRoom/JoiningNetwork/Discovering/Connecting/Connected/VerifyingConnection/WaitingForGuests/Error…）、Scaffolding 服务端（host）与客户端（guest）、协议 handler（ping/protocols/server_port/player_ping/profiles_list）、指纹验证、心跳、玩家列表同步、FakeServer（MC 局域网广播）、MC 扫描、连接难度计算（NAT 四档）、重连、UAC 提权接力、退出兜底清理。 |
| `easytier_process.h/.cpp` | 132 / 732 | **EasyTier 进程管理**：easytier-core/cli 查找与启停、TOML 配置生成（`[[peer]]` 表数组 + 陶瓦公共节点）、`--no-tun` 参数、RPC 端口确定性生成、端口转发（port-forward add）、TCP 白名单动态更新、peer 表轮询解析（虚拟 IP/NAT 类型/host 活跃性 cost 过滤）。 |
| `room_code.h/.cpp` | 26 / 111 | **房码算法**：16 位 base34（不含 I/O）整体 mod 7 校验（对齐陶瓦）、生成/解析、I→1/O→0 兼容。 |
| `scaffolding_protocol.h/.cpp` | 46 / 59 | **Scaffolding 协议**：请求包 `[typeLen][type][bodyLen][body]`、响应包 `[status][bodyLen][body]`（无 type，FIFO 匹配）构建工具。 |
| `mc_scanner.h/.cpp` | 64 / 205 | **MC LAN 扫描器**：UDP 224.0.2.60:4445 监听真实 MC 服务器广播，检测 MC 端口。 |
| `port_request.h/.cpp` | 19 / 34 | **端口请求**：requestSpecific/requestFree（对齐陶瓦 ports.rs）。 |
| `connection_guard.h/.cpp` | 47 / 109 | **频率限制**：房码/连接/包速率防护。 |
| `elevated_session.h/.cpp` | 55 / 113 | **UAC 提权接力**：非提权实例保存联机参数到临时配置 → 提权重启自身 → 读取继续（--elevate-config）。 |
| `relay_crypto.h/.cpp` | 28 / 148 | **中继端点解密**：AES-256-GCM 解密 243 字节 blob（中继地址/前缀），开源构建回落全零占位。 |
| `encrypted_addr.h` | 37 | 加密 blob 布局定义（offset/len，全零占位；真实值在 .gitignore 的本地文件，由 tools/encrypt_addr.py 生成）。 |
| `encrypted_frag_1..5.cpp/.h` | 8×5 | 加密 blob 分片（混淆存放，开源构建为占位桩），`encrypted_frag_stub.cpp` 为桩实现。 |

### 1.6 会话与工具（`src/session/`、`src/utils/`）

| 文件 | 行数 | 功能 |
|---|---|---|
| `session/download_session.h/.cpp` | 147 / 137 | **下载会话**：跨任务下载会话状态（合并安装上下文关联）。 |
| `utils/types.h` | 157 | 公共类型/枚举定义。 |
| `utils/logger.h/.cpp` | 42 / 228 | 日志工具（文件日志、分类）。 |
| `utils/hash_utils.h` | 18 | 哈希工具（SHA1 等）。 |
| `utils/secure_wipe.h` | 24 | 安全内存擦除（SecureZeroMemory）。 |
| `utils/temp_tracker.h/.cpp` | 32 / 141 | **临时目录追踪**：记录/清理 `%TEMP%/shadow-merged-*` 残留（启动时 cleanupOrphans）。 |
| `utils/token_crypto.h/.cpp` | 38 / 322 | **令牌加密**：微软 refresh token 等敏感数据落盘加密。 |
| `utils/lzma/LzmaDec.h`、`Types.h` | 237 / 83 | LZMA 解压（7z 支持）。 |

### 1.7 其他

| 文件 | 行数 | 功能 |
|---|---|---|
| `update/SLUpdater.cpp` | — | 独立更新器进程（下载并替换主程序）。 |
| `tools/`（仓库） | — | 加密生成脚本：`encrypt_cf_key.py`、`encrypt_addr.py`（不在 src 内，见 git）。 |

---

## 二、QML 界面（`qml/`）

### 2.1 窗口 / 全局

| 文件 | 行数 | 功能 |
|---|---|---|
| `MainWindow.qml` | 1456 | **主窗口**：全 UI 骨架、侧边导航（navIndicator 光条）、页面路由（Loader 加载各页面，0=启动 1=下载 2=联机 3=统计 4=设置 5=安装进度）、全局 DropArea（整合包/Mod/资源包拖拽导入路由）、子浮层（版本选择/版本设置/设置等 Overlay）、ToastManager 挂载、协议同意闸门；崩溃分析接线（`onCrashAnalysisStarted`→Toast「启动失败，正在分析日志信息…」+弹窗分析态，`onCrashAnalysisReady`→结果态）；`--navigate settings:xxx` 支持设置页 section 切换。 |
| `SplashWindow.qml` | 50 | 启动画面。 |
| `StyleTokens.qml` | 133 | **设计令牌**：颜色（bg/accent/text 系列）、字号、圆角、间距常量。 |
| `AnimationTokens.qml` | 187 | **动画令牌**：时长/缓动曲线常量。 |
| `ToastManager.qml` | 144 | **Toast 通知**：右下角堆叠、天蓝色、`show(msg, duration)`。 |
| `ToastStyleSuccess.qml` / `ToastStyleWarning.qml` | 27×2 | Toast 成功/警告样式常量。 |
| `AgreementOverlay.qml` | 329 | 用户协议/隐私/条款同意浮层（HTML 渲染）。 |
| `BackgroundCropOverlay.qml` | 281 | 自定义背景裁剪设置浮层。 |
| `BetaKeyDialog.qml` | 154 | 内测密钥输入窗口（无密钥时主程序先加载它）。 |
| `DebugWindow.qml` / `DebugPanel.qml` | 164 / 132 | 调试窗口/面板（Debug 构建）。 |
| `SubPageOverlays.qml` | 138 | 子页面浮层容器（版本选择/设置等 Overlay 的路由壳）。 |

### 2.2 页面（导航主页面）

| 文件 | 行数 | 功能 |
|---|---|---|
| `HomePage.qml` | 1395 | **主页**：版本快捷选择卡、离线/正版/外置登录切换、启动按钮、游戏时长/最近游玩、背景。 |
| `DownloadPage.qml` | 2166 | **下载中心**：Tab 页（Mod/资源包/光影/整合包）、搜索/筛选（FilterCard）/分页（PaginationFooter）、结果网格、下载队列入口。 |
| `InstallPage.qml` | 849 | **安装页**：版本安装（原版/Forge/Fabric/NeoForge/OptiFine 选择、ModLoaderCard 列表）。 |
| `InstallProgressPage.qml` | 72 | 安装进度页（步骤管线展示）。 |
| `VersionSelectPage.qml` | 404 | **版本选择页**（独立页形态，替代旧左栏）。 |
| `VersionSettingsPage.qml` | 1067 | **版本设置页**（独立页形态）。 |
| `SettingsPage.qml` | 676 | **设置页**：左侧分类导航 → 各 Settings*Page（通用/Java/内存/实验/关于）；关于页含「一键安装所需 Java」卡片（架构徽标 + 前置检测状态三行 + 小字说明 + **下载进度条（百分比/字节/速度）** + 安装中/取消按钮 + Toast 完成与异常反馈 + logMessage 关键提示）。 |
| `SettingsGeneralPage.qml` | 883 | 设置-通用：下载源/线程/限速、主题、语言、游戏目录、协议等。 |
| `SettingsJavaPage.qml` | 408 | 设置-Java：Java 列表/选择/扫描；刷新按钮异步扫描（scanJavaInstallations → onJavaPathChanged → refreshAll 刷新列表+Toast，修复同步读旧缓存问题）。 |
| `SettingsMemoryPage.qml` | 247 | 设置-内存（汇总视图）。 |
| `SettingsMemorySection.qml` | 368 | 内存条组件（游戏分配标签文字自适应钳制）。 |
| `SettingsExperimentalPage.qml` | 261 | 设置-实验性功能（审计范围外）。 |
| `JavaPage.qml` | 620 | Java 管理页（详情/列表）。 |
| `StatsPage.qml` | 297 | **统计页**：版本游戏时长条（Hover 显示完整版本名 tooltip——2026-08-03 改为自定义 Popup 圆角框，替代系统默认米白尖角 ToolTip）。 |
| `SubPageOverlays.qml` | 138 | **⚠️ 死代码（无任何引用）**：旧式全页覆盖加载器（VersionSelectPage/VersionSettingsPage/Settings*Page），被 MainWindow 的 Overlay 形态取代。 |
| `MultiplayerPage.qml` | 336 | **联机页**：创建/加入房间、状态指示、房间码卡、网络监控、玩家列表、帮助面板、底部合规声明。 |

### 2.3 详情/子页面

| 文件 | 行数 | 功能 |
|---|---|---|
| `ModpackDetailPage.qml` | 457 | 整合包详情（版本列表/下载/导入）。 |
| `ModDetailPage.qml` | 748 | Mod 详情（简介/版本/依赖/下载）。 |
| `ResourcePackDetailPage.qml` | 494 | 资源包详情。 |
| `ShaderDetailPage.qml` | 407 | 光影详情。 |
| `ModpackImportOverlay.qml` | 328 | 整合包导入浮层（拖拽/选文件/自定义名）。 |
| `InstallConfigOverlay.qml` | 151 | 安装配置浮层（版本名/加载器配置）。 |
| `LaunchOverlay.qml` | 516 | 启动覆盖层（启动中状态/日志）。 |
| `CrashDialog.qml` | 330 | **崩溃分析弹窗（v2）**：双态——分析中（LoadingSpinner 转圈+提示文案）/结果（原因、建议列表、嫌疑模组标签云、报告文件提示）；按钮：重新分析/导出全部日志/打开崩溃报告/打开报告/关闭；入场 scale+opacity 动画（AnimationTokens）；`beginAnalyzing()` + `crashData` 属性驱动。 |

### 2.4 版本相关

| 文件 | 行数 | 功能 |
|---|---|---|
| `VersionCard.qml` | 115 | 版本卡片（列表项）。 |
| `DetailVersionCard.qml` | 173 | 版本详情卡（概览）。 |
| `DetailInfoCard.qml` | 110 | 通用信息卡（详情页统计项）。 |
| VersionSelectOverlay.qml | 404 | 版本选择浮层（单卡片占满整页：已安装版本列表 + 顶部工具行 标题/刷新/搜索/导入整合包/安装/排序/筛选；2026-08-07 左侧版本文件夹卡片已删）。 |
| `VersionSettingsOverlay.qml` | 1490 | **版本设置浮层（实际生效）**：7 分区（概览0/启动配置1/内存2/Mod管理3/资源包4/存档5/工具6），各分区内容 + 顶部启动按钮。概览快捷入口 2026-08-03 分类重做：统一 ShadowButton + 文件夹/日志/其他分组；Mod 文件夹按钮 visible 内联白名单判定（lt ∈ Forge/Fabric/NeoForge/Quilt，与 sidebar Mod 管理同款写法，原版必隐藏）；光影包/config 按钮已移除。 |
| `VersionLaunchSection.qml` | 599 | 启动配置分区（Java/参数/GPU）。 |
| `VersionMemorySection.qml` | 251 | 内存分区。 |

### 2.5 联机组件族（`Multiplayer*`）

| 文件 | 行数 | 功能 |
|---|---|---|
| `MultiplayerHelpPanel.qml` | 132 | 联机帮助折叠面板（架构/房主/访客/NAT/端口/FAQ）。 |
| `MultiplayerStateIndicator.qml` | 112 | 状态指示面板（角色徽章/状态文字/进度条）。 |
| `MultiplayerStateDot.qml` | 37 | 状态指示灯（颜色圆点+呼吸）。 |
| `MultiplayerRoomCodeCard.qml` | 121 | 房间码卡（复制/难度徽章）。 |
| `MultiplayerNetworkPanel.qml` | 119 | 网络监控面板（难度/协议/MC端口/在线玩家/指纹）。 |
| `MultiplayerPlayerCard.qml` | 147 | 玩家卡（名称/身份/延迟动画）。 |

### 2.6 通用组件

| 文件 | 行数 | 功能 |
|---|---|---|
| `ShadowButton.qml` | 95 | 通用按钮（hover/press 缩放动效）。 |
| `ShadowIconButton.qml` | 122 | 图标按钮。 |
| `ShadowSwitch.qml` | 70 | 开关。 |
| `ShadowDropdown.qml` | 222 | 下拉选择。 |
| `LabeledDropdown.qml` | 36 | 带标签下拉。 |
| `InputBox.qml` | 274 | 输入框（密码模式/历史下拉/校验错误态/右侧按钮）。 |
| `SearchBox.qml` | 55 | 搜索框。 |
| `ConfirmDialog.qml` | 72 | 确认弹窗。 |
| `GenericPopup.qml` | 177 | 通用弹窗（标题/内容/按钮）。 |
| `SelectionPopup.qml` | 217 | 选择列表弹窗。 |
| `ProfileSelectPopup.qml` | 214 | 头像/配置选择弹窗。 |
| `LoadingSpinner.qml` | 69 | 加载转圈（Canvas）。 |
| `LoadStatus.qml` | 53 | 加载状态条。 |
| `KillButton.qml` | 97 | 红色终止按钮。 |
| `RefreshButton.qml` | 38 | 刷新按钮。 |
| `BackButton.qml` | 53 | 返回按钮。 |
| `PaginationFooter.qml` | 137 | 分页脚（上一页/页码/下一页）。 |
| `FilterCard.qml` | 347 | 筛选卡（分类/加载器/版本过滤）。 |
| `ExpandableGroupCard.qml` | 82 | 可折叠分组卡。 |
| `DownloadCard.qml` | 320 | 下载项卡片（进度/速度/状态）。 |
| `DownloadCardTagRow.qml` | 44 | 下载卡标签行。 |
| `DownloadQueueCard.qml` | 366 | 下载队列卡片（队列项）。 |
| `DownloadQueuePanel.qml` | 198 | 下载队列面板（右下角浮层）。 |
| `ModLoaderCard.qml` | 163 | 加载器卡片（Forge/Fabric/NeoForge/OptiFine 选择）。 |
| `ModpackInfoPanel.qml` | 149 | 整合包信息面板。 |
| `MinecraftHead2D.qml` | 83 | 2D MC 头像（皮肤渲染）。 |
| `InlineToast.qml` | 65 | 内联提示条。 |

---

## 三、文档更新记录

| 日期 | 说明 |
|---|---|
| 2026-08-08 | 启动自动安装 Java（Java 缺失时）：shadow_backend launch() 的 javaPath.isEmpty() 不再直接 launchBlocked → autoInstallJavaThenLaunch（版本归一化 16→17）→ JavaBackend::installJavaForLaunch（复用 JavaRuntimeInstaller::installJavaAsync，worker 下载/解压+主线程回调）→ 完成回调 scanJavaInstallations 刷新 + 用 onDone 返回的 javaExe 直接 proceedLaunch；进度经 launchCheckWarning/Progress → LaunchOverlay Java 下载 toast（对齐 authlib 卡片，独立 _javaDlActive 状态）；cancelLaunch 中止安装。测试：JavaAutoInstallTest（真实网络 8/21 下载解压 PASS）、JavaRulesTest（规则 19 用例 PASS） |
| 2026-08-08 | 启动自动安装 Java 架构修正（b5b8784 后 + 本轮）：自动安装下沉到 LaunchBackend 状态机（runNextCheck Step 1）——Java 无效时不再在 ShadowBackend::launch 提前拦截，而是 overlay 立即显示 + Step 1 触发安装（launchCheckWarning/Progress 驱动 Java toast）+ 完成回调更新 m_pendingJavaPath 继续状态机；ShadowBackend 只 setJavaAutoInstallRequest(major) + 注入 setJavaInstaller；删 autoInstallJavaThenLaunch/快照成员；JavaBackend::installJavaForLaunch 加 onProgress(pct,status) |
| 2026-08-08 | Java 自动安装取消语义：LaunchBackend 注入 JavaCancelFn（JavaRuntimeInstaller::cancelInstall）；cancelLaunch 时下载中 abort+清理（failJob 删 zip/半解压目录）、解压中不打断（解压线程不读 m_cancelled）；JavaCancelTest 回归（已缓存不误删/失败清理残留） |
| 2026-08-08 | 启动时序重构：Java 检测完全下沉 LaunchBackend 状态机 Step 1（注入 JavaMajorResolverFn/JavaMatcherFn，ShadowBackend::launch 删整段匹配代码）；startJavaAutoInstall 防重入（stop timer + 忽略重复请求）——修复双触发锁冲突 bug |
| 2026-08-08 | Legacy3 全量清理（备份 backup/legacy3-cleanup-2026-08-08/）：删 installLegacy3 函数+声明+forgeStep3_install 的 legacy3 分支（改报错）；forgeStep1/fallback 链 universal/client 类别删除只留 installer.jar；删 installerLibsSkipped 信号+version_backend 连接+QML skipped 显示；Forge147Test 注释更新。legacy2 取消安全：installLegacy2/1 置 m_syncInstallRunning（destroyMergedContext 不删写中 tempDir）+ m_cancelled 快速失败 + 库下载 QEventLoop cancelPoll（200ms abort）+ cancelVersionInstall 同步安装中延迟销毁 + finished handler ctx->failed 清理。验证：1.5.2/1.6.4 Forge147Test ok=1 |
| 2026-08-08 | Mod disabled 解析修复（scanMods）：禁用文件用真实路径 parseJar（.disabled 本身是 zip 可读），fileName 保留真实名+enabled=false；minimal 分支剥 .disabled 取 baseName。ToggleScriptTest 新增真实 zip disabled 解析断言 |
| 2026-08-07 | Mod 启禁用 + 启动脚本导出（00804ed，+357/-6）：local_mod_manager setModEnabled（.jar↔.jar.disabled）+ scanMods 识别禁用态；launcher buildLaunchScript 复用 buildArgs 输出 .bat；launch/shadow backend 转发 + saveTextFile；VersionSettingsOverlay Mod 卡片启禁用按钮 + 概览其他导出脚本按钮；ToggleScriptTest 回归 |
| 2026-08-07 | Forge 列表过滤扩到 1.5 及更早（5ef27ee）：queryForgeVersions 开头正则 ^1\.[0-5] 拦截（FML 4.x 需 fmllibs，官方源死 404；主流启动器 源码证实 1.5.2 也要 argo-small-3.2 等）；1.6+ 保留 |
| 2026-08-07 | Forge 版本列表只保留 installer 类别（d1674a1，对齐 主流启动器）：BMCLAPI/官方双解析器过滤无 installer 版本——1.4.7 及更早（35 版本全 universal）列表为空，1.5.2+ 不受影响；1.4.7 fmllibs 官方源已死（404）故从根源杜绝 |
| 2026-08-07 | Legacy 3 根本性修正（bd85c62）：实测 universal.zip（746）非自包含 JAR——client.jar 1933 条目、universal 缺 1688 个混淆类（lg 等）→ NoClassDefFoundError；重写 installLegacy3：游戏 JAR=原版 client.jar 副本 + universal→libraries/forge-{ver}.jar + flatten |
| 2026-08-07 | 1.4.7 Legacy 3 mainClass 修正（d1939e4）：恒用 net.minecraft.client.Minecraft（实测 FMLRelauncher 无 main，FML 通过 Minecraft.fmlReentry 注入）；删 JAR 扫描逻辑 |
| 2026-08-07 | 1.4.7+forge 启动崩溃修复（bca513b）：launcher pre-1.6 mainClass 覆盖加 FML 例外（FMLRelauncher 保留）；version_isolation 隔离模式统一返回 game/（删降级 verDir 分支，junction 不再建到 versions/.minecraft）；删 isDirNonEmpty 死代码 |
| 2026-08-07 | 开启 JVM 标准/错误输出到日志（1cd4bac）：launcher onReadyReadStdout/Stderr 逐行 qCInfo（[JVM 输出]/[JVM 错误输出] 前缀，曾按用户要求关闭）；runBootstrapperSync 改每次迭代 drain 两通道（原依赖 waitForReadyRead 返回值，纯 stderr 进程会阻塞）+ stdout/stderr 写日志；OptiFine 安装器 capturedOutput 实际内容写日志（原只打行数） |
| 2026-08-07 | merged 1.4.7 主文件下载失败修复（f9ad0e4）：fallback 链补 universal.zip/client.zip 三后缀（回滚时丢 9bca7f5）；新增 Yidao147Test 回归（驿道下载 universal.zip PK 魔数） |
| 2026-08-07 | forge 下载驿道化 + versionInfo.libraries 并入安装器库（4031685，+161/-180）：merged 主文件下载（裸 QNAM）→驿道（downloadViaYidao 临时文件→内存，进度/100KB 校验/fallback/取消保留，loaderDlReply→loaderDlHandle）；downloadVersionLibraries 同步 QNAM→驿道（QEventLoop+15s 兑底）；forgeStepLibs 并入 versionInfo.libraries（1.7.10 0→17 任务提前并行）；tv/twitch→libraries.minecraft.net 映射；删 LibSkipTest 残留目标 |
| 2026-08-07 | Legacy 2/3 安装器库步骤评估 + QML 进度可见性（96d791c）：新增 installerLibsSkipped 信号（Legacy 3 无 install_profile.json → 步骤标跳过不闪完成）；downloadVersionLibraries 加文件级进度（installerLibsFileProgress done/total）；version_backend skipped 联动（installerLibsDone 跳过已 skipped）；DownloadQueueCard 灰点+已跳过标签 |
| 2026-08-07 | Legacy 2/3 合并统一旧版安装路线（mod_loader_installer.{h,cpp}，6639ebf，+281/-83，对齐 主流启动器/主流启动器）：新增 downloadVersionLibraries() 统一库下载（rules 判定 + natives/classifier 三级提取 + files.minecraftforge.net 兜底 + 跳过已存在）；Legacy 2 universal 提取路径对齐主流启动器实现 McLibGet(install.path)（不再网络下载 minecraftforge）；Legacy 3 补 flatten 拍平（原裸 inheritsFrom+空 libraries，merged 删原版后断裂）；forgeStep1 竞速改顺序尝试（installer 优先，防抢 universal 按 installer 校验失败）；officialMavenBaseForGroup 补老版本映射。验证：1.5.2/1.4.7/1.6.4 全 ok=1，natives×2、nightly rules 排除、flatten 后 inheritsFrom=(none) |
| 2026-08-07 | 版本选择页删减优化（VersionSelectOverlay.qml/shadow_backend.{h,cpp}，6894d9d，+20/-249）：删左侧"版本文件夹"大卡片（游戏文件夹列表/添加文件夹按钮/磁盘剩余xGB条）→ 右侧版本列表卡片占满整页；导入整合包按钮改用通用 ShadowButton 迁至顶部工具行（标题/刷新/搜索/导入/安装/排序/筛选平齐）。C++ 清理：删 Q_PROPERTY gameDirInfo/gameDirectories/diskFree/diskPercent + Q_INVOKABLE refreshGameDirInfo/setGameDir/removeGameDir + 实现（QStorageInfo 磁盘查询/QTimer 版本统计/同步6后端）+ m_gameDirInfo 成员；保留 gameDir 属性/gameDirChanged 信号（AppBackend 真实源转发，大量 QML 读 gameDir）/openGameDir。验证：编译通过 + qml.exe 布局断言 card_full=true/header_overlap=false + 窗口宽度 640-960 搜索框无挤压 |
| 2026-08-07 | 下载模块全面审计（任务A）+ 死代码删减（任务B，3a1e1f6，+6/-443）：任务A结论——所有下载已合理归属引擎（MC→盘古→夸父+山海经、整合包→女娲→精卫、Java→夸父单线程、Mod/资源包/光影→驿道 downloadWithReply，2026-08-02 回退有历史注释；版本管线内部竞速/修复/OptiFine、女娲 POST 批量解析、启动侧 manifest/更新/皮肤各有语义不宜强整合，司南仅 GET 不整合 POST）。任务B——删除 downloader.h/.cpp（Phase 2.3 遗留单文件下载器，全仓零引用） + CMakeLists 移除；mod_manager 清理夸父 fd 死分支（创建路径早已被驿道取代，wasKuafu 恒 false）+ tmpFile/tmpPath 死字段；顺带修 retryModFileDownload 悬垂迭代器（erase 后读 it->received UB，改擦除前取）。验证：Release 编译全绿（含测试项目）、FDTest 夸父 20MB ok=1 回归、全仓无 Downloader 残留 |
| 2026-08-07 | Modrinth 侧统一接入司南引擎（mod_manager.{h,cpp}，a1105ab）：用户问详情页版本列表是否走司南——调查结论 CF 侧全走（getJsonWithFallback），Modrinth 侧版本列表/依赖/下载前查询仍直接 HttpClient。新增统一入口 fetchViaSinan(url, cacheable, done, fail)（司南优先 getJson，未注入回退 HttpClient，签名同构零语义损失）；9 处调用点替换：fetchModVersions/fetchResourcepackVersions/fetchShaderVersions（详情页版本列表×3）+ getModVersions + downloadResourcepack/downloadShader 下载前查询×2 + getModDependencies 两段 + searchModrinthProjects。语义等价（非 200 从 done 改走 fail，各 fail 回调已有对应处理）且司南多一次重试；效果：详情页往返版本/依赖 300s 缓存秒开 |
| 2026-08-07 | 前置模组三项修复（ModDetailPage.qml/mod_manager.cpp/resource_backend.cpp/cf_api.cpp，8cddb02）：①required/optional 排序（Modrinth getModDependencies 与 CF resolveCfDependencies 均 stable_sort required 在前；QJsonArray 迭代器代理不支持 swap → 先转 std::vector）②依赖缓存 _depsCache（slug→deps，返回上一级秒开不再 10s+ 网络重请求；CF 分支也补 _versionCache 版本缓存）③CF overlay 前置加载不出——根因是镜像 files 端点依赖被误判恒空（实测带依赖，样本恰好全是无依赖 mod）；新增 fetchModDependencies（/mods/{id} latestFiles[0].dependencies，双源镜像优先官方降级，官方 key 解密可直连实测）；relationType 只收 2=required/3=optional 跳过 1=embedded/4=incompatible/5=include；resolveCfDependencies 串行改 4 路并发保序（上限 8→24 覆盖 Mekanism 全家桶）；依赖获取统一在 onModDetailSlugChanged（勿提前 return） |
| 2026-08-07 | 启动界面版本号自适应缩小（HomePage.qml，78c505e）：长版本号溢出展示框；RowLayout 改 anchors.fill 撑满容器（防循环）+ TextMetrics 16px 基准测量 + 按可用宽比例缩放字号（上限16px不放大/下限9px），不用省略号（用户要求缩小而非 Elide）；qml.exe 实测无 binding loop、渲染断言 PASS |
| 2026-08-07 | 超额止损数据丢失修复（file_downloader.cpp，d8098b2）：实测 QNetworkReply::abort() 清空未读缓冲（abort 后 readAll 返回 0）→ 改为 abort 前先 readAll 保存到 rangeData；止损条件改严格大于避免正常 206 误触发；本地 4 场景（MC/modpack 慢速止损、严格 range、200 全文件）SHA1 全部匹配 |
| 2026-08-07 | 超额下载浪费修复（file_downloader.cpp，7ec05bd）：日志实锤 476 次截断浪费 1261MB（178%）——根因 tryAddThread 切分后 in-flight 请求仍按旧 range 拉数据（首线程请求全文件被切分后服务器仍发完整文件）；修复为 worker downloadProgress 检测 206 已收满本线程范围即 abort 止损（对齐 主流启动器 流式 DownloadUndone=0 即断语义），数据前缀完整不丢进度不重试；超额从整个旧 range 降到约一个网络包 |
| 2026-08-06 | 导出五轮复比对：修资源包子项精确模式哈希泄漏（勾选子项时未勾选 zip 不再进 files[]/直装——collectMods 按 rpSubs 限定范围）；确认安装侧 modpack_parser 按 path 通用下载（resourcepacks 引用安全）（modpack_exporter.cpp） |
| 2026-08-06 | 导出四轮复比对再修：①子项父勾选检查（父未勾选时子项规则不再泄漏导出——BUG）②存档按修改时间倒序、子项文件夹时间倒序+黑名单过滤+texturepacks 合并（主流启动器 ReloadSubOptions）③包名空兕底版本名（主流启动器 StartExport 不拦截空名）④哈希收集扩展：mods 压缩包变体（zip/rar/disabled/old）+ resourcepacks zip 也走在线匹配（主流启动器 packs/resource 语义），files[] path 通用化（modpack_exporter.cpp/ExportModpackSection.qml） |
| 2026-08-06 | 导出三轮复比对再修 6 项：①downloads 排序方向（主流启动器 非 Modrinth 优先，原反了）②配置 PackPath 应用（读取后直接导出不弹窗）③清除配置覆盖入口（主流启动器 ResetConfigOverrides）④配置 UncheckedOptions 还原未勾选状态 ⑤打包进度 100ms 节流（大包防信号风暴）⑥取消导出静默（不弹“失败”误导）（modpack_exporter.cpp/ExportModpackSection.qml） |
| 2026-08-06 | 导出对齐 主流启动器 补齐轮 + 代码复查：①补 immersive_paintings 子项、子项黑名单（Quark Programmer Art 等 UI+收集双过滤）、Mod 子项随父勾选显隐（parent 字段）、packdata/tacz/地图/JEI/EMI/帕秋莉/截图等补 showRules 可见性（无内容隐藏）②配置读取规则覆盖模式（rawRules，手工规则整体生效）③修严重 BUG：ModpackExporter 构造后未初始化 setGameDir（m_gameDir 空→导出全路径失效）④隔离版本内容根支持（versions/{id}/game/）⑤联网失败弹窗在浮层关闭时自动继续防 worker 死等（modpack_exporter/shadow_backend/ExportModpackSection） |
| 2026-08-06 | 导出全量对齐 PCL2（差距报告 P0-P2 全部落地）：规则驱动选项表 optionDefs（19 项，含隐私项默认不勾）+ Like 通配引擎（* ? [] ! 反选、/或\结尾=目录前缀）；全局排除 *.log/*.dat_old/*.BakaCoreInfo/hmclversion.cfg/log4j2.xml；子项规则（saves 带修改时间、资源包/光影子项 id:name/id:dir:name）；exportContext 扩展（options 可见性列表/rpItems/shaderItems/javaAvailable）；配置保存/读取（saveExportConfig/loadExportConfig 三段式 txt）；exportVersion 新签名（checkedOptions+extraFiles）规则收集；分阶段进度（收集/哈希/联网/打包）；QML 动态选项渲染+二次分发警告+Java 不可用禁用；ModpackExportTest 回归通过（规则收集/隐私排除/全局排除实测验证）（modpack_exporter.{h,cpp}/ExportModpackSection.qml/test_export.cpp） |
| 2026-08-06 | 导出选项按版本实际情况动态显隐（同主流启动器 ShowRules）：ModpackExporter 新增 exportContext（版本 JSON 读 modable/hasOptiFine + 目录非空检测），QML 模组/配置/光影/资源包/存档选项按 ctx 显隐（原版不再出现模组/光影/配置）；进分区/切版本刷新 ctx；另输出 主流启动器 导出模块差距报告 docs/audit/export-gap-report-2026-08-06.md（选项体系/规则系统/配置化三层面 30+ 差距项） |
| 2026-08-06 | 修选中版本不刷新老问题：①恢复上次选中版本改挂 installedVersionsChanged 且检查 installedIds（旧逻辑挂 versionListReady 查 manifest 在线清单→manifest 有但未安装的版本被误选中，如 26.2 vs 26.2-forge-65.1.0）②新增 ensureSelectedVersionValid（选中不存在→自动切第一个已安装，无则清空）；MainWindow homePageLoader onVisibleChanged 进启动页即校验+toast ③删除版本后 installedVersionsChanged 自愈自动切可用版本 ④open*Folder/openVersionDir 全部加版本存在性检查（不再 mkpath 凭空创建文件夹），overlay 快捷按钮失败 toast（shadow_backend/version_backend/settings_backend/MainWindow/VersionSettingsOverlay） |
| 2026-08-06 | 修导出无反馈根治：ExportModpackSection 的 onCompleted connect 改为声明式 Connections（onCompleted 早于 MainWindow Loader onLoaded 的 backend 注入，连接全被跳过→C++ 导出完但 QML 零反馈）；存档加载挂起补载（onBackendChanged）；诊断日志 console.log→console.info（logger 只写 QtInfoMsg，console.log 是 QtDebugMsg 不落盘）；C++ 端导出开始/结束/参数不完整加 qCInfo 日志（ExportModpackSection.qml、modpack_exporter.cpp） |
| 2026-08-06 | 修导出界面滚动不到底 + 滚动条遮挡内容：contentHeight 从绑 implicitHeight 改绑 childrenRect（ColumnLayout.implicitHeight 会漏显式 height 子项——QML 实测 218 vs 实际 374，导致滚动范围偏小滚不到底）；滚动条显式实例化 + 内容宽减滚动条宽（overlay 不再遮右侧）；onAccepted 防御式路径转换（typeof 判断 + file:/// 剥离）加固（ExportModpackSection.qml） |①恢复上次选中版本改挂 installedVersionsChanged 且检查 installedIds（旧逻辑挂 versionListReady 查 manifest 在线清单→manifest 有但未安装的版本被误选中，如 26.2 vs 26.2-forge-65.1.0）②新增 ensureSelectedVersionValid（选中不存在→自动切第一个已安装，无则清空）；MainWindow homePageLoader onVisibleChanged 进启动页即校验+toast ③删除版本后 installedVersionsChanged 自愈自动切可用版本 ④open*Folder/openVersionDir 全部加版本存在性检查（不再 mkpath 凭空创建文件夹），overlay 快捷按钮失败 toast（shadow_backend/version_backend/settings_backend/MainWindow/VersionSettingsOverlay） |
| 2026-08-06 | 修导出无反馈根治：ExportModpackSection 的 onCompleted connect 改为声明式 Connections（onCompleted 早于 MainWindow Loader onLoaded 的 backend 注入，连接全被跳过→C++ 导出完但 QML 零反馈）；存档加载挂起补载（onBackendChanged）；诊断日志 console.log→console.info（logger 只写 QtInfoMsg，console.log 是 QtDebugMsg 不落盘）；C++ 端导出开始/结束/参数不完整加 qCInfo 日志（ExportModpackSection.qml、modpack_exporter.cpp） |
| 2026-08-06 | 修导出界面滚动不到底 + 滚动条遮挡内容：contentHeight 从绑 implicitHeight 改绑 childrenRect（ColumnLayout.implicitHeight 会漏显式 height 子项——QML 实测 218 vs 实际 374，导致滚动范围偏小滚不到底）；滚动条显式实例化 + 内容宽减滚动条宽（overlay 不再遮右侧）；onAccepted 防御式路径转换（typeof 判断 + file:/// 剥离）加固（ExportModpackSection.qml） |
|---|---|
| 2026-08-05 | 修弹窗莫名出现且关不掉：ConfirmDialog 组件靠外部 visible 控制（直接实例化即常显）→ exportLookupDialog 默认隐藏+信号驱动；opened 绑定覆盖赋值 → 改信号驱动；Qt.StandardPaths → Qt.labs.platform（2279efc） |
| 2026-08-05 | 修版本设置页全空白：ExportModpackSection.qml 未登记进 CMakeLists QML 资源列表（qt6_add_resources 显式列表非 glob）→ qrc 缺失 → not a type（291e0b3） |
| 2026-08-05 | 导出全面自查修复：ScrollView 防裁切、ConfirmDialog 移顶层全屏、存档版本切换重载、格式切换保留路径、批量查询分块 500（484ce7d） |
| 2026-08-05 | 导出对齐 主流启动器 补全：game=minecraft 规范、versionId/summary、IncludeJava、hostedAssetsOnly、查询失败 ConfirmDialog 弹窗、垃圾目录排除；修 ConfirmDialog onAccept/onClosed 覆盖 bug + zip 泄漏（c62cb43） |
| 2026-08-05 | 整合包导出完全对齐 主流启动器：双平台在线来源查询（Modrinth sha1 + CF MurmurHash2 指纹）、hosted→files[]/未托管→overrides 直装、CF zip 格式、ModrinthUploadMode、存档子项（modpack_exporter.{h,cpp} + ExportModpackSection.qml） |
| 2026-08-05 | 导出整合包改为版本设置独立分区 Section 7（ExportModpackSection.qml 取代浮层）：侧边栏入口、通用组件、worker 线程零阻塞 |
| 2026-08-05 | 整合包导出 .mrpack（P0 收官）：ModpackExporter + ZipArchive 写入 + ExportModpackOverlay 浮层 + 工具分区入口 + ModpackExportTest 回归测试 |
| 2026-08-05 | 缓存命中失效修复：山海经 appendTasks 加预检查（assets 不再全量重下 578MB）、夸父后台预检加 fallback 复制（merged/跨版本命中）、AssetDownloader 增加 m_minecraftDir（asset_downloader.{h,cpp}、version_downloader.cpp） |
| 2026-08-05 | 缓存命中日志补齐：notifyCacheHit 节流明细 + 夸父完成汇总 + 盘古预检汇总（file_downloader.{h,cpp}、version_downloader.cpp） |
| 2026-08-05 | 资源索引双源竞速修复（镜像跟随302）+ 官方源分片下载（探测Range→4片并行），assets 启动提速（version_downloader.cpp） |
| 2026-08-05 | 安装完成态残留修复：纯 MC 完成后 installing 永久 true → 整合包导入被拒；重复导入被 fail() phase 守卫静默吞掉（version_backend.cpp + modpack_install_task.cpp） |
| 2026-08-05 | 山海经防卡死：degraded 30s 自动恢复 + 全源不可用 5s 冷却重试 + 持续 120s 失败收尾（asset_downloader.{h,cpp}） |
| 2026-08-05 | 卡片总进度统一为步骤管线加权：删 merged 双写 smoothProgress（公式 A/B）、merged 卡片改 totalProgress、client.jar 归属澄清（cat1）、JSON 步骤尽早完成、syncPrimaryProgress 真两段式（version_backend.cpp） |
| 2026-08-05 | 尾程加速：慢速分片看门狗（<128KB/s 持续 2s → 剩余范围一分为二并跑 + 中止老连接换新）+ 分片截断字节二次扣减修正（file_downloader.{h,cpp}） |
| 2026-08-04 | 新增数据包（Data Pack）Tab：双源池子（Modrinth project_type:datapack + CF classId=6945）、DataPackDetailPage、FilterCard datapack 行（来源/类别/排序）；随后全 Tab 加来源筛选（sourceFilter → 五池 source 参数）；CF sortField 参数化；数据包池权重 2.5→1.0。 |
| 2026-08-04 | 修 StatsPage 黑屏：Popup 残留 ToolTip 专属属性（text/delay/timeout）导致 QML 报错 delegate 创建失败；改自定义 tipText 属性 + parent 挂 root + mapToItem 坐标换算。 |
| 2026-08-03 | 首次建档（全量归档 src/ 与 qml/ 全部文件）。 |
| 2026-08-03 | VersionSettingsOverlay 概览快捷入口分类重做（ShadowButton 统一 + 文件夹/日志/其他分组 + 修 stub isModdedVersion 恒隐藏 bug）；StatsPage tooltip 自定义 Popup 圆角框。 |
| 2026-08-03 | 快捷入口 Mod 按钮 visible 改 sidebar 同款内联白名单（readonly property 中转不生效，内联可靠）；删光影包/config 按钮；移除临时 DIA 日志。 |

> 之后每次代码变更后在此追加一行：日期 + 变更文件 + 一句话说明。
