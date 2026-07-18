# C++ Shadow Launcher — Project Status

> Updated: 2026-06-17 23:20 HKT

## Phase 2 Progress

### ✅ Complete (7 modules, ~2500 lines)

| Module | File | Lines | Feature | Build |
|--------|------|-------|---------|-------|
| Logger | `logger.cpp` | 104 | Multi-level logging (info/warn/error) | ✅ |
| VersionManager | `version_manager.cpp` | 255 | Manifest fetch, release/snapshot/old filter, JSON parse | ✅ |
| HttpClient | `http_client.cpp` | 258 | Singleton QNetworkAccessManager, proxy, DownloadQueue | ✅ |
| Downloader | `downloader.cpp` | 255 | Single-file download, SHA1, retry, resume | ✅ |
| Launcher | `launcher.cpp` | 218 | QProcess Minecraft launch, JVM args, taskkill | ✅ |
| ModManager | `mod_manager.cpp` | 465 | Modrinth API search/versions/download, popular mods | ✅ |
| ParallelDownloader | `parallel_downloader.cpp` | 525 | Multi-threaded concurrent download, mirror racing, cancel | ✅ |

### 🟡 In Progress

| Module | File | Status |
|--------|------|--------|
| VersionDownloader | `version_downloader.cpp` | Sub-agent running — Minecraft version install pipeline |

### ❌ Not Started

| Module | Priority | Description |
|--------|----------|-------------|
| Backend bridges (10) | 🟡 | QML ↔ C++ bindings (currently 3-line stubs) |
| Full QML UI | 🟢 | Replace Phase1Main.qml with real UI |

### Infrastructure

- `src/utils/types.h` — Shared structs: McVersion, Account, DownloadTask, ModrinthResource, AppSettings
- `src/third_party/nlohmann/` — JSON library (header-only)
- CMakeLists.txt — MSVC 2022 + Qt 6.5.3 + C++17

### Build

```
VS 2022 Build Tools + Qt 6.5.3 msvc2019_64
cmake -G "Visual Studio 17 2022" -A x64
Output: build/Release/ShadowLauncher.exe
Last build: 2026-06-17 23:15 — SUCCESS
```
