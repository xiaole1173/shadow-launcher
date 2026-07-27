/****************************************************************************
** Meta object code from reading C++ file 'shadow_backend.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/backend/shadow_backend.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'shadow_backend.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.8.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN14ShadowLauncher13ShadowBackendE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN14ShadowLauncher13ShadowBackendE = QtMocHelpers::stringData(
    "ShadowLauncher::ShadowBackend",
    "betaVerified",
    "",
    "betaKeyInvalid",
    "reason",
    "betaStatusChanged",
    "updateCheckingChanged",
    "updateStateChanged",
    "toastMessage",
    "message",
    "updateDownloadProgress",
    "received",
    "total",
    "updateChangelogAvailable",
    "version",
    "notes",
    "accountChanged",
    "microsoftLoginProgress",
    "step",
    "detail",
    "microsoftLoginSuccess",
    "username",
    "uuid",
    "microsoftLoginFailed",
    "error",
    "skinReady",
    "offlineSkinReady",
    "offlineHistoryChanged",
    "javaPathChanged",
    "javaReadyChanged",
    "memorySettingsChanged",
    "jvmArgsChanged",
    "gameArgsChanged",
    "highPerfGpuChanged",
    "versionLaunchSettingsChanged",
    "versionId",
    "launchBlocked",
    "generalSettingsChanged",
    "downloadSettingsChanged",
    "isolationChanged",
    "embeddedLoginChanged",
    "versionListReady",
    "versionDetailsReady",
    "scanningChanged",
    "installedVersionsChanged",
    "activeVersionNamesChanged",
    "selectedVersionChanged",
    "selectedVersionClearedAfterDelete",
    "currentVersionSummaryChanged",
    "installStateChanged",
    "installPhaseChanged",
    "installFinished",
    "success",
    "installComplete",
    "installName",
    "launchProgressChanged",
    "progress",
    "status",
    "launchStateChanged",
    "minecraftStarted",
    "minecraftStopped",
    "crashDetected",
    "QVariantMap",
    "report",
    "isRunningChanged",
    "runningCountChanged",
    "resourceDownloadStateChanged",
    "resourcepackSearchCompleted",
    "QVariantList",
    "results",
    "totalHits",
    "resourcepackSearchFailed",
    "resourcepackDownloadFinished",
    "slug",
    "filePath",
    "resourcepackVersionsLoaded",
    "slugToVersions",
    "resourcepackVersionsPartial",
    "versions",
    "details",
    "resourcepackVersionsProgress",
    "done",
    "modVersionsLoaded",
    "modVersionsPartial",
    "modVersionsProgress",
    "fabricApiVersionsReady",
    "modFileDownloadStarted",
    "downloadId",
    "fileName",
    "fileSize",
    "displayName",
    "modFileDownloadProgress",
    "modFileDownloadFinished",
    "modFileDownloadFailed",
    "errorDetail",
    "shaderVersionsLoaded",
    "shaderVersionsPartial",
    "shaderVersionsProgress",
    "resourceDownloadProgress",
    "completed",
    "resourceDownloadDone",
    "verifyRunningChanged",
    "repairRunningChanged",
    "verifyCheckedChanged",
    "verifyTotalChanged",
    "verifyResultTextChanged",
    "downloadQueueChanged",
    "downloadQueueFull",
    "searchResultsReady",
    "modSearchResultsReady",
    "shaderSearchResultsReady",
    "gameDirChanged",
    "themeChanged",
    "agreementAcceptedChanged",
    "loginModeChanged",
    "customBgChanged",
    "logMessage",
    "msg",
    "skinChanged",
    "wardrobeBusyChanged",
    "wardrobeError",
    "forgeVersionsReady",
    "fabricVersionsReady",
    "neoforgeVersionsReady",
    "optifineVersionsReady",
    "statsChanged",
    "statsLoadingChanged",
    "iconCached",
    "webpUrl",
    "pngPath",
    "navigateToRequested",
    "pageIndex",
    "subTab",
    "openRpDetailRequested",
    "openModDetailRequested",
    "openShaderDetailRequested",
    "setRpShowPreReleases",
    "show",
    "openRpVersionMenu",
    "expandRpDetailGroup",
    "major",
    "selectRpDetailSubVer",
    "verifyStarted",
    "verifyProgress",
    "checked",
    "verifyFinished",
    "allPassed",
    "verifyFailedFiles",
    "failedFiles",
    "launchCheckProgress",
    "launchCheckFailed",
    "phase",
    "launchCheckMissingFiles",
    "files",
    "launchCheckWarning",
    "warning",
    "setEmbeddedLoginEnabled",
    "v",
    "setCustomBgPath",
    "path",
    "setSidebarOpacity",
    "setContentOpacity",
    "setCropX",
    "setCropY",
    "updateCrop",
    "x",
    "y",
    "pickBackgroundImage",
    "switchLanguage",
    "index",
    "readLanguageFile",
    "listMods",
    "listResourcePacks",
    "listSaves",
    "offlineLogin",
    "updateOfflineSkin",
    "removeOfflineUsername",
    "microsoftLogin",
    "cancelMicrosoftLogin",
    "logout",
    "scanJavaInstallations",
    "autoSelectJava",
    "detectJava",
    "browseJava",
    "selectJavaByIndex",
    "getMemoryStatus",
    "setMinMemory",
    "mb",
    "setMaxMemory",
    "setIsolationEnabled",
    "enabled",
    "getVersionGameDir",
    "migrateVersionToIsolated",
    "openGameDir",
    "openLatestLog",
    "openLogsFolder",
    "openLauncherLogsFolder",
    "openCrashLog",
    "openSavesFolder",
    "openScreenshotsFolder",
    "openModsFolder",
    "openResourcePacksFolder",
    "openShaderPacksFolder",
    "openVersionDir",
    "deleteVersion",
    "refreshVersionList",
    "refreshInstalled",
    "refreshInstalledList",
    "refreshVersionDetails",
    "refreshGameDirInfo",
    "installVersion",
    "cancelInstall",
    "cancelVersionInstall",
    "dismissCard",
    "installId",
    "launch",
    "online",
    "cancelLaunch",
    "killGameProcess",
    "killMinecraft",
    "killGameByPid",
    "pid",
    "runningGames",
    "getPopularMods",
    "loader",
    "getShaderList",
    "searchMods",
    "query",
    "searchModsEx",
    "category",
    "gameVersion",
    "environment",
    "license",
    "offset",
    "limit",
    "getModCategories",
    "searchShadersEx",
    "gameVersions",
    "categories",
    "performance",
    "downloadMod",
    "minecraftDir",
    "downloadShader",
    "searchResourcepacks",
    "downloadResourcepack",
    "fetchResourcepackVersions",
    "slugs",
    "fetchModVersions",
    "fetchShaderVersions",
    "downloadModFile",
    "url",
    "savePath",
    "expectedSize",
    "sha1",
    "receivedOffset",
    "resumeId",
    "cancelModFileDownload",
    "pauseModFileDownload",
    "resumeModFileDownload",
    "retryModFileDownload",
    "browseSkin",
    "uploadSkin",
    "skinPath",
    "modelType",
    "saveWardrobeSettings",
    "capeId",
    "saveSkinToFile",
    "availableCapes",
    "loginType",
    "cacheIconAsync",
    "cachedIconPath",
    "queryForgeVersions",
    "mcVersion",
    "queryFabricVersions",
    "queryNeoForgeVersions",
    "queryOptifineVersions",
    "queryFabricApiVersions",
    "cancelModLoaderQueries",
    "cacheForgeInstallerSha1",
    "mcVer",
    "forgeVer",
    "installFabricApi",
    "installModLoader",
    "loaderType",
    "loaderVersion",
    "fabricApiVersion",
    "fabricApiUrl",
    "fabricApiSavePath",
    "forgeInstallerSha1",
    "installOptifine",
    "optifineVersion",
    "forgeVersion",
    "bmclType",
    "bmclPatch",
    "setSelectedVersion",
    "setTheme",
    "theme",
    "checkAll",
    "setGameDir",
    "dir",
    "getAutoMemory",
    "setAutoMemoryEnabled",
    "versionMemoryMode",
    "setVersionMemoryMode",
    "mode",
    "versionMemoryManualMB",
    "setVersionMemoryManualMB",
    "resolvedMemoryMB",
    "versionJavaMode",
    "setVersionJavaMode",
    "versionJvmArgsMode",
    "setVersionJvmArgsMode",
    "versionJvmArgs",
    "setVersionJvmArgs",
    "args",
    "resolvedJvmArgs",
    "versionGameArgsMode",
    "setVersionGameArgsMode",
    "versionGameArgs",
    "setVersionGameArgs",
    "resolvedGameArgs",
    "versionHighPerfGpuMode",
    "setVersionHighPerfGpuMode",
    "versionHighPerfGpu",
    "setVersionHighPerfGpu",
    "resolvedHighPerfGpu",
    "setJvmArgs",
    "setGameArgs",
    "setHighPerfGpu",
    "copyToClipboard",
    "text",
    "isModdedVersion",
    "openJavaFileDialog",
    "pickJava",
    "checkFileChanges",
    "deleteMod",
    "filename",
    "deleteResourcePack",
    "importMod",
    "deleteSave",
    "saveName",
    "migrateVersion",
    "openConfigFolder",
    "removeGameDir",
    "cancelQueuedDownload",
    "verifyVersion",
    "cleanCorruptVersion",
    "renameVersion",
    "oldId",
    "newId",
    "cloneVersion",
    "sourceId",
    "copyVersionPath",
    "repairVersion",
    "cancelVerify",
    "openVerifyReport",
    "submitBetaKey",
    "key",
    "checkForUpdate",
    "settings",
    "SettingsBackend*",
    "diagAutoLangComboIdx",
    "setAutoLangModeFromCombo",
    "idx",
    "logUiMsg",
    "statsBackend",
    "StatsBackend*",
    "javaBackend",
    "JavaBackend*",
    "userDataBackend",
    "UserDataBackend*",
    "refreshGameStats",
    "resolveIconUrl",
    "cacheIconBatchAsync",
    "urls",
    "iconCachedPath",
    "resolveShaderIconUrl",
    "cacheShaderIconBatchAsync",
    "resolveRpIconUrl",
    "cacheRpIconBatchAsync",
    "account",
    "yggdrasil",
    "offlineUsername",
    "isOnline",
    "accountUuid",
    "offlineUuid",
    "offlineSkinPath",
    "offlineUsernames",
    "lastLoginMode",
    "javaPath",
    "javaVersion",
    "javaMajor",
    "javaInstalled",
    "javaReady",
    "minMemoryMb",
    "maxMemoryMb",
    "isolationEnabled",
    "embeddedLoginEnabled",
    "availableJavaList",
    "selectedVersion",
    "versionIds",
    "versionList",
    "releaseVersions",
    "snapshotVersions",
    "oldVersions",
    "aprilFoolVersions",
    "installedVersions",
    "activeVersionNames",
    "installing",
    "installVersionId",
    "installPhase",
    "launching",
    "lastCrash",
    "launchStatus",
    "isRunning",
    "runningCount",
    "launchVersion",
    "launchUsername",
    "downloading",
    "resourceDownloadTotal",
    "resourceDownloadSpeed",
    "resourceDownloadFile",
    "modManager",
    "multiplayer",
    "gameDir",
    "dataDir",
    "appVersion",
    "devMode",
    "modpackImporter",
    "updateChecking",
    "updateState",
    "agreementAccepted",
    "betaAgreementHtml",
    "privacyAgreementHtml",
    "termsAgreementHtml",
    "markAgreed",
    "customBgPath",
    "sidebarOpacity",
    "contentOpacity",
    "cropX",
    "cropY",
    "versionDetails",
    "isScanningVersions",
    "currentVersionSummary",
    "downloadQueue",
    "activeDownloads",
    "gameDirInfo",
    "gameDirectories",
    "diskFree",
    "diskPercent",
    "autoMemoryEnabled",
    "systemMemoryInfo",
    "selectedSkinPath",
    "wardrobeBusy",
    "fileDownloadSource",
    "listDownloadSource",
    "maxDownloadThreads",
    "downloadSpeedLimitMB",
    "jvmArgs",
    "gameArgs",
    "highPerfGpu",
    "verifyRunning",
    "repairRunning",
    "verifyChecked",
    "verifyTotal",
    "verifyResultOk",
    "verifyResultText",
    "installCardsModel",
    "betaStatus",
    "totalGameHours",
    "versionGameStats",
    "statsLoading",
    "statsEmpty"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN14ShadowLauncher13ShadowBackendE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
     303,   14, // methods
      99, 2807, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
     108,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0, 1832,    2, 0x06,  100 /* Public */,
       3,    1, 1833,    2, 0x06,  101 /* Public */,
       5,    0, 1836,    2, 0x06,  103 /* Public */,
       6,    0, 1837,    2, 0x06,  104 /* Public */,
       7,    0, 1838,    2, 0x06,  105 /* Public */,
       8,    1, 1839,    2, 0x06,  106 /* Public */,
      10,    2, 1842,    2, 0x06,  108 /* Public */,
      13,    2, 1847,    2, 0x06,  111 /* Public */,
      16,    0, 1852,    2, 0x06,  114 /* Public */,
      17,    2, 1853,    2, 0x06,  115 /* Public */,
      20,    2, 1858,    2, 0x06,  118 /* Public */,
      23,    1, 1863,    2, 0x06,  121 /* Public */,
      25,    0, 1866,    2, 0x06,  123 /* Public */,
      26,    0, 1867,    2, 0x06,  124 /* Public */,
      27,    0, 1868,    2, 0x06,  125 /* Public */,
      28,    0, 1869,    2, 0x06,  126 /* Public */,
      29,    0, 1870,    2, 0x06,  127 /* Public */,
      30,    0, 1871,    2, 0x06,  128 /* Public */,
      31,    0, 1872,    2, 0x06,  129 /* Public */,
      32,    0, 1873,    2, 0x06,  130 /* Public */,
      33,    0, 1874,    2, 0x06,  131 /* Public */,
      34,    1, 1875,    2, 0x06,  132 /* Public */,
      36,    1, 1878,    2, 0x06,  134 /* Public */,
      37,    0, 1881,    2, 0x06,  136 /* Public */,
      38,    0, 1882,    2, 0x06,  137 /* Public */,
      39,    0, 1883,    2, 0x06,  138 /* Public */,
      40,    0, 1884,    2, 0x06,  139 /* Public */,
      41,    0, 1885,    2, 0x06,  140 /* Public */,
      42,    0, 1886,    2, 0x06,  141 /* Public */,
      43,    0, 1887,    2, 0x06,  142 /* Public */,
      44,    0, 1888,    2, 0x06,  143 /* Public */,
      45,    0, 1889,    2, 0x06,  144 /* Public */,
      46,    0, 1890,    2, 0x06,  145 /* Public */,
      47,    0, 1891,    2, 0x06,  146 /* Public */,
      48,    0, 1892,    2, 0x06,  147 /* Public */,
      49,    0, 1893,    2, 0x06,  148 /* Public */,
      50,    0, 1894,    2, 0x06,  149 /* Public */,
      51,    1, 1895,    2, 0x06,  150 /* Public */,
      53,    1, 1898,    2, 0x06,  152 /* Public */,
      55,    2, 1901,    2, 0x06,  154 /* Public */,
      58,    0, 1906,    2, 0x06,  157 /* Public */,
      59,    0, 1907,    2, 0x06,  158 /* Public */,
      60,    0, 1908,    2, 0x06,  159 /* Public */,
      61,    1, 1909,    2, 0x06,  160 /* Public */,
      64,    0, 1912,    2, 0x06,  162 /* Public */,
      65,    0, 1913,    2, 0x06,  163 /* Public */,
      66,    0, 1914,    2, 0x06,  164 /* Public */,
      67,    2, 1915,    2, 0x06,  165 /* Public */,
      71,    1, 1920,    2, 0x06,  168 /* Public */,
      72,    3, 1923,    2, 0x06,  170 /* Public */,
      75,    1, 1930,    2, 0x06,  174 /* Public */,
      77,    3, 1933,    2, 0x06,  176 /* Public */,
      80,    2, 1940,    2, 0x06,  180 /* Public */,
      82,    1, 1945,    2, 0x06,  183 /* Public */,
      83,    3, 1948,    2, 0x06,  185 /* Public */,
      84,    2, 1955,    2, 0x06,  189 /* Public */,
      85,    1, 1960,    2, 0x06,  192 /* Public */,
      86,    4, 1963,    2, 0x06,  194 /* Public */,
      91,    3, 1972,    2, 0x06,  199 /* Public */,
      92,    4, 1979,    2, 0x06,  203 /* Public */,
      93,    3, 1988,    2, 0x06,  208 /* Public */,
      95,    1, 1995,    2, 0x06,  212 /* Public */,
      96,    3, 1998,    2, 0x06,  214 /* Public */,
      97,    2, 2005,    2, 0x06,  218 /* Public */,
      98,    3, 2010,    2, 0x06,  221 /* Public */,
     100,    1, 2017,    2, 0x06,  225 /* Public */,
     101,    0, 2020,    2, 0x06,  227 /* Public */,
     102,    0, 2021,    2, 0x06,  228 /* Public */,
     103,    0, 2022,    2, 0x06,  229 /* Public */,
     104,    0, 2023,    2, 0x06,  230 /* Public */,
     105,    0, 2024,    2, 0x06,  231 /* Public */,
     106,    0, 2025,    2, 0x06,  232 /* Public */,
     107,    1, 2026,    2, 0x06,  233 /* Public */,
     108,    1, 2029,    2, 0x06,  235 /* Public */,
     109,    1, 2032,    2, 0x06,  237 /* Public */,
     110,    1, 2035,    2, 0x06,  239 /* Public */,
     111,    0, 2038,    2, 0x06,  241 /* Public */,
     112,    0, 2039,    2, 0x06,  242 /* Public */,
     113,    0, 2040,    2, 0x06,  243 /* Public */,
     114,    0, 2041,    2, 0x06,  244 /* Public */,
     115,    0, 2042,    2, 0x06,  245 /* Public */,
     116,    1, 2043,    2, 0x06,  246 /* Public */,
     118,    0, 2046,    2, 0x06,  248 /* Public */,
     119,    0, 2047,    2, 0x06,  249 /* Public */,
     120,    1, 2048,    2, 0x06,  250 /* Public */,
     121,    1, 2051,    2, 0x06,  252 /* Public */,
     122,    1, 2054,    2, 0x06,  254 /* Public */,
     123,    1, 2057,    2, 0x06,  256 /* Public */,
     124,    1, 2060,    2, 0x06,  258 /* Public */,
     125,    0, 2063,    2, 0x06,  260 /* Public */,
     126,    0, 2064,    2, 0x06,  261 /* Public */,
     127,    2, 2065,    2, 0x06,  262 /* Public */,
     130,    2, 2070,    2, 0x06,  265 /* Public */,
     133,    1, 2075,    2, 0x06,  268 /* Public */,
     134,    1, 2078,    2, 0x06,  270 /* Public */,
     135,    1, 2081,    2, 0x06,  272 /* Public */,
     136,    1, 2084,    2, 0x06,  274 /* Public */,
     138,    0, 2087,    2, 0x06,  276 /* Public */,
     139,    1, 2088,    2, 0x06,  277 /* Public */,
     141,    1, 2091,    2, 0x06,  279 /* Public */,
     142,    0, 2094,    2, 0x06,  281 /* Public */,
     143,    2, 2095,    2, 0x06,  282 /* Public */,
     145,    1, 2100,    2, 0x06,  285 /* Public */,
     147,    1, 2103,    2, 0x06,  287 /* Public */,
     149,    1, 2106,    2, 0x06,  289 /* Public */,
     150,    2, 2109,    2, 0x06,  291 /* Public */,
     152,    1, 2114,    2, 0x06,  294 /* Public */,
     154,    1, 2117,    2, 0x06,  296 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
     156,    1, 2120,    2, 0x02,  298 /* Public */,
     158,    1, 2123,    2, 0x02,  300 /* Public */,
     160,    1, 2126,    2, 0x02,  302 /* Public */,
     161,    1, 2129,    2, 0x02,  304 /* Public */,
     162,    1, 2132,    2, 0x02,  306 /* Public */,
     163,    1, 2135,    2, 0x02,  308 /* Public */,
     164,    2, 2138,    2, 0x02,  310 /* Public */,
     167,    0, 2143,    2, 0x02,  313 /* Public */,
     168,    1, 2144,    2, 0x02,  314 /* Public */,
     170,    0, 2147,    2, 0x102,  316 /* Public | MethodIsConst  */,
     171,    1, 2148,    2, 0x102,  317 /* Public | MethodIsConst  */,
     171,    0, 2151,    2, 0x122,  319 /* Public | MethodCloned | MethodIsConst  */,
     172,    1, 2152,    2, 0x102,  320 /* Public | MethodIsConst  */,
     172,    0, 2155,    2, 0x122,  322 /* Public | MethodCloned | MethodIsConst  */,
     173,    1, 2156,    2, 0x102,  323 /* Public | MethodIsConst  */,
     173,    0, 2159,    2, 0x122,  325 /* Public | MethodCloned | MethodIsConst  */,
     174,    1, 2160,    2, 0x02,  326 /* Public */,
     175,    1, 2163,    2, 0x02,  328 /* Public */,
     176,    1, 2166,    2, 0x02,  330 /* Public */,
     177,    0, 2169,    2, 0x02,  332 /* Public */,
     178,    0, 2170,    2, 0x02,  333 /* Public */,
     179,    0, 2171,    2, 0x02,  334 /* Public */,
     180,    0, 2172,    2, 0x02,  335 /* Public */,
     181,    0, 2173,    2, 0x02,  336 /* Public */,
     182,    0, 2174,    2, 0x02,  337 /* Public */,
     183,    0, 2175,    2, 0x02,  338 /* Public */,
     184,    1, 2176,    2, 0x02,  339 /* Public */,
     185,    0, 2179,    2, 0x02,  341 /* Public */,
     186,    1, 2180,    2, 0x02,  342 /* Public */,
     188,    1, 2183,    2, 0x02,  344 /* Public */,
     189,    1, 2186,    2, 0x02,  346 /* Public */,
     191,    1, 2189,    2, 0x102,  348 /* Public | MethodIsConst  */,
     192,    1, 2192,    2, 0x02,  350 /* Public */,
     193,    1, 2195,    2, 0x02,  352 /* Public */,
     193,    0, 2198,    2, 0x22,  354 /* Public | MethodCloned */,
     194,    1, 2199,    2, 0x02,  355 /* Public */,
     194,    0, 2202,    2, 0x22,  357 /* Public | MethodCloned */,
     195,    1, 2203,    2, 0x02,  358 /* Public */,
     195,    0, 2206,    2, 0x22,  360 /* Public | MethodCloned */,
     196,    0, 2207,    2, 0x02,  361 /* Public */,
     197,    1, 2208,    2, 0x02,  362 /* Public */,
     197,    0, 2211,    2, 0x22,  364 /* Public | MethodCloned */,
     198,    1, 2212,    2, 0x02,  365 /* Public */,
     198,    0, 2215,    2, 0x22,  367 /* Public | MethodCloned */,
     199,    1, 2216,    2, 0x02,  368 /* Public */,
     199,    0, 2219,    2, 0x22,  370 /* Public | MethodCloned */,
     200,    1, 2220,    2, 0x02,  371 /* Public */,
     200,    0, 2223,    2, 0x22,  373 /* Public | MethodCloned */,
     201,    1, 2224,    2, 0x02,  374 /* Public */,
     201,    0, 2227,    2, 0x22,  376 /* Public | MethodCloned */,
     202,    1, 2228,    2, 0x02,  377 /* Public */,
     202,    0, 2231,    2, 0x22,  379 /* Public | MethodCloned */,
     203,    1, 2232,    2, 0x02,  380 /* Public */,
     204,    1, 2235,    2, 0x02,  382 /* Public */,
     205,    0, 2238,    2, 0x02,  384 /* Public */,
     206,    0, 2239,    2, 0x02,  385 /* Public */,
     207,    0, 2240,    2, 0x02,  386 /* Public */,
     208,    0, 2241,    2, 0x02,  387 /* Public */,
     209,    0, 2242,    2, 0x02,  388 /* Public */,
     210,    1, 2243,    2, 0x02,  389 /* Public */,
     211,    0, 2246,    2, 0x02,  391 /* Public */,
     212,    1, 2247,    2, 0x02,  392 /* Public */,
     213,    1, 2250,    2, 0x02,  394 /* Public */,
     215,    2, 2253,    2, 0x02,  396 /* Public */,
     217,    0, 2258,    2, 0x02,  399 /* Public */,
     218,    0, 2259,    2, 0x02,  400 /* Public */,
     219,    0, 2260,    2, 0x02,  401 /* Public */,
     220,    1, 2261,    2, 0x02,  402 /* Public */,
     222,    0, 2264,    2, 0x02,  404 /* Public */,
     223,    1, 2265,    2, 0x02,  405 /* Public */,
     225,    0, 2268,    2, 0x02,  407 /* Public */,
     226,    2, 2269,    2, 0x02,  408 /* Public */,
     226,    1, 2274,    2, 0x22,  411 /* Public | MethodCloned */,
     228,    8, 2277,    2, 0x02,  413 /* Public */,
     235,    0, 2294,    2, 0x02,  422 /* Public */,
     236,    7, 2295,    2, 0x02,  423 /* Public */,
     240,    3, 2310,    2, 0x02,  431 /* Public */,
     240,    2, 2317,    2, 0x22,  435 /* Public | MethodCloned */,
     242,    3, 2322,    2, 0x02,  438 /* Public */,
     242,    2, 2329,    2, 0x22,  442 /* Public | MethodCloned */,
     243,    4, 2334,    2, 0x02,  445 /* Public */,
     243,    3, 2343,    2, 0x22,  450 /* Public | MethodCloned */,
     243,    2, 2350,    2, 0x22,  454 /* Public | MethodCloned */,
     243,    1, 2355,    2, 0x22,  457 /* Public | MethodCloned */,
     244,    3, 2358,    2, 0x02,  459 /* Public */,
     244,    2, 2365,    2, 0x22,  463 /* Public | MethodCloned */,
     245,    1, 2370,    2, 0x02,  466 /* Public */,
     247,    1, 2373,    2, 0x02,  468 /* Public */,
     248,    1, 2376,    2, 0x02,  470 /* Public */,
     249,    7, 2379,    2, 0x02,  472 /* Public */,
     249,    6, 2394,    2, 0x22,  480 /* Public | MethodCloned */,
     249,    5, 2407,    2, 0x22,  487 /* Public | MethodCloned */,
     256,    1, 2418,    2, 0x02,  493 /* Public */,
     257,    1, 2421,    2, 0x02,  495 /* Public */,
     258,    1, 2424,    2, 0x02,  497 /* Public */,
     259,    1, 2427,    2, 0x02,  499 /* Public */,
     260,    0, 2430,    2, 0x02,  501 /* Public */,
     261,    2, 2431,    2, 0x02,  502 /* Public */,
     264,    3, 2436,    2, 0x02,  505 /* Public */,
     266,    0, 2443,    2, 0x02,  509 /* Public */,
     267,    0, 2444,    2, 0x102,  510 /* Public | MethodIsConst  */,
     268,    0, 2445,    2, 0x102,  511 /* Public | MethodIsConst  */,
     269,    1, 2446,    2, 0x02,  512 /* Public */,
     270,    1, 2449,    2, 0x102,  514 /* Public | MethodIsConst  */,
     271,    1, 2452,    2, 0x02,  516 /* Public */,
     273,    1, 2455,    2, 0x02,  518 /* Public */,
     274,    1, 2458,    2, 0x02,  520 /* Public */,
     275,    1, 2461,    2, 0x02,  522 /* Public */,
     276,    1, 2464,    2, 0x02,  524 /* Public */,
     277,    0, 2467,    2, 0x02,  526 /* Public */,
     278,    3, 2468,    2, 0x02,  527 /* Public */,
     281,    3, 2475,    2, 0x02,  531 /* Public */,
     282,    8, 2482,    2, 0x02,  535 /* Public */,
     282,    7, 2499,    2, 0x22,  544 /* Public | MethodCloned */,
     282,    6, 2514,    2, 0x22,  552 /* Public | MethodCloned */,
     282,    5, 2527,    2, 0x22,  559 /* Public | MethodCloned */,
     282,    4, 2538,    2, 0x22,  565 /* Public | MethodCloned */,
     289,    6, 2547,    2, 0x02,  570 /* Public */,
     289,    5, 2560,    2, 0x22,  577 /* Public | MethodCloned */,
     289,    4, 2571,    2, 0x22,  583 /* Public | MethodCloned */,
     294,    1, 2580,    2, 0x02,  588 /* Public */,
     295,    1, 2583,    2, 0x02,  590 /* Public */,
     297,    1, 2586,    2, 0x02,  592 /* Public */,
     298,    1, 2589,    2, 0x02,  594 /* Public */,
     300,    0, 2592,    2, 0x02,  596 /* Public */,
     301,    1, 2593,    2, 0x02,  597 /* Public */,
     302,    1, 2596,    2, 0x102,  599 /* Public | MethodIsConst  */,
     303,    2, 2599,    2, 0x02,  601 /* Public */,
     305,    1, 2604,    2, 0x102,  604 /* Public | MethodIsConst  */,
     306,    2, 2607,    2, 0x02,  606 /* Public */,
     307,    1, 2612,    2, 0x02,  609 /* Public */,
     308,    1, 2615,    2, 0x102,  611 /* Public | MethodIsConst  */,
     309,    2, 2618,    2, 0x02,  613 /* Public */,
     310,    1, 2623,    2, 0x102,  616 /* Public | MethodIsConst  */,
     311,    2, 2626,    2, 0x02,  618 /* Public */,
     312,    1, 2631,    2, 0x102,  621 /* Public | MethodIsConst  */,
     313,    2, 2634,    2, 0x02,  623 /* Public */,
     315,    1, 2639,    2, 0x102,  626 /* Public | MethodIsConst  */,
     316,    1, 2642,    2, 0x102,  628 /* Public | MethodIsConst  */,
     317,    2, 2645,    2, 0x02,  630 /* Public */,
     318,    1, 2650,    2, 0x102,  633 /* Public | MethodIsConst  */,
     319,    2, 2653,    2, 0x02,  635 /* Public */,
     320,    1, 2658,    2, 0x102,  638 /* Public | MethodIsConst  */,
     321,    1, 2661,    2, 0x102,  640 /* Public | MethodIsConst  */,
     322,    2, 2664,    2, 0x02,  642 /* Public */,
     323,    1, 2669,    2, 0x102,  645 /* Public | MethodIsConst  */,
     324,    2, 2672,    2, 0x02,  647 /* Public */,
     325,    1, 2677,    2, 0x102,  650 /* Public | MethodIsConst  */,
     326,    1, 2680,    2, 0x02,  652 /* Public */,
     327,    1, 2683,    2, 0x02,  654 /* Public */,
     328,    1, 2686,    2, 0x02,  656 /* Public */,
     329,    1, 2689,    2, 0x02,  658 /* Public */,
     331,    1, 2692,    2, 0x102,  660 /* Public | MethodIsConst  */,
     332,    0, 2695,    2, 0x02,  662 /* Public */,
     333,    0, 2696,    2, 0x02,  663 /* Public */,
     334,    0, 2697,    2, 0x02,  664 /* Public */,
     335,    2, 2698,    2, 0x02,  665 /* Public */,
     335,    1, 2703,    2, 0x22,  668 /* Public | MethodCloned */,
     337,    2, 2706,    2, 0x02,  670 /* Public */,
     337,    1, 2711,    2, 0x22,  673 /* Public | MethodCloned */,
     338,    2, 2714,    2, 0x02,  675 /* Public */,
     338,    1, 2719,    2, 0x22,  678 /* Public | MethodCloned */,
     339,    2, 2722,    2, 0x02,  680 /* Public */,
     339,    1, 2727,    2, 0x22,  683 /* Public | MethodCloned */,
     341,    1, 2730,    2, 0x02,  685 /* Public */,
     342,    0, 2733,    2, 0x02,  687 /* Public */,
     343,    1, 2734,    2, 0x02,  688 /* Public */,
     344,    1, 2737,    2, 0x02,  690 /* Public */,
     345,    1, 2740,    2, 0x02,  692 /* Public */,
     346,    1, 2743,    2, 0x02,  694 /* Public */,
     347,    2, 2746,    2, 0x02,  696 /* Public */,
     347,    1, 2751,    2, 0x02,  699 /* Public */,
     350,    2, 2754,    2, 0x02,  701 /* Public */,
     350,    1, 2759,    2, 0x02,  704 /* Public */,
     352,    1, 2762,    2, 0x02,  706 /* Public */,
     353,    1, 2765,    2, 0x02,  708 /* Public */,
     354,    0, 2768,    2, 0x02,  710 /* Public */,
     355,    0, 2769,    2, 0x02,  711 /* Public */,
     356,    1, 2770,    2, 0x02,  712 /* Public */,
     358,    0, 2773,    2, 0x02,  714 /* Public */,
     359,    0, 2774,    2, 0x102,  715 /* Public | MethodIsConst  */,
     361,    0, 2775,    2, 0x102,  716 /* Public | MethodIsConst  */,
     362,    1, 2776,    2, 0x02,  717 /* Public */,
     364,    1, 2779,    2, 0x02,  719 /* Public */,
     365,    0, 2782,    2, 0x102,  721 /* Public | MethodIsConst  */,
     367,    0, 2783,    2, 0x102,  722 /* Public | MethodIsConst  */,
     369,    0, 2784,    2, 0x102,  723 /* Public | MethodIsConst  */,
     371,    0, 2785,    2, 0x02,  724 /* Public */,
     372,    1, 2786,    2, 0x02,  725 /* Public */,
     373,    1, 2789,    2, 0x02,  727 /* Public */,
     375,    1, 2792,    2, 0x102,  729 /* Public | MethodIsConst  */,
     376,    1, 2795,    2, 0x02,  731 /* Public */,
     377,    1, 2798,    2, 0x02,  733 /* Public */,
     378,    1, 2801,    2, 0x02,  735 /* Public */,
     379,    1, 2804,    2, 0x02,  737 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    4,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    9,
    QMetaType::Void, QMetaType::LongLong, QMetaType::LongLong,   11,   12,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   14,   15,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   18,   19,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   21,   22,
    QMetaType::Void, QMetaType::QString,   24,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString,    4,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   52,
    QMetaType::Void, QMetaType::QString,   54,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,   56,   57,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 62,   63,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 68, QMetaType::Int,   69,   70,
    QMetaType::Void, QMetaType::QString,   24,
    QMetaType::Void, QMetaType::QString, QMetaType::Bool, QMetaType::QString,   73,   52,   74,
    QMetaType::Void, 0x80000000 | 62,   76,
    QMetaType::Void, QMetaType::QString, QMetaType::QStringList, 0x80000000 | 62,   73,   78,   79,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   81,   12,
    QMetaType::Void, 0x80000000 | 62,   76,
    QMetaType::Void, QMetaType::QString, QMetaType::QStringList, 0x80000000 | 62,   73,   78,   79,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   81,   12,
    QMetaType::Void, 0x80000000 | 68,   78,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::LongLong, QMetaType::QString,   87,   88,   89,   90,
    QMetaType::Void, QMetaType::Int, QMetaType::LongLong, QMetaType::LongLong,   87,   11,   12,
    QMetaType::Void, QMetaType::Int, QMetaType::Bool, QMetaType::QString, QMetaType::QString,   87,   52,   74,   90,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::QString,   87,   94,   90,
    QMetaType::Void, 0x80000000 | 62,   76,
    QMetaType::Void, QMetaType::QString, QMetaType::QStringList, 0x80000000 | 62,   73,   78,   79,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   81,   12,
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::QString,   99,   12,   88,
    QMetaType::Void, QMetaType::Bool,   52,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   90,
    QMetaType::Void, 0x80000000 | 68,   69,
    QMetaType::Void, 0x80000000 | 68,   69,
    QMetaType::Void, 0x80000000 | 68,   69,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,  117,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   24,
    QMetaType::Void, 0x80000000 | 68,   78,
    QMetaType::Void, 0x80000000 | 68,   78,
    QMetaType::Void, 0x80000000 | 68,   78,
    QMetaType::Void, 0x80000000 | 68,   78,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,  128,  129,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,  131,  132,
    QMetaType::Void, QMetaType::QString,   73,
    QMetaType::Void, QMetaType::QString,   73,
    QMetaType::Void, QMetaType::QString,   73,
    QMetaType::Void, QMetaType::Bool,  137,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,  140,
    QMetaType::Void, QMetaType::QString,   14,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,  144,   12,
    QMetaType::Void, QMetaType::Bool,  146,
    QMetaType::Void, QMetaType::QStringList,  148,
    QMetaType::Void, QMetaType::QString,   18,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,  151,   79,
    QMetaType::Void, QMetaType::QStringList,  153,
    QMetaType::Void, QMetaType::QString,  155,

 // methods: parameters
    QMetaType::Void, QMetaType::Bool,  157,
    QMetaType::Void, QMetaType::QString,  159,
    QMetaType::Void, QMetaType::QReal,  157,
    QMetaType::Void, QMetaType::QReal,  157,
    QMetaType::Void, QMetaType::QReal,  157,
    QMetaType::Void, QMetaType::QReal,  157,
    QMetaType::Void, QMetaType::QReal, QMetaType::QReal,  165,  166,
    QMetaType::QString,
    QMetaType::Void, QMetaType::Int,  169,
    QMetaType::Int,
    0x80000000 | 68, QMetaType::QString,   35,
    0x80000000 | 68,
    0x80000000 | 68, QMetaType::QString,   35,
    0x80000000 | 68,
    0x80000000 | 68, QMetaType::QString,   35,
    0x80000000 | 68,
    QMetaType::Void, QMetaType::QString,   21,
    QMetaType::Void, QMetaType::QString,   21,
    QMetaType::Void, QMetaType::QString,   21,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    0x80000000 | 68,
    QMetaType::QString,
    QMetaType::QString,
    QMetaType::QString,
    QMetaType::Void, QMetaType::Int,  169,
    0x80000000 | 62,
    QMetaType::Void, QMetaType::Int,  187,
    QMetaType::Void, QMetaType::Int,  187,
    QMetaType::Void, QMetaType::Bool,  190,
    QMetaType::QString, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString,   35,
    QMetaType::Bool, QMetaType::QString,   35,
    QMetaType::Bool,
    QMetaType::Bool, QMetaType::QString,   35,
    QMetaType::Bool,
    QMetaType::Bool, QMetaType::QString,   35,
    QMetaType::Bool,
    QMetaType::Bool,
    QMetaType::Bool, QMetaType::QString,   35,
    QMetaType::Bool,
    QMetaType::Bool, QMetaType::QString,   35,
    QMetaType::Bool,
    QMetaType::Bool, QMetaType::QString,   35,
    QMetaType::Bool,
    QMetaType::Bool, QMetaType::QString,   35,
    QMetaType::Bool,
    QMetaType::Bool, QMetaType::QString,   35,
    QMetaType::Bool,
    QMetaType::Bool, QMetaType::QString,   35,
    QMetaType::Bool,
    QMetaType::Void, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString,   35,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   35,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString,  214,
    QMetaType::Void, QMetaType::QString, QMetaType::Bool,   35,  216,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::LongLong,  221,
    0x80000000 | 68,
    0x80000000 | 68, QMetaType::QString,  224,
    0x80000000 | 68,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,  227,  224,
    QMetaType::Void, QMetaType::QString,  227,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::Int, QMetaType::Int,  227,  224,  229,  230,  231,  232,  233,  234,
    0x80000000 | 62,
    QMetaType::Void, QMetaType::QString, QMetaType::QStringList, QMetaType::QStringList, QMetaType::QStringList, QMetaType::QStringList, QMetaType::Int, QMetaType::Int,  227,  237,  238,  239,  224,  233,  234,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString,   73,  230,  241,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   73,  230,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString,   73,  230,  241,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   73,  230,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::Int, QMetaType::QStringList,  227,  230,  233,  238,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::Int,  227,  230,  233,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,  227,  230,
    QMetaType::Void, QMetaType::QString,  227,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString,   73,  230,  241,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   73,  230,
    QMetaType::Void, QMetaType::QStringList,  246,
    QMetaType::Void, QMetaType::QStringList,  246,
    QMetaType::Void, QMetaType::QStringList,  246,
    QMetaType::Int, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::LongLong, QMetaType::QString, QMetaType::LongLong, QMetaType::Int,  250,  251,   90,  252,  253,  254,  255,
    QMetaType::Int, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::LongLong, QMetaType::QString, QMetaType::LongLong,  250,  251,   90,  252,  253,  254,
    QMetaType::Int, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::LongLong, QMetaType::QString,  250,  251,   90,  252,  253,
    QMetaType::Void, QMetaType::Int,   87,
    QMetaType::Void, QMetaType::Int,   87,
    QMetaType::Void, QMetaType::Int,   87,
    QMetaType::Void, QMetaType::Int,   87,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,  262,  263,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::Int,  262,  265,  263,
    QMetaType::Void,
    0x80000000 | 68,
    QMetaType::QString,
    QMetaType::Void, QMetaType::QString,  128,
    QMetaType::QString, QMetaType::QString,  128,
    QMetaType::Void, QMetaType::QString,  272,
    QMetaType::Void, QMetaType::QString,  272,
    QMetaType::Void, QMetaType::QString,  272,
    QMetaType::Void, QMetaType::QString,  272,
    QMetaType::Void, QMetaType::QString,  272,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString,  279,  280,  253,
    QMetaType::Bool, QMetaType::QString, QMetaType::QString, QMetaType::QString,   14,  250,  251,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,  272,  283,  284,   54,  285,  286,  287,  288,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,  272,  283,  284,   54,  285,  286,  287,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,  272,  283,  284,   54,  285,  286,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,  272,  283,  284,   54,  285,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,  272,  283,  284,   54,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,  272,  290,  291,   54,  292,  293,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,  272,  290,  291,   54,  292,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,  272,  290,  291,   54,
    QMetaType::Void, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString,  296,
    0x80000000 | 62, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString,  299,
    QMetaType::Int,
    QMetaType::Void, QMetaType::Bool,  190,
    QMetaType::Int, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,   35,  304,
    QMetaType::Int, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,   35,  187,
    QMetaType::Int, QMetaType::QString,   35,
    QMetaType::Int, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,   35,  304,
    QMetaType::Int, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,   35,  304,
    QMetaType::QString, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   35,  314,
    QMetaType::QString, QMetaType::QString,   35,
    QMetaType::Int, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,   35,  304,
    QMetaType::QString, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   35,  314,
    QMetaType::QString, QMetaType::QString,   35,
    QMetaType::Int, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,   35,  304,
    QMetaType::Bool, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString, QMetaType::Bool,   35,  157,
    QMetaType::Bool, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString,  314,
    QMetaType::Void, QMetaType::QString,  314,
    QMetaType::Void, QMetaType::Bool,  157,
    QMetaType::Void, QMetaType::QString,  330,
    QMetaType::Bool, QMetaType::QString,   35,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,  336,   35,
    QMetaType::Void, QMetaType::QString,  336,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,  336,   35,
    QMetaType::Void, QMetaType::QString,  336,
    QMetaType::Bool, QMetaType::QString, QMetaType::QString,   74,   35,
    QMetaType::Bool, QMetaType::QString,   74,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,  340,   35,
    QMetaType::Void, QMetaType::QString,  340,
    QMetaType::Void, QMetaType::QString,   35,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    2,
    QMetaType::Void, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString,   35,
    QMetaType::Bool, QMetaType::QString, QMetaType::QString,  348,  349,
    QMetaType::Bool, QMetaType::QString,  348,
    QMetaType::Bool, QMetaType::QString, QMetaType::QString,  351,  349,
    QMetaType::Bool, QMetaType::QString,  351,
    QMetaType::QString, QMetaType::QString,   35,
    QMetaType::Void, QMetaType::QString,   35,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,  357,
    QMetaType::Void,
    0x80000000 | 360,
    QMetaType::Int,
    QMetaType::Void, QMetaType::Int,  363,
    QMetaType::Void, QMetaType::QString,  117,
    0x80000000 | 366,
    0x80000000 | 368,
    0x80000000 | 370,
    QMetaType::Void,
    QMetaType::QString, QMetaType::QString,  250,
    QMetaType::Void, QMetaType::QStringList,  374,
    QMetaType::QString, QMetaType::QString,  250,
    QMetaType::QString, QMetaType::QString,  250,
    QMetaType::Void, QMetaType::QStringList,  374,
    QMetaType::QString, QMetaType::QString,  250,
    QMetaType::Void, QMetaType::QStringList,  374,

 // properties: name, type, flags, notifyId, revision
     380, QMetaType::QObjectStar, 0x00015401, uint(-1), 0,
     381, QMetaType::QObjectStar, 0x00015401, uint(-1), 0,
      21, QMetaType::QString, 0x00015001, uint(8), 0,
     382, QMetaType::QString, 0x00015001, uint(8), 0,
     383, QMetaType::Bool, 0x00015001, uint(8), 0,
     384, QMetaType::QString, 0x00015001, uint(8), 0,
     385, QMetaType::QString, 0x00015001, uint(8), 0,
     262, QMetaType::QString, 0x00015001, uint(12), 0,
     386, QMetaType::QString, 0x00015001, uint(13), 0,
     387, QMetaType::QStringList, 0x00015001, uint(14), 0,
     388, QMetaType::Int, 0x00015103, uint(79), 0,
     389, QMetaType::QString, 0x00015001, uint(15), 0,
     390, QMetaType::QString, 0x00015001, uint(15), 0,
     391, QMetaType::Int, 0x00015001, uint(15), 0,
     392, QMetaType::Bool, 0x00015001, uint(15), 0,
     393, QMetaType::Bool, 0x00015001, uint(15), 0,
     394, QMetaType::Int, 0x00015001, uint(17), 0,
     395, QMetaType::Int, 0x00015001, uint(17), 0,
     396, QMetaType::Bool, 0x00015001, uint(25), 0,
     397, QMetaType::Bool, 0x00015103, uint(26), 0,
     398, 0x80000000 | 68, 0x00015009, uint(15), 0,
     399, QMetaType::QString, 0x00015001, uint(32), 0,
     400, QMetaType::QStringList, 0x00015001, uint(27), 0,
     401, 0x80000000 | 68, 0x00015009, uint(27), 0,
     402, QMetaType::QStringList, 0x00015001, uint(27), 0,
     403, QMetaType::QStringList, 0x00015001, uint(27), 0,
     404, QMetaType::QStringList, 0x00015001, uint(27), 0,
     405, QMetaType::QStringList, 0x00015001, uint(27), 0,
     406, QMetaType::QStringList, 0x00015001, uint(30), 0,
     407, QMetaType::QStringList, 0x00015001, uint(31), 0,
     408, QMetaType::Bool, 0x00015001, uint(35), 0,
     409, QMetaType::QString, 0x00015001, uint(35), 0,
     410, QMetaType::QString, 0x00015001, uint(36), 0,
     411, QMetaType::Bool, 0x00015001, uint(40), 0,
     412, 0x80000000 | 62, 0x00015009, uint(43), 0,
     413, QMetaType::QString, 0x00015001, uint(39), 0,
     414, QMetaType::Bool, 0x00015001, uint(44), 0,
     415, QMetaType::Int, 0x00015001, uint(45), 0,
     416, QMetaType::QString, 0x00015001, uint(40), 0,
     417, QMetaType::QString, 0x00015001, uint(40), 0,
     418, QMetaType::Bool, 0x00015001, uint(46), 0,
      98, QMetaType::Int, 0x00015001, uint(64), 0,
     419, QMetaType::Int, 0x00015001, uint(64), 0,
     420, QMetaType::Int, 0x00015001, uint(64), 0,
     421, QMetaType::QString, 0x00015001, uint(64), 0,
     422, QMetaType::QObjectStar, 0x00015401, uint(-1), 0,
     423, QMetaType::QObjectStar, 0x00015401, uint(-1), 0,
     424, QMetaType::QString, 0x00015001, uint(76), 0,
     425, QMetaType::QString, 0x00015401, uint(-1), 0,
     426, QMetaType::QString, 0x00015401, uint(-1), 0,
     296, QMetaType::QString, 0x00015001, uint(77), 0,
     427, QMetaType::Bool, 0x00015401, uint(-1), 0,
     428, QMetaType::QObjectStar, 0x00015401, uint(-1), 0,
     429, QMetaType::Bool, 0x00015001, uint(3), 0,
     430, QMetaType::Int, 0x00015001, uint(4), 0,
     431, QMetaType::Bool, 0x00015001, uint(78), 0,
     432, QMetaType::QString, 0x00015401, uint(-1), 0,
     433, QMetaType::QString, 0x00015401, uint(-1), 0,
     434, QMetaType::QString, 0x00015401, uint(-1), 0,
     435, QMetaType::Bool, 0x00015103, uint(-1), 0,
     436, QMetaType::QString, 0x00015001, uint(80), 0,
     437, QMetaType::QReal, 0x00015001, uint(80), 0,
     438, QMetaType::QReal, 0x00015001, uint(80), 0,
     439, QMetaType::QReal, 0x00015103, uint(80), 0,
     440, QMetaType::QReal, 0x00015103, uint(80), 0,
     441, 0x80000000 | 68, 0x00015009, uint(28), 0,
     442, QMetaType::Bool, 0x00015001, uint(29), 0,
     443, 0x80000000 | 62, 0x00015009, uint(34), 0,
     444, 0x80000000 | 68, 0x00015009, uint(71), 0,
     445, 0x80000000 | 68, 0x00015009, uint(71), 0,
     446, 0x80000000 | 62, 0x00015009, uint(76), 0,
     447, 0x80000000 | 68, 0x00015409, uint(-1), 0,
     448, QMetaType::LongLong, 0x00015401, uint(-1), 0,
     449, QMetaType::Int, 0x00015401, uint(-1), 0,
     450, QMetaType::Bool, 0x00015001, uint(17), 0,
     451, 0x80000000 | 62, 0x00015009, uint(17), 0,
     267, 0x80000000 | 68, 0x00015409, uint(-1), 0,
     268, QMetaType::QString, 0x00015401, uint(-1), 0,
     452, QMetaType::QString, 0x00015001, uint(82), 0,
     453, QMetaType::Bool, 0x00015001, uint(83), 0,
     454, QMetaType::Int, 0x00015103, uint(24), 0,
     455, QMetaType::Int, 0x00015103, uint(24), 0,
     456, QMetaType::Int, 0x00015103, uint(24), 0,
     457, QMetaType::Double, 0x00015103, uint(24), 0,
     458, QMetaType::QString, 0x00015001, uint(18), 0,
     459, QMetaType::QString, 0x00015001, uint(19), 0,
     460, QMetaType::Bool, 0x00015001, uint(20), 0,
     461, QMetaType::Bool, 0x00015001, uint(66), 0,
     462, QMetaType::Bool, 0x00015001, uint(67), 0,
     463, QMetaType::Int, 0x00015001, uint(68), 0,
     464, QMetaType::Int, 0x00015001, uint(69), 0,
     465, QMetaType::Bool, 0x00015001, uint(70), 0,
     466, QMetaType::QString, 0x00015001, uint(70), 0,
     467, QMetaType::QObjectStar, 0x00015401, uint(-1), 0,
     468, QMetaType::QString, 0x00015001, uint(2), 0,
     469, QMetaType::Double, 0x00015001, uint(89), 0,
     470, 0x80000000 | 68, 0x00015009, uint(89), 0,
     471, QMetaType::Bool, 0x00015001, uint(90), 0,
     472, QMetaType::Bool, 0x00015001, uint(89), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::ShadowBackend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN14ShadowLauncher13ShadowBackendE.offsetsAndSizes,
    qt_meta_data_ZN14ShadowLauncher13ShadowBackendE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN14ShadowLauncher13ShadowBackendE_t,
        // property 'account'
        QtPrivate::TypeAndForceComplete<QObject*, std::true_type>,
        // property 'yggdrasil'
        QtPrivate::TypeAndForceComplete<QObject*, std::true_type>,
        // property 'username'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'offlineUsername'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'isOnline'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'accountUuid'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'offlineUuid'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'skinPath'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'offlineSkinPath'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'offlineUsernames'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'lastLoginMode'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'javaPath'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'javaVersion'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'javaMajor'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'javaInstalled'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'javaReady'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'minMemoryMb'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'maxMemoryMb'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'isolationEnabled'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'embeddedLoginEnabled'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'availableJavaList'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // property 'selectedVersion'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'versionIds'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'versionList'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // property 'releaseVersions'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'snapshotVersions'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'oldVersions'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'aprilFoolVersions'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'installedVersions'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'activeVersionNames'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'installing'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'installVersionId'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'installPhase'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'launching'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'lastCrash'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::true_type>,
        // property 'launchStatus'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'isRunning'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'runningCount'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'launchVersion'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'launchUsername'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'downloading'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'resourceDownloadProgress'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'resourceDownloadTotal'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'resourceDownloadSpeed'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'resourceDownloadFile'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'modManager'
        QtPrivate::TypeAndForceComplete<QObject*, std::true_type>,
        // property 'multiplayer'
        QtPrivate::TypeAndForceComplete<QObject*, std::true_type>,
        // property 'gameDir'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'dataDir'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'appVersion'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'theme'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'devMode'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'modpackImporter'
        QtPrivate::TypeAndForceComplete<QObject*, std::true_type>,
        // property 'updateChecking'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'updateState'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'agreementAccepted'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'betaAgreementHtml'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'privacyAgreementHtml'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'termsAgreementHtml'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'markAgreed'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'customBgPath'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'sidebarOpacity'
        QtPrivate::TypeAndForceComplete<qreal, std::true_type>,
        // property 'contentOpacity'
        QtPrivate::TypeAndForceComplete<qreal, std::true_type>,
        // property 'cropX'
        QtPrivate::TypeAndForceComplete<qreal, std::true_type>,
        // property 'cropY'
        QtPrivate::TypeAndForceComplete<qreal, std::true_type>,
        // property 'versionDetails'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // property 'isScanningVersions'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'currentVersionSummary'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::true_type>,
        // property 'downloadQueue'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // property 'activeDownloads'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // property 'gameDirInfo'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::true_type>,
        // property 'gameDirectories'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // property 'diskFree'
        QtPrivate::TypeAndForceComplete<qint64, std::true_type>,
        // property 'diskPercent'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'autoMemoryEnabled'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'systemMemoryInfo'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::true_type>,
        // property 'availableCapes'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // property 'loginType'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'selectedSkinPath'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'wardrobeBusy'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'fileDownloadSource'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'listDownloadSource'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'maxDownloadThreads'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'downloadSpeedLimitMB'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'jvmArgs'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'gameArgs'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'highPerfGpu'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'verifyRunning'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'repairRunning'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'verifyChecked'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'verifyTotal'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'verifyResultOk'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'verifyResultText'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'installCardsModel'
        QtPrivate::TypeAndForceComplete<QObject*, std::true_type>,
        // property 'betaStatus'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'totalGameHours'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'versionGameStats'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // property 'statsLoading'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'statsEmpty'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<ShadowBackend, std::true_type>,
        // method 'betaVerified'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'betaKeyInvalid'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'betaStatusChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateCheckingChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'toastMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'updateDownloadProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        // method 'updateChangelogAvailable'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'accountChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'microsoftLoginProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'microsoftLoginSuccess'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'microsoftLoginFailed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'skinReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'offlineSkinReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'offlineHistoryChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'javaPathChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'javaReadyChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'memorySettingsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'jvmArgsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'gameArgsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'highPerfGpuChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'versionLaunchSettingsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'launchBlocked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'generalSettingsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'downloadSettingsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'isolationChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'embeddedLoginChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'versionListReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'versionDetailsReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'scanningChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'installedVersionsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'activeVersionNamesChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectedVersionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectedVersionClearedAfterDelete'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'currentVersionSummaryChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'installStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'installPhaseChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'installFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'installComplete'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'launchProgressChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'launchStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'minecraftStarted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'minecraftStopped'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'crashDetected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'isRunningChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'runningCountChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'resourceDownloadStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'resourcepackSearchCompleted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'resourcepackSearchFailed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'resourcepackDownloadFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'resourcepackVersionsLoaded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'resourcepackVersionsPartial'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'resourcepackVersionsProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'modVersionsLoaded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'modVersionsPartial'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'modVersionsProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'fabricApiVersionsReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantList &, std::false_type>,
        // method 'modFileDownloadStarted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'modFileDownloadProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        // method 'modFileDownloadFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'modFileDownloadFailed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'shaderVersionsLoaded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'shaderVersionsPartial'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'shaderVersionsProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'resourceDownloadProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'resourceDownloadDone'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'verifyRunningChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'repairRunningChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'verifyCheckedChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'verifyTotalChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'verifyResultTextChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'downloadQueueChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'downloadQueueFull'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'searchResultsReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantList &, std::false_type>,
        // method 'modSearchResultsReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantList &, std::false_type>,
        // method 'shaderSearchResultsReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantList &, std::false_type>,
        // method 'gameDirChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'themeChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'agreementAcceptedChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'loginModeChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'customBgChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'logMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'skinChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'wardrobeBusyChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'wardrobeError'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'forgeVersionsReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantList &, std::false_type>,
        // method 'fabricVersionsReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantList &, std::false_type>,
        // method 'neoforgeVersionsReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantList &, std::false_type>,
        // method 'optifineVersionsReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantList &, std::false_type>,
        // method 'statsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'statsLoadingChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'iconCached'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'navigateToRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'openRpDetailRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'openModDetailRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'openShaderDetailRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setRpShowPreReleases'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'openRpVersionMenu'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'expandRpDetailGroup'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'selectRpDetailSubVer'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'verifyStarted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'verifyProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'verifyFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'verifyFailedFiles'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        // method 'launchCheckProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'launchCheckFailed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'launchCheckMissingFiles'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        // method 'launchCheckWarning'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setEmbeddedLoginEnabled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'setCustomBgPath'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setSidebarOpacity'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qreal, std::false_type>,
        // method 'setContentOpacity'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qreal, std::false_type>,
        // method 'setCropX'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qreal, std::false_type>,
        // method 'setCropY'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qreal, std::false_type>,
        // method 'updateCrop'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qreal, std::false_type>,
        QtPrivate::TypeAndForceComplete<qreal, std::false_type>,
        // method 'pickBackgroundImage'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'switchLanguage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'readLanguageFile'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'listMods'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'listMods'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        // method 'listResourcePacks'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'listResourcePacks'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        // method 'listSaves'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'listSaves'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        // method 'offlineLogin'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'updateOfflineSkin'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'removeOfflineUsername'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'microsoftLogin'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'cancelMicrosoftLogin'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'logout'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'scanJavaInstallations'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        // method 'autoSelectJava'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'detectJava'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'browseJava'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'selectJavaByIndex'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'getMemoryStatus'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::false_type>,
        // method 'setMinMemory'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setMaxMemory'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setIsolationEnabled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'getVersionGameDir'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'migrateVersionToIsolated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'openGameDir'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'openGameDir'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'openLatestLog'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'openLatestLog'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'openLogsFolder'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'openLogsFolder'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'openLauncherLogsFolder'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'openCrashLog'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'openCrashLog'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'openSavesFolder'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'openSavesFolder'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'openScreenshotsFolder'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'openScreenshotsFolder'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'openModsFolder'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'openModsFolder'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'openResourcePacksFolder'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'openResourcePacksFolder'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'openShaderPacksFolder'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'openShaderPacksFolder'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'openVersionDir'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'deleteVersion'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'refreshVersionList'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'refreshInstalled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'refreshInstalledList'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'refreshVersionDetails'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'refreshGameDirInfo'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'installVersion'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cancelInstall'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'cancelVersionInstall'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'dismissCard'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'launch'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'cancelLaunch'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'killGameProcess'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'killMinecraft'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'killGameByPid'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        // method 'runningGames'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        // method 'getPopularMods'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'getShaderList'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        // method 'searchMods'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'searchMods'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'searchModsEx'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'getModCategories'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::false_type>,
        // method 'searchShadersEx'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'downloadMod'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'downloadMod'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'downloadShader'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'downloadShader'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'searchResourcepacks'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        // method 'searchResourcepacks'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'searchResourcepacks'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'searchResourcepacks'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'downloadResourcepack'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'downloadResourcepack'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'fetchResourcepackVersions'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        // method 'fetchModVersions'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        // method 'fetchShaderVersions'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        // method 'downloadModFile'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'downloadModFile'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        // method 'downloadModFile'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cancelModFileDownload'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'pauseModFileDownload'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'resumeModFileDownload'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'retryModFileDownload'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'browseSkin'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'uploadSkin'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'saveWardrobeSettings'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'saveSkinToFile'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'availableCapes'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        // method 'loginType'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'cacheIconAsync'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cachedIconPath'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'queryForgeVersions'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'queryFabricVersions'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'queryNeoForgeVersions'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'queryOptifineVersions'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'queryFabricApiVersions'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cancelModLoaderQueries'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'cacheForgeInstallerSha1'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'installFabricApi'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'installModLoader'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'installModLoader'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'installModLoader'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'installModLoader'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'installModLoader'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'installOptifine'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'installOptifine'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'installOptifine'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setSelectedVersion'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setTheme'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'checkAll'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setGameDir'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'getAutoMemory'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setAutoMemoryEnabled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'versionMemoryMode'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setVersionMemoryMode'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'versionMemoryManualMB'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setVersionMemoryManualMB'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'resolvedMemoryMB'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'versionJavaMode'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setVersionJavaMode'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'versionJvmArgsMode'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setVersionJvmArgsMode'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'versionJvmArgs'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setVersionJvmArgs'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'resolvedJvmArgs'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'versionGameArgsMode'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setVersionGameArgsMode'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'versionGameArgs'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setVersionGameArgs'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'resolvedGameArgs'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'versionHighPerfGpuMode'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setVersionHighPerfGpuMode'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'versionHighPerfGpu'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setVersionHighPerfGpu'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'resolvedHighPerfGpu'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setJvmArgs'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setGameArgs'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setHighPerfGpu'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'copyToClipboard'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'isModdedVersion'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'openJavaFileDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'pickJava'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'checkFileChanges'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'deleteMod'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'deleteMod'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'deleteResourcePack'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'deleteResourcePack'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'importMod'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'importMod'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'deleteSave'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'deleteSave'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'migrateVersion'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'openConfigFolder'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'removeGameDir'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'cancelQueuedDownload'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'verifyVersion'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cleanCorruptVersion'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'renameVersion'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'renameVersion'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cloneVersion'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cloneVersion'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'copyVersionPath'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'repairVersion'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cancelVerify'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openVerifyReport'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'submitBetaKey'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'checkForUpdate'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'settings'
        QtPrivate::TypeAndForceComplete<SettingsBackend *, std::false_type>,
        // method 'diagAutoLangComboIdx'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setAutoLangModeFromCombo'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'logUiMsg'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'statsBackend'
        QtPrivate::TypeAndForceComplete<StatsBackend *, std::false_type>,
        // method 'javaBackend'
        QtPrivate::TypeAndForceComplete<JavaBackend *, std::false_type>,
        // method 'userDataBackend'
        QtPrivate::TypeAndForceComplete<UserDataBackend *, std::false_type>,
        // method 'refreshGameStats'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'resolveIconUrl'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cacheIconBatchAsync'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        // method 'iconCachedPath'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'resolveShaderIconUrl'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cacheShaderIconBatchAsync'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        // method 'resolveRpIconUrl'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cacheRpIconBatchAsync'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::ShadowBackend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ShadowBackend *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->betaVerified(); break;
        case 1: _t->betaKeyInvalid((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->betaStatusChanged(); break;
        case 3: _t->updateCheckingChanged(); break;
        case 4: _t->updateStateChanged(); break;
        case 5: _t->toastMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->updateDownloadProgress((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[2]))); break;
        case 7: _t->updateChangelogAvailable((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 8: _t->accountChanged(); break;
        case 9: _t->microsoftLoginProgress((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 10: _t->microsoftLoginSuccess((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 11: _t->microsoftLoginFailed((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 12: _t->skinReady(); break;
        case 13: _t->offlineSkinReady(); break;
        case 14: _t->offlineHistoryChanged(); break;
        case 15: _t->javaPathChanged(); break;
        case 16: _t->javaReadyChanged(); break;
        case 17: _t->memorySettingsChanged(); break;
        case 18: _t->jvmArgsChanged(); break;
        case 19: _t->gameArgsChanged(); break;
        case 20: _t->highPerfGpuChanged(); break;
        case 21: _t->versionLaunchSettingsChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 22: _t->launchBlocked((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 23: _t->generalSettingsChanged(); break;
        case 24: _t->downloadSettingsChanged(); break;
        case 25: _t->isolationChanged(); break;
        case 26: _t->embeddedLoginChanged(); break;
        case 27: _t->versionListReady(); break;
        case 28: _t->versionDetailsReady(); break;
        case 29: _t->scanningChanged(); break;
        case 30: _t->installedVersionsChanged(); break;
        case 31: _t->activeVersionNamesChanged(); break;
        case 32: _t->selectedVersionChanged(); break;
        case 33: _t->selectedVersionClearedAfterDelete(); break;
        case 34: _t->currentVersionSummaryChanged(); break;
        case 35: _t->installStateChanged(); break;
        case 36: _t->installPhaseChanged(); break;
        case 37: _t->installFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 38: _t->installComplete((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 39: _t->launchProgressChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 40: _t->launchStateChanged(); break;
        case 41: _t->minecraftStarted(); break;
        case 42: _t->minecraftStopped(); break;
        case 43: _t->crashDetected((*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 44: _t->isRunningChanged(); break;
        case 45: _t->runningCountChanged(); break;
        case 46: _t->resourceDownloadStateChanged(); break;
        case 47: _t->resourcepackSearchCompleted((*reinterpret_cast< std::add_pointer_t<QVariantList>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 48: _t->resourcepackSearchFailed((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 49: _t->resourcepackDownloadFinished((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 50: _t->resourcepackVersionsLoaded((*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 51: _t->resourcepackVersionsPartial((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[3]))); break;
        case 52: _t->resourcepackVersionsProgress((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 53: _t->modVersionsLoaded((*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 54: _t->modVersionsPartial((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[3]))); break;
        case 55: _t->modVersionsProgress((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 56: _t->fabricApiVersionsReady((*reinterpret_cast< std::add_pointer_t<QVariantList>>(_a[1]))); break;
        case 57: _t->modFileDownloadStarted((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4]))); break;
        case 58: _t->modFileDownloadProgress((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[3]))); break;
        case 59: _t->modFileDownloadFinished((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4]))); break;
        case 60: _t->modFileDownloadFailed((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 61: _t->shaderVersionsLoaded((*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 62: _t->shaderVersionsPartial((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[3]))); break;
        case 63: _t->shaderVersionsProgress((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 64: _t->resourceDownloadProgress((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 65: _t->resourceDownloadDone((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 66: _t->verifyRunningChanged(); break;
        case 67: _t->repairRunningChanged(); break;
        case 68: _t->verifyCheckedChanged(); break;
        case 69: _t->verifyTotalChanged(); break;
        case 70: _t->verifyResultTextChanged(); break;
        case 71: _t->downloadQueueChanged(); break;
        case 72: _t->downloadQueueFull((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 73: _t->searchResultsReady((*reinterpret_cast< std::add_pointer_t<QVariantList>>(_a[1]))); break;
        case 74: _t->modSearchResultsReady((*reinterpret_cast< std::add_pointer_t<QVariantList>>(_a[1]))); break;
        case 75: _t->shaderSearchResultsReady((*reinterpret_cast< std::add_pointer_t<QVariantList>>(_a[1]))); break;
        case 76: _t->gameDirChanged(); break;
        case 77: _t->themeChanged(); break;
        case 78: _t->agreementAcceptedChanged(); break;
        case 79: _t->loginModeChanged(); break;
        case 80: _t->customBgChanged(); break;
        case 81: _t->logMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 82: _t->skinChanged(); break;
        case 83: _t->wardrobeBusyChanged(); break;
        case 84: _t->wardrobeError((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 85: _t->forgeVersionsReady((*reinterpret_cast< std::add_pointer_t<QVariantList>>(_a[1]))); break;
        case 86: _t->fabricVersionsReady((*reinterpret_cast< std::add_pointer_t<QVariantList>>(_a[1]))); break;
        case 87: _t->neoforgeVersionsReady((*reinterpret_cast< std::add_pointer_t<QVariantList>>(_a[1]))); break;
        case 88: _t->optifineVersionsReady((*reinterpret_cast< std::add_pointer_t<QVariantList>>(_a[1]))); break;
        case 89: _t->statsChanged(); break;
        case 90: _t->statsLoadingChanged(); break;
        case 91: _t->iconCached((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 92: _t->navigateToRequested((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 93: _t->openRpDetailRequested((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 94: _t->openModDetailRequested((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 95: _t->openShaderDetailRequested((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 96: _t->setRpShowPreReleases((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 97: _t->openRpVersionMenu(); break;
        case 98: _t->expandRpDetailGroup((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 99: _t->selectRpDetailSubVer((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 100: _t->verifyStarted(); break;
        case 101: _t->verifyProgress((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 102: _t->verifyFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 103: _t->verifyFailedFiles((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 104: _t->launchCheckProgress((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 105: _t->launchCheckFailed((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 106: _t->launchCheckMissingFiles((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 107: _t->launchCheckWarning((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 108: _t->setEmbeddedLoginEnabled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 109: _t->setCustomBgPath((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 110: _t->setSidebarOpacity((*reinterpret_cast< std::add_pointer_t<qreal>>(_a[1]))); break;
        case 111: _t->setContentOpacity((*reinterpret_cast< std::add_pointer_t<qreal>>(_a[1]))); break;
        case 112: _t->setCropX((*reinterpret_cast< std::add_pointer_t<qreal>>(_a[1]))); break;
        case 113: _t->setCropY((*reinterpret_cast< std::add_pointer_t<qreal>>(_a[1]))); break;
        case 114: _t->updateCrop((*reinterpret_cast< std::add_pointer_t<qreal>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<qreal>>(_a[2]))); break;
        case 115: { QString _r = _t->pickBackgroundImage();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 116: _t->switchLanguage((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 117: { int _r = _t->readLanguageFile();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 118: { QVariantList _r = _t->listMods((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 119: { QVariantList _r = _t->listMods();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 120: { QVariantList _r = _t->listResourcePacks((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 121: { QVariantList _r = _t->listResourcePacks();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 122: { QVariantList _r = _t->listSaves((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 123: { QVariantList _r = _t->listSaves();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 124: _t->offlineLogin((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 125: _t->updateOfflineSkin((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 126: _t->removeOfflineUsername((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 127: _t->microsoftLogin(); break;
        case 128: _t->cancelMicrosoftLogin(); break;
        case 129: _t->logout(); break;
        case 130: { QVariantList _r = _t->scanJavaInstallations();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 131: { QString _r = _t->autoSelectJava();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 132: { QString _r = _t->detectJava();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 133: { QString _r = _t->browseJava();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 134: _t->selectJavaByIndex((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 135: { QVariantMap _r = _t->getMemoryStatus();
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 136: _t->setMinMemory((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 137: _t->setMaxMemory((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 138: _t->setIsolationEnabled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 139: { QString _r = _t->getVersionGameDir((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 140: _t->migrateVersionToIsolated((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 141: { bool _r = _t->openGameDir((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 142: { bool _r = _t->openGameDir();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 143: { bool _r = _t->openLatestLog((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 144: { bool _r = _t->openLatestLog();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 145: { bool _r = _t->openLogsFolder((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 146: { bool _r = _t->openLogsFolder();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 147: { bool _r = _t->openLauncherLogsFolder();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 148: { bool _r = _t->openCrashLog((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 149: { bool _r = _t->openCrashLog();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 150: { bool _r = _t->openSavesFolder((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 151: { bool _r = _t->openSavesFolder();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 152: { bool _r = _t->openScreenshotsFolder((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 153: { bool _r = _t->openScreenshotsFolder();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 154: { bool _r = _t->openModsFolder((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 155: { bool _r = _t->openModsFolder();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 156: { bool _r = _t->openResourcePacksFolder((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 157: { bool _r = _t->openResourcePacksFolder();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 158: { bool _r = _t->openShaderPacksFolder((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 159: { bool _r = _t->openShaderPacksFolder();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 160: _t->openVersionDir((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 161: _t->deleteVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 162: _t->refreshVersionList(); break;
        case 163: _t->refreshInstalled(); break;
        case 164: _t->refreshInstalledList(); break;
        case 165: _t->refreshVersionDetails(); break;
        case 166: _t->refreshGameDirInfo(); break;
        case 167: _t->installVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 168: _t->cancelInstall(); break;
        case 169: _t->cancelVersionInstall((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 170: _t->dismissCard((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 171: _t->launch((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 172: _t->cancelLaunch(); break;
        case 173: _t->killGameProcess(); break;
        case 174: _t->killMinecraft(); break;
        case 175: _t->killGameByPid((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1]))); break;
        case 176: { QVariantList _r = _t->runningGames();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 177: { QVariantList _r = _t->getPopularMods((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 178: { QVariantList _r = _t->getShaderList();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 179: _t->searchMods((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 180: _t->searchMods((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 181: _t->searchModsEx((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[6])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[7])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[8]))); break;
        case 182: { QVariantMap _r = _t->getModCategories();
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 183: _t->searchShadersEx((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[6])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[7]))); break;
        case 184: _t->downloadMod((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 185: _t->downloadMod((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 186: _t->downloadShader((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 187: _t->downloadShader((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 188: _t->searchResourcepacks((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[4]))); break;
        case 189: _t->searchResourcepacks((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 190: _t->searchResourcepacks((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 191: _t->searchResourcepacks((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 192: _t->downloadResourcepack((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 193: _t->downloadResourcepack((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 194: _t->fetchResourcepackVersions((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 195: _t->fetchModVersions((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 196: _t->fetchShaderVersions((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 197: { int _r = _t->downloadModFile((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[6])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[7])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 198: { int _r = _t->downloadModFile((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[6])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 199: { int _r = _t->downloadModFile((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 200: _t->cancelModFileDownload((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 201: _t->pauseModFileDownload((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 202: _t->resumeModFileDownload((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 203: _t->retryModFileDownload((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 204: _t->browseSkin(); break;
        case 205: _t->uploadSkin((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 206: _t->saveWardrobeSettings((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 207: _t->saveSkinToFile(); break;
        case 208: { QVariantList _r = _t->availableCapes();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 209: { QString _r = _t->loginType();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 210: _t->cacheIconAsync((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 211: { QString _r = _t->cachedIconPath((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 212: _t->queryForgeVersions((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 213: _t->queryFabricVersions((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 214: _t->queryNeoForgeVersions((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 215: _t->queryOptifineVersions((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 216: _t->queryFabricApiVersions((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 217: _t->cancelModLoaderQueries(); break;
        case 218: _t->cacheForgeInstallerSha1((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 219: { bool _r = _t->installFabricApi((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 220: _t->installModLoader((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[6])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[7])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[8]))); break;
        case 221: _t->installModLoader((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[6])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[7]))); break;
        case 222: _t->installModLoader((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[6]))); break;
        case 223: _t->installModLoader((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5]))); break;
        case 224: _t->installModLoader((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4]))); break;
        case 225: _t->installOptifine((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[6]))); break;
        case 226: _t->installOptifine((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5]))); break;
        case 227: _t->installOptifine((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4]))); break;
        case 228: _t->setSelectedVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 229: _t->setTheme((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 230: { QVariantMap _r = _t->checkAll((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 231: _t->setGameDir((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 232: { int _r = _t->getAutoMemory();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 233: _t->setAutoMemoryEnabled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 234: { int _r = _t->versionMemoryMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 235: _t->setVersionMemoryMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 236: { int _r = _t->versionMemoryManualMB((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 237: _t->setVersionMemoryManualMB((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 238: { int _r = _t->resolvedMemoryMB((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 239: { int _r = _t->versionJavaMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 240: _t->setVersionJavaMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 241: { int _r = _t->versionJvmArgsMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 242: _t->setVersionJvmArgsMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 243: { QString _r = _t->versionJvmArgs((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 244: _t->setVersionJvmArgs((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 245: { QString _r = _t->resolvedJvmArgs((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 246: { int _r = _t->versionGameArgsMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 247: _t->setVersionGameArgsMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 248: { QString _r = _t->versionGameArgs((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 249: _t->setVersionGameArgs((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 250: { QString _r = _t->resolvedGameArgs((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 251: { int _r = _t->versionHighPerfGpuMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 252: _t->setVersionHighPerfGpuMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 253: { bool _r = _t->versionHighPerfGpu((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 254: _t->setVersionHighPerfGpu((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 255: { bool _r = _t->resolvedHighPerfGpu((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 256: _t->setJvmArgs((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 257: _t->setGameArgs((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 258: _t->setHighPerfGpu((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 259: _t->copyToClipboard((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 260: { bool _r = _t->isModdedVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 261: _t->openJavaFileDialog(); break;
        case 262: _t->pickJava(); break;
        case 263: _t->checkFileChanges(); break;
        case 264: _t->deleteMod((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 265: _t->deleteMod((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 266: _t->deleteResourcePack((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 267: _t->deleteResourcePack((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 268: { bool _r = _t->importMod((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 269: { bool _r = _t->importMod((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 270: _t->deleteSave((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 271: _t->deleteSave((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 272: _t->migrateVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 273: _t->openConfigFolder(); break;
        case 274: _t->removeGameDir((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 275: _t->cancelQueuedDownload((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 276: _t->verifyVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 277: _t->cleanCorruptVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 278: { bool _r = _t->renameVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 279: { bool _r = _t->renameVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 280: { bool _r = _t->cloneVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 281: { bool _r = _t->cloneVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 282: { QString _r = _t->copyVersionPath((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 283: _t->repairVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 284: _t->cancelVerify(); break;
        case 285: _t->openVerifyReport(); break;
        case 286: _t->submitBetaKey((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 287: _t->checkForUpdate(); break;
        case 288: { SettingsBackend* _r = _t->settings();
            if (_a[0]) *reinterpret_cast< SettingsBackend**>(_a[0]) = std::move(_r); }  break;
        case 289: { int _r = _t->diagAutoLangComboIdx();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 290: _t->setAutoLangModeFromCombo((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 291: _t->logUiMsg((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 292: { StatsBackend* _r = _t->statsBackend();
            if (_a[0]) *reinterpret_cast< StatsBackend**>(_a[0]) = std::move(_r); }  break;
        case 293: { JavaBackend* _r = _t->javaBackend();
            if (_a[0]) *reinterpret_cast< JavaBackend**>(_a[0]) = std::move(_r); }  break;
        case 294: { UserDataBackend* _r = _t->userDataBackend();
            if (_a[0]) *reinterpret_cast< UserDataBackend**>(_a[0]) = std::move(_r); }  break;
        case 295: _t->refreshGameStats(); break;
        case 296: { QString _r = _t->resolveIconUrl((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 297: _t->cacheIconBatchAsync((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 298: { QString _r = _t->iconCachedPath((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 299: { QString _r = _t->resolveShaderIconUrl((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 300: _t->cacheShaderIconBatchAsync((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 301: { QString _r = _t->resolveRpIconUrl((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 302: _t->cacheRpIconBatchAsync((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::betaVerified; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & );
            if (_q_method_type _q_method = &ShadowBackend::betaKeyInvalid; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::betaStatusChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::updateCheckingChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::updateStateChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & );
            if (_q_method_type _q_method = &ShadowBackend::toastMessage; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(qint64 , qint64 );
            if (_q_method_type _q_method = &ShadowBackend::updateDownloadProgress; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & , const QString & );
            if (_q_method_type _q_method = &ShadowBackend::updateChangelogAvailable; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::accountChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 8;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & , const QString & );
            if (_q_method_type _q_method = &ShadowBackend::microsoftLoginProgress; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 9;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & , const QString & );
            if (_q_method_type _q_method = &ShadowBackend::microsoftLoginSuccess; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 10;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & );
            if (_q_method_type _q_method = &ShadowBackend::microsoftLoginFailed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 11;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::skinReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 12;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::offlineSkinReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 13;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::offlineHistoryChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 14;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::javaPathChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 15;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::javaReadyChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 16;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::memorySettingsChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 17;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::jvmArgsChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 18;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::gameArgsChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 19;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::highPerfGpuChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 20;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & );
            if (_q_method_type _q_method = &ShadowBackend::versionLaunchSettingsChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 21;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & );
            if (_q_method_type _q_method = &ShadowBackend::launchBlocked; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 22;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::generalSettingsChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 23;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::downloadSettingsChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 24;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::isolationChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 25;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::embeddedLoginChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 26;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::versionListReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 27;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::versionDetailsReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 28;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::scanningChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 29;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::installedVersionsChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 30;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::activeVersionNamesChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 31;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::selectedVersionChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 32;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::selectedVersionClearedAfterDelete; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 33;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::currentVersionSummaryChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 34;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::installStateChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 35;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::installPhaseChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 36;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(bool );
            if (_q_method_type _q_method = &ShadowBackend::installFinished; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 37;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & );
            if (_q_method_type _q_method = &ShadowBackend::installComplete; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 38;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(int , const QString & );
            if (_q_method_type _q_method = &ShadowBackend::launchProgressChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 39;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::launchStateChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 40;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::minecraftStarted; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 41;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::minecraftStopped; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 42;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QVariantMap & );
            if (_q_method_type _q_method = &ShadowBackend::crashDetected; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 43;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::isRunningChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 44;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::runningCountChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 45;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::resourceDownloadStateChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 46;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QVariantList & , int );
            if (_q_method_type _q_method = &ShadowBackend::resourcepackSearchCompleted; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 47;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & );
            if (_q_method_type _q_method = &ShadowBackend::resourcepackSearchFailed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 48;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & , bool , const QString & );
            if (_q_method_type _q_method = &ShadowBackend::resourcepackDownloadFinished; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 49;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QVariantMap & );
            if (_q_method_type _q_method = &ShadowBackend::resourcepackVersionsLoaded; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 50;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & , const QStringList & , const QVariantMap & );
            if (_q_method_type _q_method = &ShadowBackend::resourcepackVersionsPartial; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 51;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(int , int );
            if (_q_method_type _q_method = &ShadowBackend::resourcepackVersionsProgress; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 52;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QVariantMap & );
            if (_q_method_type _q_method = &ShadowBackend::modVersionsLoaded; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 53;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & , const QStringList & , const QVariantMap & );
            if (_q_method_type _q_method = &ShadowBackend::modVersionsPartial; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 54;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(int , int );
            if (_q_method_type _q_method = &ShadowBackend::modVersionsProgress; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 55;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QVariantList & );
            if (_q_method_type _q_method = &ShadowBackend::fabricApiVersionsReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 56;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(int , const QString & , qint64 , const QString & );
            if (_q_method_type _q_method = &ShadowBackend::modFileDownloadStarted; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 57;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(int , qint64 , qint64 );
            if (_q_method_type _q_method = &ShadowBackend::modFileDownloadProgress; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 58;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(int , bool , const QString & , const QString & );
            if (_q_method_type _q_method = &ShadowBackend::modFileDownloadFinished; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 59;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(int , const QString & , const QString & );
            if (_q_method_type _q_method = &ShadowBackend::modFileDownloadFailed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 60;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QVariantMap & );
            if (_q_method_type _q_method = &ShadowBackend::shaderVersionsLoaded; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 61;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & , const QStringList & , const QVariantMap & );
            if (_q_method_type _q_method = &ShadowBackend::shaderVersionsPartial; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 62;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(int , int );
            if (_q_method_type _q_method = &ShadowBackend::shaderVersionsProgress; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 63;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(int , int , const QString & );
            if (_q_method_type _q_method = &ShadowBackend::resourceDownloadProgress; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 64;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(bool );
            if (_q_method_type _q_method = &ShadowBackend::resourceDownloadDone; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 65;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::verifyRunningChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 66;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::repairRunningChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 67;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::verifyCheckedChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 68;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::verifyTotalChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 69;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::verifyResultTextChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 70;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::downloadQueueChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 71;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & );
            if (_q_method_type _q_method = &ShadowBackend::downloadQueueFull; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 72;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QVariantList & );
            if (_q_method_type _q_method = &ShadowBackend::searchResultsReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 73;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QVariantList & );
            if (_q_method_type _q_method = &ShadowBackend::modSearchResultsReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 74;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QVariantList & );
            if (_q_method_type _q_method = &ShadowBackend::shaderSearchResultsReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 75;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::gameDirChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 76;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::themeChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 77;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::agreementAcceptedChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 78;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::loginModeChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 79;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::customBgChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 80;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & );
            if (_q_method_type _q_method = &ShadowBackend::logMessage; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 81;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::skinChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 82;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::wardrobeBusyChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 83;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & );
            if (_q_method_type _q_method = &ShadowBackend::wardrobeError; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 84;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QVariantList & );
            if (_q_method_type _q_method = &ShadowBackend::forgeVersionsReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 85;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QVariantList & );
            if (_q_method_type _q_method = &ShadowBackend::fabricVersionsReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 86;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QVariantList & );
            if (_q_method_type _q_method = &ShadowBackend::neoforgeVersionsReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 87;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QVariantList & );
            if (_q_method_type _q_method = &ShadowBackend::optifineVersionsReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 88;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::statsChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 89;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::statsLoadingChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 90;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & , const QString & );
            if (_q_method_type _q_method = &ShadowBackend::iconCached; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 91;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(int , int );
            if (_q_method_type _q_method = &ShadowBackend::navigateToRequested; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 92;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & );
            if (_q_method_type _q_method = &ShadowBackend::openRpDetailRequested; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 93;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & );
            if (_q_method_type _q_method = &ShadowBackend::openModDetailRequested; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 94;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & );
            if (_q_method_type _q_method = &ShadowBackend::openShaderDetailRequested; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 95;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(bool );
            if (_q_method_type _q_method = &ShadowBackend::setRpShowPreReleases; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 96;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::openRpVersionMenu; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 97;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & );
            if (_q_method_type _q_method = &ShadowBackend::expandRpDetailGroup; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 98;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & );
            if (_q_method_type _q_method = &ShadowBackend::selectRpDetailSubVer; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 99;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)();
            if (_q_method_type _q_method = &ShadowBackend::verifyStarted; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 100;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(int , int );
            if (_q_method_type _q_method = &ShadowBackend::verifyProgress; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 101;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(bool );
            if (_q_method_type _q_method = &ShadowBackend::verifyFinished; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 102;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QStringList & );
            if (_q_method_type _q_method = &ShadowBackend::verifyFailedFiles; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 103;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & );
            if (_q_method_type _q_method = &ShadowBackend::launchCheckProgress; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 104;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & , const QString & );
            if (_q_method_type _q_method = &ShadowBackend::launchCheckFailed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 105;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QStringList & );
            if (_q_method_type _q_method = &ShadowBackend::launchCheckMissingFiles; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 106;
                return;
            }
        }
        {
            using _q_method_type = void (ShadowBackend::*)(const QString & );
            if (_q_method_type _q_method = &ShadowBackend::launchCheckWarning; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 107;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QObject**>(_v) = _t->account(); break;
        case 1: *reinterpret_cast< QObject**>(_v) = _t->yggdrasil(); break;
        case 2: *reinterpret_cast< QString*>(_v) = _t->username(); break;
        case 3: *reinterpret_cast< QString*>(_v) = _t->offlineUsername(); break;
        case 4: *reinterpret_cast< bool*>(_v) = _t->isOnline(); break;
        case 5: *reinterpret_cast< QString*>(_v) = _t->accountUuid(); break;
        case 6: *reinterpret_cast< QString*>(_v) = _t->offlineUuid(); break;
        case 7: *reinterpret_cast< QString*>(_v) = _t->skinPath(); break;
        case 8: *reinterpret_cast< QString*>(_v) = _t->offlineSkinPath(); break;
        case 9: *reinterpret_cast< QStringList*>(_v) = _t->offlineUsernames(); break;
        case 10: *reinterpret_cast< int*>(_v) = _t->lastLoginMode(); break;
        case 11: *reinterpret_cast< QString*>(_v) = _t->javaPath(); break;
        case 12: *reinterpret_cast< QString*>(_v) = _t->javaVersion(); break;
        case 13: *reinterpret_cast< int*>(_v) = _t->javaMajor(); break;
        case 14: *reinterpret_cast< bool*>(_v) = _t->javaInstalled(); break;
        case 15: *reinterpret_cast< bool*>(_v) = _t->javaInstalled(); break;
        case 16: *reinterpret_cast< int*>(_v) = _t->minMemoryMb(); break;
        case 17: *reinterpret_cast< int*>(_v) = _t->maxMemoryMb(); break;
        case 18: *reinterpret_cast< bool*>(_v) = _t->isolationEnabled(); break;
        case 19: *reinterpret_cast< bool*>(_v) = _t->embeddedLoginEnabled(); break;
        case 20: *reinterpret_cast< QVariantList*>(_v) = _t->availableJavaList(); break;
        case 21: *reinterpret_cast< QString*>(_v) = _t->selectedVersion(); break;
        case 22: *reinterpret_cast< QStringList*>(_v) = _t->versionIds(); break;
        case 23: *reinterpret_cast< QVariantList*>(_v) = _t->versionList(); break;
        case 24: *reinterpret_cast< QStringList*>(_v) = _t->releaseVersions(); break;
        case 25: *reinterpret_cast< QStringList*>(_v) = _t->snapshotVersions(); break;
        case 26: *reinterpret_cast< QStringList*>(_v) = _t->oldVersions(); break;
        case 27: *reinterpret_cast< QStringList*>(_v) = _t->aprilFoolVersions(); break;
        case 28: *reinterpret_cast< QStringList*>(_v) = _t->installedVersions(); break;
        case 29: *reinterpret_cast< QStringList*>(_v) = _t->activeVersionNames(); break;
        case 30: *reinterpret_cast< bool*>(_v) = _t->isInstalling(); break;
        case 31: *reinterpret_cast< QString*>(_v) = _t->installVersionId(); break;
        case 32: *reinterpret_cast< QString*>(_v) = _t->installPhase(); break;
        case 33: *reinterpret_cast< bool*>(_v) = _t->isLaunching(); break;
        case 34: *reinterpret_cast< QVariantMap*>(_v) = _t->lastCrash(); break;
        case 35: *reinterpret_cast< QString*>(_v) = _t->launchStatus(); break;
        case 36: *reinterpret_cast< bool*>(_v) = _t->isRunning(); break;
        case 37: *reinterpret_cast< int*>(_v) = _t->runningCount(); break;
        case 38: *reinterpret_cast< QString*>(_v) = _t->launchVersion(); break;
        case 39: *reinterpret_cast< QString*>(_v) = _t->launchUsername(); break;
        case 40: *reinterpret_cast< bool*>(_v) = _t->isResourceDownloading(); break;
        case 41: *reinterpret_cast< int*>(_v) = _t->resourceDownloadProgress(); break;
        case 42: *reinterpret_cast< int*>(_v) = _t->resourceDownloadTotal(); break;
        case 43: *reinterpret_cast< int*>(_v) = _t->resourceDownloadSpeed(); break;
        case 44: *reinterpret_cast< QString*>(_v) = _t->resourceDownloadFile(); break;
        case 45: *reinterpret_cast< QObject**>(_v) = _t->modManager(); break;
        case 46: *reinterpret_cast< QObject**>(_v) = _t->multiplayer(); break;
        case 47: *reinterpret_cast< QString*>(_v) = _t->gameDir(); break;
        case 48: *reinterpret_cast< QString*>(_v) = _t->appDataDir(); break;
        case 49: *reinterpret_cast< QString*>(_v) = _t->appVersion(); break;
        case 50: *reinterpret_cast< QString*>(_v) = _t->theme(); break;
        case 51: *reinterpret_cast< bool*>(_v) = _t->devMode(); break;
        case 52: *reinterpret_cast< QObject**>(_v) = _t->modpackImporter(); break;
        case 53: *reinterpret_cast< bool*>(_v) = _t->updateChecking(); break;
        case 54: *reinterpret_cast< int*>(_v) = _t->updateState(); break;
        case 55: *reinterpret_cast< bool*>(_v) = _t->agreementAccepted(); break;
        case 56: *reinterpret_cast< QString*>(_v) = _t->betaAgreementHtml(); break;
        case 57: *reinterpret_cast< QString*>(_v) = _t->privacyAgreementHtml(); break;
        case 58: *reinterpret_cast< QString*>(_v) = _t->termsAgreementHtml(); break;
        case 59: *reinterpret_cast< bool*>(_v) = _t->isMarkAgreed(); break;
        case 60: *reinterpret_cast< QString*>(_v) = _t->customBgPath(); break;
        case 61: *reinterpret_cast< qreal*>(_v) = _t->sidebarOpacity(); break;
        case 62: *reinterpret_cast< qreal*>(_v) = _t->contentOpacity(); break;
        case 63: *reinterpret_cast< qreal*>(_v) = _t->cropX(); break;
        case 64: *reinterpret_cast< qreal*>(_v) = _t->cropY(); break;
        case 65: *reinterpret_cast< QVariantList*>(_v) = _t->versionDetails(); break;
        case 66: *reinterpret_cast< bool*>(_v) = _t->isScanningVersions(); break;
        case 67: *reinterpret_cast< QVariantMap*>(_v) = _t->currentVersionSummary(); break;
        case 68: *reinterpret_cast< QVariantList*>(_v) = _t->downloadQueue(); break;
        case 69: *reinterpret_cast< QVariantList*>(_v) = _t->activeDownloads(); break;
        case 70: *reinterpret_cast< QVariantMap*>(_v) = _t->gameDirInfo(); break;
        case 71: *reinterpret_cast< QVariantList*>(_v) = _t->gameDirectories(); break;
        case 72: *reinterpret_cast< qint64*>(_v) = _t->diskFree(); break;
        case 73: *reinterpret_cast< int*>(_v) = _t->diskPercent(); break;
        case 74: *reinterpret_cast< bool*>(_v) = _t->autoMemoryEnabled(); break;
        case 75: *reinterpret_cast< QVariantMap*>(_v) = _t->systemMemoryInfo(); break;
        case 76: *reinterpret_cast< QVariantList*>(_v) = _t->availableCapes(); break;
        case 77: *reinterpret_cast< QString*>(_v) = _t->loginType(); break;
        case 78: *reinterpret_cast< QString*>(_v) = _t->selectedSkinPath(); break;
        case 79: *reinterpret_cast< bool*>(_v) = _t->isWardrobeBusy(); break;
        case 80: *reinterpret_cast< int*>(_v) = _t->fileDownloadSource(); break;
        case 81: *reinterpret_cast< int*>(_v) = _t->listDownloadSource(); break;
        case 82: *reinterpret_cast< int*>(_v) = _t->maxDownloadThreads(); break;
        case 83: *reinterpret_cast< double*>(_v) = _t->downloadSpeedLimitMB(); break;
        case 84: *reinterpret_cast< QString*>(_v) = _t->jvmArgs(); break;
        case 85: *reinterpret_cast< QString*>(_v) = _t->gameArgs(); break;
        case 86: *reinterpret_cast< bool*>(_v) = _t->highPerfGpu(); break;
        case 87: *reinterpret_cast< bool*>(_v) = _t->verifyRunning(); break;
        case 88: *reinterpret_cast< bool*>(_v) = _t->repairRunning(); break;
        case 89: *reinterpret_cast< int*>(_v) = _t->verifyChecked(); break;
        case 90: *reinterpret_cast< int*>(_v) = _t->verifyTotal(); break;
        case 91: *reinterpret_cast< bool*>(_v) = _t->verifyResultOk(); break;
        case 92: *reinterpret_cast< QString*>(_v) = _t->verifyResultText(); break;
        case 93: *reinterpret_cast< QObject**>(_v) = _t->installCardsModel(); break;
        case 94: *reinterpret_cast< QString*>(_v) = _t->betaStatus(); break;
        case 95: *reinterpret_cast< double*>(_v) = _t->totalGameHours(); break;
        case 96: *reinterpret_cast< QVariantList*>(_v) = _t->versionGameStats(); break;
        case 97: *reinterpret_cast< bool*>(_v) = _t->statsLoading(); break;
        case 98: *reinterpret_cast< bool*>(_v) = _t->statsEmpty(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 10: _t->setLastLoginMode(*reinterpret_cast< int*>(_v)); break;
        case 19: _t->setEmbeddedLoginEnabled(*reinterpret_cast< bool*>(_v)); break;
        case 59: _t->setMarkAgreed(*reinterpret_cast< bool*>(_v)); break;
        case 63: _t->setCropX(*reinterpret_cast< qreal*>(_v)); break;
        case 64: _t->setCropY(*reinterpret_cast< qreal*>(_v)); break;
        case 80: _t->setFileDownloadSource(*reinterpret_cast< int*>(_v)); break;
        case 81: _t->setListDownloadSource(*reinterpret_cast< int*>(_v)); break;
        case 82: _t->setMaxDownloadThreads(*reinterpret_cast< int*>(_v)); break;
        case 83: _t->setDownloadSpeedLimitMB(*reinterpret_cast< double*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *ShadowLauncher::ShadowBackend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::ShadowBackend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN14ShadowLauncher13ShadowBackendE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::ShadowBackend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 303)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 303;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 303)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 303;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 99;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::ShadowBackend::betaVerified()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ShadowLauncher::ShadowBackend::betaKeyInvalid(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void ShadowLauncher::ShadowBackend::betaStatusChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void ShadowLauncher::ShadowBackend::updateCheckingChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void ShadowLauncher::ShadowBackend::updateStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void ShadowLauncher::ShadowBackend::toastMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void ShadowLauncher::ShadowBackend::updateDownloadProgress(qint64 _t1, qint64 _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void ShadowLauncher::ShadowBackend::updateChangelogAvailable(const QString & _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 8
void ShadowLauncher::ShadowBackend::accountChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void ShadowLauncher::ShadowBackend::microsoftLoginProgress(const QString & _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 9, _a);
}

// SIGNAL 10
void ShadowLauncher::ShadowBackend::microsoftLoginSuccess(const QString & _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 10, _a);
}

// SIGNAL 11
void ShadowLauncher::ShadowBackend::microsoftLoginFailed(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 11, _a);
}

// SIGNAL 12
void ShadowLauncher::ShadowBackend::skinReady()
{
    QMetaObject::activate(this, &staticMetaObject, 12, nullptr);
}

// SIGNAL 13
void ShadowLauncher::ShadowBackend::offlineSkinReady()
{
    QMetaObject::activate(this, &staticMetaObject, 13, nullptr);
}

// SIGNAL 14
void ShadowLauncher::ShadowBackend::offlineHistoryChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 14, nullptr);
}

// SIGNAL 15
void ShadowLauncher::ShadowBackend::javaPathChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 15, nullptr);
}

// SIGNAL 16
void ShadowLauncher::ShadowBackend::javaReadyChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 16, nullptr);
}

// SIGNAL 17
void ShadowLauncher::ShadowBackend::memorySettingsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 17, nullptr);
}

// SIGNAL 18
void ShadowLauncher::ShadowBackend::jvmArgsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 18, nullptr);
}

// SIGNAL 19
void ShadowLauncher::ShadowBackend::gameArgsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 19, nullptr);
}

// SIGNAL 20
void ShadowLauncher::ShadowBackend::highPerfGpuChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 20, nullptr);
}

// SIGNAL 21
void ShadowLauncher::ShadowBackend::versionLaunchSettingsChanged(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 21, _a);
}

// SIGNAL 22
void ShadowLauncher::ShadowBackend::launchBlocked(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 22, _a);
}

// SIGNAL 23
void ShadowLauncher::ShadowBackend::generalSettingsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 23, nullptr);
}

// SIGNAL 24
void ShadowLauncher::ShadowBackend::downloadSettingsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 24, nullptr);
}

// SIGNAL 25
void ShadowLauncher::ShadowBackend::isolationChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 25, nullptr);
}

// SIGNAL 26
void ShadowLauncher::ShadowBackend::embeddedLoginChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 26, nullptr);
}

// SIGNAL 27
void ShadowLauncher::ShadowBackend::versionListReady()
{
    QMetaObject::activate(this, &staticMetaObject, 27, nullptr);
}

// SIGNAL 28
void ShadowLauncher::ShadowBackend::versionDetailsReady()
{
    QMetaObject::activate(this, &staticMetaObject, 28, nullptr);
}

// SIGNAL 29
void ShadowLauncher::ShadowBackend::scanningChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 29, nullptr);
}

// SIGNAL 30
void ShadowLauncher::ShadowBackend::installedVersionsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 30, nullptr);
}

// SIGNAL 31
void ShadowLauncher::ShadowBackend::activeVersionNamesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 31, nullptr);
}

// SIGNAL 32
void ShadowLauncher::ShadowBackend::selectedVersionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 32, nullptr);
}

// SIGNAL 33
void ShadowLauncher::ShadowBackend::selectedVersionClearedAfterDelete()
{
    QMetaObject::activate(this, &staticMetaObject, 33, nullptr);
}

// SIGNAL 34
void ShadowLauncher::ShadowBackend::currentVersionSummaryChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 34, nullptr);
}

// SIGNAL 35
void ShadowLauncher::ShadowBackend::installStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 35, nullptr);
}

// SIGNAL 36
void ShadowLauncher::ShadowBackend::installPhaseChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 36, nullptr);
}

// SIGNAL 37
void ShadowLauncher::ShadowBackend::installFinished(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 37, _a);
}

// SIGNAL 38
void ShadowLauncher::ShadowBackend::installComplete(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 38, _a);
}

// SIGNAL 39
void ShadowLauncher::ShadowBackend::launchProgressChanged(int _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 39, _a);
}

// SIGNAL 40
void ShadowLauncher::ShadowBackend::launchStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 40, nullptr);
}

// SIGNAL 41
void ShadowLauncher::ShadowBackend::minecraftStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 41, nullptr);
}

// SIGNAL 42
void ShadowLauncher::ShadowBackend::minecraftStopped()
{
    QMetaObject::activate(this, &staticMetaObject, 42, nullptr);
}

// SIGNAL 43
void ShadowLauncher::ShadowBackend::crashDetected(const QVariantMap & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 43, _a);
}

// SIGNAL 44
void ShadowLauncher::ShadowBackend::isRunningChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 44, nullptr);
}

// SIGNAL 45
void ShadowLauncher::ShadowBackend::runningCountChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 45, nullptr);
}

// SIGNAL 46
void ShadowLauncher::ShadowBackend::resourceDownloadStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 46, nullptr);
}

// SIGNAL 47
void ShadowLauncher::ShadowBackend::resourcepackSearchCompleted(const QVariantList & _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 47, _a);
}

// SIGNAL 48
void ShadowLauncher::ShadowBackend::resourcepackSearchFailed(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 48, _a);
}

// SIGNAL 49
void ShadowLauncher::ShadowBackend::resourcepackDownloadFinished(const QString & _t1, bool _t2, const QString & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 49, _a);
}

// SIGNAL 50
void ShadowLauncher::ShadowBackend::resourcepackVersionsLoaded(const QVariantMap & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 50, _a);
}

// SIGNAL 51
void ShadowLauncher::ShadowBackend::resourcepackVersionsPartial(const QString & _t1, const QStringList & _t2, const QVariantMap & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 51, _a);
}

// SIGNAL 52
void ShadowLauncher::ShadowBackend::resourcepackVersionsProgress(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 52, _a);
}

// SIGNAL 53
void ShadowLauncher::ShadowBackend::modVersionsLoaded(const QVariantMap & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 53, _a);
}

// SIGNAL 54
void ShadowLauncher::ShadowBackend::modVersionsPartial(const QString & _t1, const QStringList & _t2, const QVariantMap & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 54, _a);
}

// SIGNAL 55
void ShadowLauncher::ShadowBackend::modVersionsProgress(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 55, _a);
}

// SIGNAL 56
void ShadowLauncher::ShadowBackend::fabricApiVersionsReady(const QVariantList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 56, _a);
}

// SIGNAL 57
void ShadowLauncher::ShadowBackend::modFileDownloadStarted(int _t1, const QString & _t2, qint64 _t3, const QString & _t4)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))) };
    QMetaObject::activate(this, &staticMetaObject, 57, _a);
}

// SIGNAL 58
void ShadowLauncher::ShadowBackend::modFileDownloadProgress(int _t1, qint64 _t2, qint64 _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 58, _a);
}

// SIGNAL 59
void ShadowLauncher::ShadowBackend::modFileDownloadFinished(int _t1, bool _t2, const QString & _t3, const QString & _t4)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))) };
    QMetaObject::activate(this, &staticMetaObject, 59, _a);
}

// SIGNAL 60
void ShadowLauncher::ShadowBackend::modFileDownloadFailed(int _t1, const QString & _t2, const QString & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 60, _a);
}

// SIGNAL 61
void ShadowLauncher::ShadowBackend::shaderVersionsLoaded(const QVariantMap & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 61, _a);
}

// SIGNAL 62
void ShadowLauncher::ShadowBackend::shaderVersionsPartial(const QString & _t1, const QStringList & _t2, const QVariantMap & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 62, _a);
}

// SIGNAL 63
void ShadowLauncher::ShadowBackend::shaderVersionsProgress(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 63, _a);
}

// SIGNAL 64
void ShadowLauncher::ShadowBackend::resourceDownloadProgress(int _t1, int _t2, const QString & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 64, _a);
}

// SIGNAL 65
void ShadowLauncher::ShadowBackend::resourceDownloadDone(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 65, _a);
}

// SIGNAL 66
void ShadowLauncher::ShadowBackend::verifyRunningChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 66, nullptr);
}

// SIGNAL 67
void ShadowLauncher::ShadowBackend::repairRunningChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 67, nullptr);
}

// SIGNAL 68
void ShadowLauncher::ShadowBackend::verifyCheckedChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 68, nullptr);
}

// SIGNAL 69
void ShadowLauncher::ShadowBackend::verifyTotalChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 69, nullptr);
}

// SIGNAL 70
void ShadowLauncher::ShadowBackend::verifyResultTextChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 70, nullptr);
}

// SIGNAL 71
void ShadowLauncher::ShadowBackend::downloadQueueChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 71, nullptr);
}

// SIGNAL 72
void ShadowLauncher::ShadowBackend::downloadQueueFull(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 72, _a);
}

// SIGNAL 73
void ShadowLauncher::ShadowBackend::searchResultsReady(const QVariantList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 73, _a);
}

// SIGNAL 74
void ShadowLauncher::ShadowBackend::modSearchResultsReady(const QVariantList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 74, _a);
}

// SIGNAL 75
void ShadowLauncher::ShadowBackend::shaderSearchResultsReady(const QVariantList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 75, _a);
}

// SIGNAL 76
void ShadowLauncher::ShadowBackend::gameDirChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 76, nullptr);
}

// SIGNAL 77
void ShadowLauncher::ShadowBackend::themeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 77, nullptr);
}

// SIGNAL 78
void ShadowLauncher::ShadowBackend::agreementAcceptedChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 78, nullptr);
}

// SIGNAL 79
void ShadowLauncher::ShadowBackend::loginModeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 79, nullptr);
}

// SIGNAL 80
void ShadowLauncher::ShadowBackend::customBgChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 80, nullptr);
}

// SIGNAL 81
void ShadowLauncher::ShadowBackend::logMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 81, _a);
}

// SIGNAL 82
void ShadowLauncher::ShadowBackend::skinChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 82, nullptr);
}

// SIGNAL 83
void ShadowLauncher::ShadowBackend::wardrobeBusyChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 83, nullptr);
}

// SIGNAL 84
void ShadowLauncher::ShadowBackend::wardrobeError(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 84, _a);
}

// SIGNAL 85
void ShadowLauncher::ShadowBackend::forgeVersionsReady(const QVariantList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 85, _a);
}

// SIGNAL 86
void ShadowLauncher::ShadowBackend::fabricVersionsReady(const QVariantList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 86, _a);
}

// SIGNAL 87
void ShadowLauncher::ShadowBackend::neoforgeVersionsReady(const QVariantList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 87, _a);
}

// SIGNAL 88
void ShadowLauncher::ShadowBackend::optifineVersionsReady(const QVariantList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 88, _a);
}

// SIGNAL 89
void ShadowLauncher::ShadowBackend::statsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 89, nullptr);
}

// SIGNAL 90
void ShadowLauncher::ShadowBackend::statsLoadingChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 90, nullptr);
}

// SIGNAL 91
void ShadowLauncher::ShadowBackend::iconCached(const QString & _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 91, _a);
}

// SIGNAL 92
void ShadowLauncher::ShadowBackend::navigateToRequested(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 92, _a);
}

// SIGNAL 93
void ShadowLauncher::ShadowBackend::openRpDetailRequested(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 93, _a);
}

// SIGNAL 94
void ShadowLauncher::ShadowBackend::openModDetailRequested(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 94, _a);
}

// SIGNAL 95
void ShadowLauncher::ShadowBackend::openShaderDetailRequested(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 95, _a);
}

// SIGNAL 96
void ShadowLauncher::ShadowBackend::setRpShowPreReleases(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 96, _a);
}

// SIGNAL 97
void ShadowLauncher::ShadowBackend::openRpVersionMenu()
{
    QMetaObject::activate(this, &staticMetaObject, 97, nullptr);
}

// SIGNAL 98
void ShadowLauncher::ShadowBackend::expandRpDetailGroup(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 98, _a);
}

// SIGNAL 99
void ShadowLauncher::ShadowBackend::selectRpDetailSubVer(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 99, _a);
}

// SIGNAL 100
void ShadowLauncher::ShadowBackend::verifyStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 100, nullptr);
}

// SIGNAL 101
void ShadowLauncher::ShadowBackend::verifyProgress(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 101, _a);
}

// SIGNAL 102
void ShadowLauncher::ShadowBackend::verifyFinished(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 102, _a);
}

// SIGNAL 103
void ShadowLauncher::ShadowBackend::verifyFailedFiles(const QStringList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 103, _a);
}

// SIGNAL 104
void ShadowLauncher::ShadowBackend::launchCheckProgress(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 104, _a);
}

// SIGNAL 105
void ShadowLauncher::ShadowBackend::launchCheckFailed(const QString & _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 105, _a);
}

// SIGNAL 106
void ShadowLauncher::ShadowBackend::launchCheckMissingFiles(const QStringList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 106, _a);
}

// SIGNAL 107
void ShadowLauncher::ShadowBackend::launchCheckWarning(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 107, _a);
}
QT_WARNING_POP
