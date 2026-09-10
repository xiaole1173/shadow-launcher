# 启动器 QML 颜色审计报告

- 唯一硬编码颜色: **374** 种 / 共 **888** 处
- 精确匹配现有令牌: **31** 种 / 82 处
- 近似匹配(可合并): **57** 种 / 145 处
- 偏离较远(需人工判断): **286** 种 / 661 处

## 精确匹配 TOP（直接替换为令牌）

- `#4a8fe7` × 17 → `StyleTokens.accentVivid` (4 个文件)
- `#c05050` × 10 → `StyleTokens.textDanger` (6 个文件)
- `#6080e8` × 6 → `StyleTokens.accentLight` (4 个文件)
- `#ffffff` × 5 → `StyleTokens.textInverse` (4 个文件)
- `#141820` × 5 → `StyleTokens.surfaceOverlay` (3 个文件)
- `#1a1d24` × 5 → `StyleTokens.surfaceLight` (4 个文件)
- `#5068c8` × 3 → `StyleTokens.accentHover` (2 个文件)
- `#1e2230` × 3 → `StyleTokens.bgElevated` (2 个文件)
- `#1a3a5c` × 2 → `StyleTokens.infoBg` (2 个文件)
- `#5098e8` × 2 → `StyleTokens.info` (2 个文件)
- `#1a3a1a` × 2 → `StyleTokens.successBg` (2 个文件)
- `#ef4444` × 2 → `StyleTokens.error` (2 个文件)
- `#2a3040` × 2 → `StyleTokens.bgHover` (1 个文件)
- `#0c0f16` × 1 → `StyleTokens.bgPrimary` (1 个文件)
- `#11141c` × 1 → `StyleTokens.bgSecondary` (1 个文件)
- `#1a1f2e` × 1 → `StyleTokens.bgCard` (1 个文件)
- `#1a1f2a` × 1 → `StyleTokens.bgInput` (1 个文件)
- `#e8ecf8` × 1 → `StyleTokens.textPrimary` (1 个文件)
- `#e4e8f2` × 1 → `StyleTokens.textSecondary` (1 个文件)
- `#a8b0c0` × 1 → `StyleTokens.textTertiary` (1 个文件)

## 近似匹配 TOP（建议合并）

- `#2a2f3a` × 20 → `StyleTokens.bgHover` `#2a3040` (距离 10.6, 6 个文件)
- `#0d1018` × 11 → `StyleTokens.bgPrimary` `#0c0f16` (距离 4.2, 4 个文件)
- `#7888a8` × 9 → `StyleTokens.textSubtle` `#8088a0` (距离 17.9, 4 个文件)
- `#181f30` × 8 → `StyleTokens.bgCard` `#1a1f2e` (距离 4.5, 3 个文件)
- `#0f111a` × 8 → `StyleTokens.bgSecondary` `#11141c` (距离 7.5, 3 个文件)
- `#161a24` × 6 → `StyleTokens.surfaceLight` `#1a1d24` (距离 8.2, 2 个文件)
- `#0e1018` × 6 → `StyleTokens.bgPrimary` `#0c0f16` (距离 4.9, 5 个文件)
- `#1a2440` × 4 → `StyleTokens.accentSubtle` `#1a2848` (距离 16.0, 4 个文件)
- `#121620` × 4 → `StyleTokens.surfaceOverlay` `#141820` (距离 4.9, 3 个文件)
- `#151922` × 4 → `StyleTokens.surfaceOverlay` `#141820` (距离 4.2, 4 个文件)
- `#161a26` × 3 → `StyleTokens.surfaceLight` `#1a1d24` (距离 8.9, 3 个文件)
- `#1a202c` × 3 → `StyleTokens.bgCard` `#1a1f2e` (距离 4.0, 1 个文件)
- `#0e1118` × 3 → `StyleTokens.bgPrimary` `#0c0f16` (距离 6.0, 1 个文件)
- `#1e2840` × 3 → `StyleTokens.accentSubtle` `#1a2848` (距离 15.0, 1 个文件)
- `#151c30` × 3 → `StyleTokens.bgCard` `#1a1f2e` (距离 9.9, 1 个文件)
- `#141a24` × 2 → `StyleTokens.surfaceOverlay` `#141820` (距离 8.0, 2 个文件)
- `#151a24` × 2 → `StyleTokens.surfaceOverlay` `#141820` (距离 8.1, 1 个文件)
- `#3a1518` × 2 → `StyleTokens.errorBg` `#3a1a1a` (距离 10.6, 1 个文件)
- `#3a1818` × 2 → `StyleTokens.errorBg` `#3a1a1a` (距离 5.3, 2 个文件)
- `#505868` × 2 → `StyleTokens.textMuted` `#505468` (距离 8.0, 1 个文件)
- `#808aa0` × 2 → `StyleTokens.textSubtle` `#8088a0` (距离 4.0, 1 个文件)
- `#191e2a` × 2 → `StyleTokens.bgInput` `#1a1f2a` (距离 2.4, 1 个文件)
- `#1e2430` × 2 → `StyleTokens.bgElevated` `#1e2230` (距离 4.0, 1 个文件)
- `#2a2e42` × 1 → `StyleTokens.bgHover` `#2a3040` (距离 5.3, 1 个文件)
- `#2a2a3a` × 1 → `StyleTokens.bgHover` `#2a3040` (距离 15.9, 1 个文件)
- `#192650` × 1 → `StyleTokens.accentSubtle` `#1a2848` (距离 14.5, 1 个文件)
- `#0d1117` × 1 → `StyleTokens.bgPrimary` `#0c0f16` (距离 4.6, 1 个文件)
- `#1a2030` × 1 → `StyleTokens.bgCard` `#1a1f2e` (距离 4.0, 1 个文件)
- `#1a1e28` × 1 → `StyleTokens.bgInput` `#1a1f2a` (距离 4.0, 1 个文件)
- `#12151c` × 1 → `StyleTokens.bgSecondary` `#11141c` (距离 2.4, 1 个文件)

## 偏离较远 TOP（需人工判断）

- `#b4bac6` × 20 → 最近: `StyleTokens.textTertiary` `#a8b0c0` (距离 28.2) 文件: SettingsGeneralPage.qml, SettingsJavaPage.qml, ShadowDropdown.qml, VersionSelectPage.qml, VersionSettingsPage.qml
- `#7e8596` × 19 → 最近: `StyleTokens.textSubtle` `#8088a0` (距离 18.5) 文件: HomePage.qml, SettingsGeneralPage.qml, SettingsJavaPage.qml, VersionSelectOverlay.qml, VersionSelectPage.qml
- `#8890a0` × 15 → 最近: `StyleTokens.textSubtle` `#8088a0` (距离 19.6) 文件: InstallConfigOverlay.qml, ModDetailPage.qml, SettingsMemoryPage.qml, SettingsMemorySection.qml, SettingsPage.qml
- `#d0d4e0` × 13 → 最近: `StyleTokens.textSecondary` `#e4e8f2` (距离 58.1) 文件: DetailVersionCard.qml, DownloadCard.qml, DownloadPage.qml, JavaPage.qml, LaunchOverlay.qml
- `#44ff44` × 12 → 最近: `StyleTokens.success` `#4bc870` (距离 134.2) 文件: JavaPage.qml
- `#a0a8c0` × 11 → 最近: `StyleTokens.textTertiary` `#a8b0c0` (距离 19.6) 文件: DataPackDetailPage.qml, InstallPage.qml, ModDetailPage.qml, ResourcePackDetailPage.qml, SettingsExperimentalPage.qml
- `#606478` × 10 → 最近: `StyleTokens.textMuted` `#505468` (距离 48.0) 文件: DataPackDetailPage.qml, DetailVersionCard.qml, DownloadPage.qml, LaunchOverlay.qml, LoadStatus.qml
- `#687080` × 10 → 最近: `StyleTokens.textMuted` `#505468` (距离 77.6) 文件: DownloadCard.qml, FilterCard.qml, JavaPage.qml, ModLoaderCard.qml
- `#3a50b0` × 9 → 最近: `StyleTokens.accentHover` `#5068c8` (距离 70.7) 文件: DataPackDetailPage.qml, InstallPage.qml, ModDetailPage.qml, ModLoaderCard.qml, ModpackDetailPage.qml
- `#707888` × 9 → 最近: `StyleTokens.textSubtle` `#8088a0` (距离 57.1) 文件: SettingsExperimentalPage.qml, SettingsPage.qml, StatsPage.qml
- `#000000` × 7 → 最近: `StyleTokens.shadowColor` `#0a0a12` (距离 39.6) 文件: BackgroundCropOverlay.qml, ConfirmDialog.qml, InstallPage.qml, MainWindow.qml, VersionSettingsOverlay.qml
- `#252a38` × 7 → 最近: `StyleTokens.bgHover` `#2a3040` (距离 19.6) 文件: BackgroundCropOverlay.qml, FilterCard.qml, LoadingSpinner.qml, PaginationFooter.qml, SettingsExperimentalPage.qml
- `#788090` × 7 → 最近: `StyleTokens.textSubtle` `#8088a0` (距离 33.9) 文件: DetailVersionCard.qml, DownloadCard.qml, DownloadCardTagRow.qml, DownloadQueueCard.qml, ShadowDropdown.qml
- `#b8c0d0` × 7 → 最近: `StyleTokens.textTertiary` `#a8b0c0` (距离 48.0) 文件: InstallConfigOverlay.qml, MainWindow.qml, SettingsPage.qml
- `#884444` × 7 → 最近: `StyleTokens.textDanger` `#c05050` (距离 85.3) 文件: JavaPage.qml
- `#5a6173` × 7 → 最近: `StyleTokens.textMuted` `#505468` (距离 35.2) 文件: SettingsGeneralPage.qml, SettingsJavaPage.qml, SettingsMemoryPage.qml, VersionMemorySection.qml, VersionSettingsPage.qml
- `#6090d0` × 6 → 最近: `StyleTokens.info` `#5098e8` (距离 50.0) 文件: AgreementOverlay.qml, LaunchOverlay.qml
- `#3a5ed0` × 6 → 最近: `StyleTokens.accentHover` `#5068c8` (距离 39.5) 文件: DataPackDetailPage.qml, DetailLinkBar.qml, InstallPage.qml, ModDetailPage.qml, SettingsExperimentalPage.qml
- `#4caf50` × 6 → 最近: `StyleTokens.success` `#4bc870` (距离 74.7) 文件: DebugPanel.qml, DebugWindow.qml
- `#f44336` × 6 → 最近: `StyleTokens.error` `#ef4444` (距离 25.3) 文件: DebugPanel.qml, DebugWindow.qml
- `#9094a8` × 6 → 最近: `StyleTokens.textSubtle` `#8088a0` (距离 35.8) 文件: DownloadPage.qml, FilterCard.qml, LabeledDropdown.qml, LaunchOverlay.qml, ShadowDropdown.qml
- `#80000000` × 6 → 最近: `StyleTokens.shadowColor` `#0a0a12` (距离 103.1) 文件: GenericPopup.qml, HomePage.qml, ModpackDetailPage.qml, ModpackImportOverlay.qml, ProfileSelectPopup.qml
- `#4a5ec8` × 6 → 最近: `StyleTokens.accentHover` `#5068c8` (距离 21.7) 文件: HomePage.qml, SettingsJavaPage.qml, VersionSelectOverlay.qml, VersionSettingsOverlay.qml
- `#c0c8d8` × 6 → 最近: `StyleTokens.textTertiary` `#a8b0c0` (距离 72.0) 文件: InstallPage.qml, PaginationFooter.qml, SettingsMemoryPage.qml, SettingsMemorySection.qml, VersionMemorySection.qml
- `#f59e0b` × 6 → 最近: `StyleTokens.warning` `#ff9800` (距离 26.6) 文件: MultiplayerPlayerCard.qml, MultiplayerRoomCodeCard.qml, MultiplayerStateIndicator.qml, SettingsGeneralPage.qml, VersionSelectPage.qml
- `#2a4590` × 6 → 最近: `StyleTokens.textMuted` `#505468` (距离 92.7) 文件: VersionSettingsOverlay.qml
- `#7880a0` × 5 → 最近: `StyleTokens.textSubtle` `#8088a0` (距离 19.6) 文件: AgreementOverlay.qml, VersionSettingsOverlay.qml
- `#2a3a68` × 5 → 最近: `StyleTokens.infoBg` `#1a3a5c` (距离 30.7) 文件: BackgroundCropOverlay.qml, DataPackDetailPage.qml, ModpackDetailPage.qml, ResourcePackDetailPage.qml, ShaderDetailPage.qml
- `#5b8def` × 5 → 最近: `StyleTokens.accentVivid` `#4a8fe7` (距离 28.0) 文件: DataPackDetailPage.qml, MinecraftHead2D.qml, ModDetailPage.qml, ResourcePackDetailPage.qml, ShaderDetailPage.qml
- `#9ca0b4` × 5 → 最近: `StyleTokens.textTertiary` `#a8b0c0` (距离 41.8) 文件: ExportModpackSection.qml, VersionSelectOverlay.qml, VersionSettingsOverlay.qml