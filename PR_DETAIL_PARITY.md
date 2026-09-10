# 详情页对齐主流启动器：版本过多优化 + 统一跳转/复制按钮

## 背景 / 动机

- 详情页（Mod / 资源包 / 光影 / 整合包 / 数据包）此前版本分组是**同步计算**：数据回传的同一帧内全量循环 + 分组排序（Mod 最多 4300+ 项），主线程卡死、页面打开明显掉帧。
- 五个详情页跳转/复制入口**样式与逻辑各自重复**，且"转 MC百科 / 复制名称 / 复制链接"缺失或散落多处，不统一。
- 目标：对齐主流启动器的详情页体验——版本多也能秒开、跳转/复制按钮一致。

## 改动内容

### A. 版本分组异步化 + 懒渲染（5 页统一）

- ModDetailPage（模板，此前已迁移）继续作为参考实现；本次同步迁移：
  - ResourcePackDetailPage：`_rebuildRpGrouped()`
  - ShaderDetailPage：`_rebuildGrouped()`
  - ModpackDetailPage：`_rebuildPackGrouped()`
  - DataPackDetailPage：`_rebuildGrouped()`
- 统一方案：`grouped: _groupedCache` 缓存 + `Qt.callLater` 延迟一帧批量算一次 + `_versionListEnter` 入场淡入。
- 版本列表懒渲染：每组默认只渲染前 **30** 个版本，超出显示"还有 X 个版本"按钮，点击追加 **50** 个（错峰入场 `index*80+100` 保留）。

### B. 统一跳转/复制胶囊条

- 新增 `qml/DetailLinkBar.qml`（Flow 布局，纯文本、无图标/无预览图）：
  - `links`：外部跳转胶囊（蓝边），点击 `Qt.openUrlExternally`
  - `copyItems`：复制胶囊（棕边），点击 `backend.copyToClipboard` + `toastManager.show`
- 五页注入 DetailInfoCard 底部，按钮矩阵（按上游资源平台可达性）：
  - Mod：Modrinth / CurseForge / MC百科 / 复制名称 / 复制链接
  - 光影：Modrinth / CurseForge / MC百科 / 复制名称 / 复制链接
  - 资源包：Modrinth / CurseForge / MC百科 / 复制名称 / 复制链接
  - 整合包：Modrinth / CurseForge / 复制名称 / 复制链接
  - 数据包：Modrinth / MC百科 / 复制名称 / 复制链接

### C. C++ 侧跳转解析

- `ResourceBackend::resolveProjectLinks(title, slug, kind)`：kind→平台路径映射（含 CF 数字项目 id 判定）。
- `ResourceBackend::loadMcmodMap()`：qrc 加载 `qml/mcmod_map.json`，建 en/cf/mr 三向 class-id 索引。
- `ResourceBackend::resolveMcmodUrl()`：mr slug → 英文名 →（保守）包含匹配 → 兜底**百科搜索跳转**（不确定映射绝不臆造）。
- ShadowBackend 转发 `resolveProjectLinks`（QML 可见）。

### D. MC百科映射工具（自研，满足"不搬运百科数据"）

- 新增 `tools/gen_mcmod_map.py`：从百科公开 sitemap 取 class id，抓详情页只提取**极简映射字段**（classId / 英文名 / CurseForge slug / Modrinth slug），不搬运介绍正文、物品、教程等内容。
- 产物 `qml/mcmod_map.json`（进 qrc），断点缓存 `tools/_mcmod_map_resume.jsonl`（已 gitignore）。
- 无映射时统一走百科搜索跳转兜底。

## 验证

- **Release 编译通过**（`cmake --build build --config Release --target ShadowLauncher`）。
- 硬规则自查：UI 零 emoji（图标统一 Lucide SVG）；RowLayout 图标 `Layout.preferredWidth/Height`；代码/提交信息不含"PCL2"。

## 影响面

- 新增文件：`qml/DetailLinkBar.qml`、`qml/mcmod_map.json`、`tools/gen_mcmod_map.py`
- 修改：五个详情页 QML、`resource_backend.{h,cpp}`、`shadow_backend.{h,cpp}`、`CMakeLists.txt`、`CODE_INDEX.md`、`.gitignore`
