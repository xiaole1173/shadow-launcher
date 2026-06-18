/****************************************************************************
** Meta object code from reading C++ file 'shadow_backend.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/backend/shadow_backend.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'shadow_backend.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.5.3. It"
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

#ifdef QT_MOC_HAS_STRINGDATA
struct qt_meta_stringdata_CLASSShadowLauncherSCOPEShadowBackendENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSShadowLauncherSCOPEShadowBackendENDCLASS = QtMocHelpers::stringData(
    "ShadowLauncher::ShadowBackend",
    "accountChanged",
    "",
    "skinReady",
    "offlineHistoryChanged",
    "javaPathChanged",
    "javaReadyChanged",
    "memorySettingsChanged",
    "generalSettingsChanged",
    "isolationChanged",
    "versionListReady",
    "versionDetailsReady",
    "installedVersionsChanged",
    "selectedVersionChanged",
    "installStateChanged",
    "installProgressChanged",
    "installTotalChanged",
    "installFileProgress",
    "installBytesProgress",
    "dl",
    "total",
    "installPhaseChanged",
    "installFinished",
    "success",
    "installSpeedChanged",
    "launchProgressChanged",
    "progress",
    "status",
    "launchStateChanged",
    "minecraftStarted",
    "minecraftStopped",
    "isRunningChanged",
    "resourceDownloadStateChanged",
    "resourceDownloadProgress",
    "completed",
    "fileName",
    "resourceDownloadDone",
    "verifyRunningChanged",
    "verifyCheckedChanged",
    "verifyTotalChanged",
    "installPausedChanged",
    "searchResultsReady",
    "results",
    "gameDirChanged",
    "themeChanged",
    "loginModeChanged",
    "logMessage",
    "msg",
    "verifyStarted",
    "verifyProgress",
    "checked",
    "verifyFinished",
    "allPassed",
    "verifyFailedFiles",
    "failedFiles",
    "launchCheckProgress",
    "step",
    "launchCheckFailed",
    "phase",
    "details",
    "launchCheckMissingFiles",
    "files",
    "launchCheckWarning",
    "warning",
    "listMods",
    "listResourcePacks",
    "listSaves",
    "offlineLogin",
    "username",
    "logout",
    "getSkinUrl",
    "scanJavaInstallations",
    "autoSelectJava",
    "detectJava",
    "browseJava",
    "selectJavaByIndex",
    "index",
    "getMemoryStatus",
    "setMinMemory",
    "mb",
    "setMaxMemory",
    "setIsolationEnabled",
    "enabled",
    "getVersionGameDir",
    "versionId",
    "migrateVersionToIsolated",
    "openGameDir",
    "openVersionDir",
    "deleteVersion",
    "refreshVersionList",
    "refreshInstalled",
    "refreshInstalledList",
    "refreshVersionDetails",
    "refreshGameDirInfo",
    "installVersion",
    "sourceIndex",
    "cancelInstall",
    "launch",
    "cancelLaunch",
    "killGameProcess",
    "killMinecraft",
    "getPopularMods",
    "loader",
    "getShaderList",
    "searchMods",
    "query",
    "downloadMod",
    "slug",
    "downloadShader",
    "setSelectedVersion",
    "setTheme",
    "theme",
    "checkAll",
    "setGameDir",
    "dir",
    "getAutoMemory",
    "setCloseOnLaunch",
    "v",
    "setAutoMemoryEnabled",
    "setGameArgs",
    "setJvmArgs",
    "isModdedVersion",
    "openJavaFileDialog",
    "pickJava",
    "checkFileChanges",
    "deleteMod",
    "deleteResourcePack",
    "deleteSave",
    "migrateVersion",
    "openConfigFolder",
    "openCrashLog",
    "openLatestLog",
    "openLogsFolder",
    "openModsFolder",
    "openResourcePacksFolder",
    "openSavesFolder",
    "openScreenshotsFolder",
    "openShaderPacksFolder",
    "removeGameDir",
    "pauseInstall",
    "resumeInstall",
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
    "isOnline",
    "accountUuid",
    "skinPath",
    "offlineUsernames",
    "lastLoginMode",
    "javaPath",
    "javaVersion",
    "javaMajor",
    "javaInstalled",
    "javaReady",
    "minMemoryMb",
    "maxMemoryMb",
    "closeAfterLaunch",
    "isolationEnabled",
    "availableJavaList",
    "selectedVersion",
    "versionIds",
    "versionList",
    "releaseVersions",
    "snapshotVersions",
    "oldVersions",
    "installedVersions",
    "installing",
    "installProgress",
    "installTotal",
    "installFile",
    "installVersionId",
    "installPhase",
    "installSpeed",
    "installBytesDownloaded",
    "installBytesTotal",
    "installPaused",
    "launching",
    "launchProgress",
    "launchStatus",
    "isRunning",
    "launchVersion",
    "launchUsername",
    "downloading",
    "gameDir",
    "dataDir",
    "devMode",
    "versionDetails",
    "currentVersionSummary",
    "downloadQueue",
    "activeDownloads",
    "gameDirInfo",
    "gameDirectories",
    "diskFree",
    "diskPercent",
    "closeOnLaunch",
    "autoMemoryEnabled",
    "systemMemoryInfo",
    "gameArgs",
    "jvmArgs",
    "javaCompatibility",
    "verifyRunning",
    "verifyChecked",
    "verifyTotal",
    "verifyResultOk",
    "verifyResultText"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSShadowLauncherSCOPEShadowBackendENDCLASS_t {
    uint offsetsAndSizes[424];
    char stringdata0[30];
    char stringdata1[15];
    char stringdata2[1];
    char stringdata3[10];
    char stringdata4[22];
    char stringdata5[16];
    char stringdata6[17];
    char stringdata7[22];
    char stringdata8[23];
    char stringdata9[17];
    char stringdata10[17];
    char stringdata11[20];
    char stringdata12[25];
    char stringdata13[23];
    char stringdata14[20];
    char stringdata15[23];
    char stringdata16[20];
    char stringdata17[20];
    char stringdata18[21];
    char stringdata19[3];
    char stringdata20[6];
    char stringdata21[20];
    char stringdata22[16];
    char stringdata23[8];
    char stringdata24[20];
    char stringdata25[22];
    char stringdata26[9];
    char stringdata27[7];
    char stringdata28[19];
    char stringdata29[17];
    char stringdata30[17];
    char stringdata31[17];
    char stringdata32[29];
    char stringdata33[25];
    char stringdata34[10];
    char stringdata35[9];
    char stringdata36[21];
    char stringdata37[21];
    char stringdata38[21];
    char stringdata39[19];
    char stringdata40[21];
    char stringdata41[19];
    char stringdata42[8];
    char stringdata43[15];
    char stringdata44[13];
    char stringdata45[17];
    char stringdata46[11];
    char stringdata47[4];
    char stringdata48[14];
    char stringdata49[15];
    char stringdata50[8];
    char stringdata51[15];
    char stringdata52[10];
    char stringdata53[18];
    char stringdata54[12];
    char stringdata55[20];
    char stringdata56[5];
    char stringdata57[18];
    char stringdata58[6];
    char stringdata59[8];
    char stringdata60[24];
    char stringdata61[6];
    char stringdata62[19];
    char stringdata63[8];
    char stringdata64[9];
    char stringdata65[18];
    char stringdata66[10];
    char stringdata67[13];
    char stringdata68[9];
    char stringdata69[7];
    char stringdata70[11];
    char stringdata71[22];
    char stringdata72[15];
    char stringdata73[11];
    char stringdata74[11];
    char stringdata75[18];
    char stringdata76[6];
    char stringdata77[16];
    char stringdata78[13];
    char stringdata79[3];
    char stringdata80[13];
    char stringdata81[20];
    char stringdata82[8];
    char stringdata83[18];
    char stringdata84[10];
    char stringdata85[25];
    char stringdata86[12];
    char stringdata87[15];
    char stringdata88[14];
    char stringdata89[19];
    char stringdata90[17];
    char stringdata91[21];
    char stringdata92[22];
    char stringdata93[19];
    char stringdata94[15];
    char stringdata95[12];
    char stringdata96[14];
    char stringdata97[7];
    char stringdata98[13];
    char stringdata99[16];
    char stringdata100[14];
    char stringdata101[15];
    char stringdata102[7];
    char stringdata103[14];
    char stringdata104[11];
    char stringdata105[6];
    char stringdata106[12];
    char stringdata107[5];
    char stringdata108[15];
    char stringdata109[19];
    char stringdata110[9];
    char stringdata111[6];
    char stringdata112[9];
    char stringdata113[11];
    char stringdata114[4];
    char stringdata115[14];
    char stringdata116[17];
    char stringdata117[2];
    char stringdata118[21];
    char stringdata119[12];
    char stringdata120[11];
    char stringdata121[16];
    char stringdata122[19];
    char stringdata123[9];
    char stringdata124[17];
    char stringdata125[10];
    char stringdata126[19];
    char stringdata127[11];
    char stringdata128[15];
    char stringdata129[17];
    char stringdata130[13];
    char stringdata131[14];
    char stringdata132[15];
    char stringdata133[15];
    char stringdata134[24];
    char stringdata135[16];
    char stringdata136[22];
    char stringdata137[22];
    char stringdata138[14];
    char stringdata139[13];
    char stringdata140[14];
    char stringdata141[21];
    char stringdata142[14];
    char stringdata143[20];
    char stringdata144[14];
    char stringdata145[6];
    char stringdata146[6];
    char stringdata147[13];
    char stringdata148[9];
    char stringdata149[16];
    char stringdata150[14];
    char stringdata151[9];
    char stringdata152[12];
    char stringdata153[9];
    char stringdata154[17];
    char stringdata155[14];
    char stringdata156[9];
    char stringdata157[12];
    char stringdata158[10];
    char stringdata159[14];
    char stringdata160[10];
    char stringdata161[12];
    char stringdata162[12];
    char stringdata163[17];
    char stringdata164[17];
    char stringdata165[18];
    char stringdata166[16];
    char stringdata167[11];
    char stringdata168[12];
    char stringdata169[16];
    char stringdata170[17];
    char stringdata171[12];
    char stringdata172[18];
    char stringdata173[11];
    char stringdata174[16];
    char stringdata175[13];
    char stringdata176[12];
    char stringdata177[17];
    char stringdata178[13];
    char stringdata179[13];
    char stringdata180[23];
    char stringdata181[18];
    char stringdata182[14];
    char stringdata183[10];
    char stringdata184[15];
    char stringdata185[13];
    char stringdata186[10];
    char stringdata187[14];
    char stringdata188[15];
    char stringdata189[12];
    char stringdata190[8];
    char stringdata191[8];
    char stringdata192[8];
    char stringdata193[15];
    char stringdata194[22];
    char stringdata195[14];
    char stringdata196[16];
    char stringdata197[12];
    char stringdata198[16];
    char stringdata199[9];
    char stringdata200[12];
    char stringdata201[14];
    char stringdata202[18];
    char stringdata203[17];
    char stringdata204[9];
    char stringdata205[8];
    char stringdata206[18];
    char stringdata207[14];
    char stringdata208[14];
    char stringdata209[12];
    char stringdata210[15];
    char stringdata211[17];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSShadowLauncherSCOPEShadowBackendENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSShadowLauncherSCOPEShadowBackendENDCLASS_t qt_meta_stringdata_CLASSShadowLauncherSCOPEShadowBackendENDCLASS = {
    {
        QT_MOC_LITERAL(0, 29),  // "ShadowLauncher::ShadowBackend"
        QT_MOC_LITERAL(30, 14),  // "accountChanged"
        QT_MOC_LITERAL(45, 0),  // ""
        QT_MOC_LITERAL(46, 9),  // "skinReady"
        QT_MOC_LITERAL(56, 21),  // "offlineHistoryChanged"
        QT_MOC_LITERAL(78, 15),  // "javaPathChanged"
        QT_MOC_LITERAL(94, 16),  // "javaReadyChanged"
        QT_MOC_LITERAL(111, 21),  // "memorySettingsChanged"
        QT_MOC_LITERAL(133, 22),  // "generalSettingsChanged"
        QT_MOC_LITERAL(156, 16),  // "isolationChanged"
        QT_MOC_LITERAL(173, 16),  // "versionListReady"
        QT_MOC_LITERAL(190, 19),  // "versionDetailsReady"
        QT_MOC_LITERAL(210, 24),  // "installedVersionsChanged"
        QT_MOC_LITERAL(235, 22),  // "selectedVersionChanged"
        QT_MOC_LITERAL(258, 19),  // "installStateChanged"
        QT_MOC_LITERAL(278, 22),  // "installProgressChanged"
        QT_MOC_LITERAL(301, 19),  // "installTotalChanged"
        QT_MOC_LITERAL(321, 19),  // "installFileProgress"
        QT_MOC_LITERAL(341, 20),  // "installBytesProgress"
        QT_MOC_LITERAL(362, 2),  // "dl"
        QT_MOC_LITERAL(365, 5),  // "total"
        QT_MOC_LITERAL(371, 19),  // "installPhaseChanged"
        QT_MOC_LITERAL(391, 15),  // "installFinished"
        QT_MOC_LITERAL(407, 7),  // "success"
        QT_MOC_LITERAL(415, 19),  // "installSpeedChanged"
        QT_MOC_LITERAL(435, 21),  // "launchProgressChanged"
        QT_MOC_LITERAL(457, 8),  // "progress"
        QT_MOC_LITERAL(466, 6),  // "status"
        QT_MOC_LITERAL(473, 18),  // "launchStateChanged"
        QT_MOC_LITERAL(492, 16),  // "minecraftStarted"
        QT_MOC_LITERAL(509, 16),  // "minecraftStopped"
        QT_MOC_LITERAL(526, 16),  // "isRunningChanged"
        QT_MOC_LITERAL(543, 28),  // "resourceDownloadStateChanged"
        QT_MOC_LITERAL(572, 24),  // "resourceDownloadProgress"
        QT_MOC_LITERAL(597, 9),  // "completed"
        QT_MOC_LITERAL(607, 8),  // "fileName"
        QT_MOC_LITERAL(616, 20),  // "resourceDownloadDone"
        QT_MOC_LITERAL(637, 20),  // "verifyRunningChanged"
        QT_MOC_LITERAL(658, 20),  // "verifyCheckedChanged"
        QT_MOC_LITERAL(679, 18),  // "verifyTotalChanged"
        QT_MOC_LITERAL(698, 20),  // "installPausedChanged"
        QT_MOC_LITERAL(719, 18),  // "searchResultsReady"
        QT_MOC_LITERAL(738, 7),  // "results"
        QT_MOC_LITERAL(746, 14),  // "gameDirChanged"
        QT_MOC_LITERAL(761, 12),  // "themeChanged"
        QT_MOC_LITERAL(774, 16),  // "loginModeChanged"
        QT_MOC_LITERAL(791, 10),  // "logMessage"
        QT_MOC_LITERAL(802, 3),  // "msg"
        QT_MOC_LITERAL(806, 13),  // "verifyStarted"
        QT_MOC_LITERAL(820, 14),  // "verifyProgress"
        QT_MOC_LITERAL(835, 7),  // "checked"
        QT_MOC_LITERAL(843, 14),  // "verifyFinished"
        QT_MOC_LITERAL(858, 9),  // "allPassed"
        QT_MOC_LITERAL(868, 17),  // "verifyFailedFiles"
        QT_MOC_LITERAL(886, 11),  // "failedFiles"
        QT_MOC_LITERAL(898, 19),  // "launchCheckProgress"
        QT_MOC_LITERAL(918, 4),  // "step"
        QT_MOC_LITERAL(923, 17),  // "launchCheckFailed"
        QT_MOC_LITERAL(941, 5),  // "phase"
        QT_MOC_LITERAL(947, 7),  // "details"
        QT_MOC_LITERAL(955, 23),  // "launchCheckMissingFiles"
        QT_MOC_LITERAL(979, 5),  // "files"
        QT_MOC_LITERAL(985, 18),  // "launchCheckWarning"
        QT_MOC_LITERAL(1004, 7),  // "warning"
        QT_MOC_LITERAL(1012, 8),  // "listMods"
        QT_MOC_LITERAL(1021, 17),  // "listResourcePacks"
        QT_MOC_LITERAL(1039, 9),  // "listSaves"
        QT_MOC_LITERAL(1049, 12),  // "offlineLogin"
        QT_MOC_LITERAL(1062, 8),  // "username"
        QT_MOC_LITERAL(1071, 6),  // "logout"
        QT_MOC_LITERAL(1078, 10),  // "getSkinUrl"
        QT_MOC_LITERAL(1089, 21),  // "scanJavaInstallations"
        QT_MOC_LITERAL(1111, 14),  // "autoSelectJava"
        QT_MOC_LITERAL(1126, 10),  // "detectJava"
        QT_MOC_LITERAL(1137, 10),  // "browseJava"
        QT_MOC_LITERAL(1148, 17),  // "selectJavaByIndex"
        QT_MOC_LITERAL(1166, 5),  // "index"
        QT_MOC_LITERAL(1172, 15),  // "getMemoryStatus"
        QT_MOC_LITERAL(1188, 12),  // "setMinMemory"
        QT_MOC_LITERAL(1201, 2),  // "mb"
        QT_MOC_LITERAL(1204, 12),  // "setMaxMemory"
        QT_MOC_LITERAL(1217, 19),  // "setIsolationEnabled"
        QT_MOC_LITERAL(1237, 7),  // "enabled"
        QT_MOC_LITERAL(1245, 17),  // "getVersionGameDir"
        QT_MOC_LITERAL(1263, 9),  // "versionId"
        QT_MOC_LITERAL(1273, 24),  // "migrateVersionToIsolated"
        QT_MOC_LITERAL(1298, 11),  // "openGameDir"
        QT_MOC_LITERAL(1310, 14),  // "openVersionDir"
        QT_MOC_LITERAL(1325, 13),  // "deleteVersion"
        QT_MOC_LITERAL(1339, 18),  // "refreshVersionList"
        QT_MOC_LITERAL(1358, 16),  // "refreshInstalled"
        QT_MOC_LITERAL(1375, 20),  // "refreshInstalledList"
        QT_MOC_LITERAL(1396, 21),  // "refreshVersionDetails"
        QT_MOC_LITERAL(1418, 18),  // "refreshGameDirInfo"
        QT_MOC_LITERAL(1437, 14),  // "installVersion"
        QT_MOC_LITERAL(1452, 11),  // "sourceIndex"
        QT_MOC_LITERAL(1464, 13),  // "cancelInstall"
        QT_MOC_LITERAL(1478, 6),  // "launch"
        QT_MOC_LITERAL(1485, 12),  // "cancelLaunch"
        QT_MOC_LITERAL(1498, 15),  // "killGameProcess"
        QT_MOC_LITERAL(1514, 13),  // "killMinecraft"
        QT_MOC_LITERAL(1528, 14),  // "getPopularMods"
        QT_MOC_LITERAL(1543, 6),  // "loader"
        QT_MOC_LITERAL(1550, 13),  // "getShaderList"
        QT_MOC_LITERAL(1564, 10),  // "searchMods"
        QT_MOC_LITERAL(1575, 5),  // "query"
        QT_MOC_LITERAL(1581, 11),  // "downloadMod"
        QT_MOC_LITERAL(1593, 4),  // "slug"
        QT_MOC_LITERAL(1598, 14),  // "downloadShader"
        QT_MOC_LITERAL(1613, 18),  // "setSelectedVersion"
        QT_MOC_LITERAL(1632, 8),  // "setTheme"
        QT_MOC_LITERAL(1641, 5),  // "theme"
        QT_MOC_LITERAL(1647, 8),  // "checkAll"
        QT_MOC_LITERAL(1656, 10),  // "setGameDir"
        QT_MOC_LITERAL(1667, 3),  // "dir"
        QT_MOC_LITERAL(1671, 13),  // "getAutoMemory"
        QT_MOC_LITERAL(1685, 16),  // "setCloseOnLaunch"
        QT_MOC_LITERAL(1702, 1),  // "v"
        QT_MOC_LITERAL(1704, 20),  // "setAutoMemoryEnabled"
        QT_MOC_LITERAL(1725, 11),  // "setGameArgs"
        QT_MOC_LITERAL(1737, 10),  // "setJvmArgs"
        QT_MOC_LITERAL(1748, 15),  // "isModdedVersion"
        QT_MOC_LITERAL(1764, 18),  // "openJavaFileDialog"
        QT_MOC_LITERAL(1783, 8),  // "pickJava"
        QT_MOC_LITERAL(1792, 16),  // "checkFileChanges"
        QT_MOC_LITERAL(1809, 9),  // "deleteMod"
        QT_MOC_LITERAL(1819, 18),  // "deleteResourcePack"
        QT_MOC_LITERAL(1838, 10),  // "deleteSave"
        QT_MOC_LITERAL(1849, 14),  // "migrateVersion"
        QT_MOC_LITERAL(1864, 16),  // "openConfigFolder"
        QT_MOC_LITERAL(1881, 12),  // "openCrashLog"
        QT_MOC_LITERAL(1894, 13),  // "openLatestLog"
        QT_MOC_LITERAL(1908, 14),  // "openLogsFolder"
        QT_MOC_LITERAL(1923, 14),  // "openModsFolder"
        QT_MOC_LITERAL(1938, 23),  // "openResourcePacksFolder"
        QT_MOC_LITERAL(1962, 15),  // "openSavesFolder"
        QT_MOC_LITERAL(1978, 21),  // "openScreenshotsFolder"
        QT_MOC_LITERAL(2000, 21),  // "openShaderPacksFolder"
        QT_MOC_LITERAL(2022, 13),  // "removeGameDir"
        QT_MOC_LITERAL(2036, 12),  // "pauseInstall"
        QT_MOC_LITERAL(2049, 13),  // "resumeInstall"
        QT_MOC_LITERAL(2063, 20),  // "cancelQueuedDownload"
        QT_MOC_LITERAL(2084, 13),  // "verifyVersion"
        QT_MOC_LITERAL(2098, 19),  // "cleanCorruptVersion"
        QT_MOC_LITERAL(2118, 13),  // "renameVersion"
        QT_MOC_LITERAL(2132, 5),  // "oldId"
        QT_MOC_LITERAL(2138, 5),  // "newId"
        QT_MOC_LITERAL(2144, 12),  // "cloneVersion"
        QT_MOC_LITERAL(2157, 8),  // "sourceId"
        QT_MOC_LITERAL(2166, 15),  // "copyVersionPath"
        QT_MOC_LITERAL(2182, 13),  // "repairVersion"
        QT_MOC_LITERAL(2196, 8),  // "isOnline"
        QT_MOC_LITERAL(2205, 11),  // "accountUuid"
        QT_MOC_LITERAL(2217, 8),  // "skinPath"
        QT_MOC_LITERAL(2226, 16),  // "offlineUsernames"
        QT_MOC_LITERAL(2243, 13),  // "lastLoginMode"
        QT_MOC_LITERAL(2257, 8),  // "javaPath"
        QT_MOC_LITERAL(2266, 11),  // "javaVersion"
        QT_MOC_LITERAL(2278, 9),  // "javaMajor"
        QT_MOC_LITERAL(2288, 13),  // "javaInstalled"
        QT_MOC_LITERAL(2302, 9),  // "javaReady"
        QT_MOC_LITERAL(2312, 11),  // "minMemoryMb"
        QT_MOC_LITERAL(2324, 11),  // "maxMemoryMb"
        QT_MOC_LITERAL(2336, 16),  // "closeAfterLaunch"
        QT_MOC_LITERAL(2353, 16),  // "isolationEnabled"
        QT_MOC_LITERAL(2370, 17),  // "availableJavaList"
        QT_MOC_LITERAL(2388, 15),  // "selectedVersion"
        QT_MOC_LITERAL(2404, 10),  // "versionIds"
        QT_MOC_LITERAL(2415, 11),  // "versionList"
        QT_MOC_LITERAL(2427, 15),  // "releaseVersions"
        QT_MOC_LITERAL(2443, 16),  // "snapshotVersions"
        QT_MOC_LITERAL(2460, 11),  // "oldVersions"
        QT_MOC_LITERAL(2472, 17),  // "installedVersions"
        QT_MOC_LITERAL(2490, 10),  // "installing"
        QT_MOC_LITERAL(2501, 15),  // "installProgress"
        QT_MOC_LITERAL(2517, 12),  // "installTotal"
        QT_MOC_LITERAL(2530, 11),  // "installFile"
        QT_MOC_LITERAL(2542, 16),  // "installVersionId"
        QT_MOC_LITERAL(2559, 12),  // "installPhase"
        QT_MOC_LITERAL(2572, 12),  // "installSpeed"
        QT_MOC_LITERAL(2585, 22),  // "installBytesDownloaded"
        QT_MOC_LITERAL(2608, 17),  // "installBytesTotal"
        QT_MOC_LITERAL(2626, 13),  // "installPaused"
        QT_MOC_LITERAL(2640, 9),  // "launching"
        QT_MOC_LITERAL(2650, 14),  // "launchProgress"
        QT_MOC_LITERAL(2665, 12),  // "launchStatus"
        QT_MOC_LITERAL(2678, 9),  // "isRunning"
        QT_MOC_LITERAL(2688, 13),  // "launchVersion"
        QT_MOC_LITERAL(2702, 14),  // "launchUsername"
        QT_MOC_LITERAL(2717, 11),  // "downloading"
        QT_MOC_LITERAL(2729, 7),  // "gameDir"
        QT_MOC_LITERAL(2737, 7),  // "dataDir"
        QT_MOC_LITERAL(2745, 7),  // "devMode"
        QT_MOC_LITERAL(2753, 14),  // "versionDetails"
        QT_MOC_LITERAL(2768, 21),  // "currentVersionSummary"
        QT_MOC_LITERAL(2790, 13),  // "downloadQueue"
        QT_MOC_LITERAL(2804, 15),  // "activeDownloads"
        QT_MOC_LITERAL(2820, 11),  // "gameDirInfo"
        QT_MOC_LITERAL(2832, 15),  // "gameDirectories"
        QT_MOC_LITERAL(2848, 8),  // "diskFree"
        QT_MOC_LITERAL(2857, 11),  // "diskPercent"
        QT_MOC_LITERAL(2869, 13),  // "closeOnLaunch"
        QT_MOC_LITERAL(2883, 17),  // "autoMemoryEnabled"
        QT_MOC_LITERAL(2901, 16),  // "systemMemoryInfo"
        QT_MOC_LITERAL(2918, 8),  // "gameArgs"
        QT_MOC_LITERAL(2927, 7),  // "jvmArgs"
        QT_MOC_LITERAL(2935, 17),  // "javaCompatibility"
        QT_MOC_LITERAL(2953, 13),  // "verifyRunning"
        QT_MOC_LITERAL(2967, 13),  // "verifyChecked"
        QT_MOC_LITERAL(2981, 11),  // "verifyTotal"
        QT_MOC_LITERAL(2993, 14),  // "verifyResultOk"
        QT_MOC_LITERAL(3008, 16)   // "verifyResultText"
    },
    "ShadowLauncher::ShadowBackend",
    "accountChanged",
    "",
    "skinReady",
    "offlineHistoryChanged",
    "javaPathChanged",
    "javaReadyChanged",
    "memorySettingsChanged",
    "generalSettingsChanged",
    "isolationChanged",
    "versionListReady",
    "versionDetailsReady",
    "installedVersionsChanged",
    "selectedVersionChanged",
    "installStateChanged",
    "installProgressChanged",
    "installTotalChanged",
    "installFileProgress",
    "installBytesProgress",
    "dl",
    "total",
    "installPhaseChanged",
    "installFinished",
    "success",
    "installSpeedChanged",
    "launchProgressChanged",
    "progress",
    "status",
    "launchStateChanged",
    "minecraftStarted",
    "minecraftStopped",
    "isRunningChanged",
    "resourceDownloadStateChanged",
    "resourceDownloadProgress",
    "completed",
    "fileName",
    "resourceDownloadDone",
    "verifyRunningChanged",
    "verifyCheckedChanged",
    "verifyTotalChanged",
    "installPausedChanged",
    "searchResultsReady",
    "results",
    "gameDirChanged",
    "themeChanged",
    "loginModeChanged",
    "logMessage",
    "msg",
    "verifyStarted",
    "verifyProgress",
    "checked",
    "verifyFinished",
    "allPassed",
    "verifyFailedFiles",
    "failedFiles",
    "launchCheckProgress",
    "step",
    "launchCheckFailed",
    "phase",
    "details",
    "launchCheckMissingFiles",
    "files",
    "launchCheckWarning",
    "warning",
    "listMods",
    "listResourcePacks",
    "listSaves",
    "offlineLogin",
    "username",
    "logout",
    "getSkinUrl",
    "scanJavaInstallations",
    "autoSelectJava",
    "detectJava",
    "browseJava",
    "selectJavaByIndex",
    "index",
    "getMemoryStatus",
    "setMinMemory",
    "mb",
    "setMaxMemory",
    "setIsolationEnabled",
    "enabled",
    "getVersionGameDir",
    "versionId",
    "migrateVersionToIsolated",
    "openGameDir",
    "openVersionDir",
    "deleteVersion",
    "refreshVersionList",
    "refreshInstalled",
    "refreshInstalledList",
    "refreshVersionDetails",
    "refreshGameDirInfo",
    "installVersion",
    "sourceIndex",
    "cancelInstall",
    "launch",
    "cancelLaunch",
    "killGameProcess",
    "killMinecraft",
    "getPopularMods",
    "loader",
    "getShaderList",
    "searchMods",
    "query",
    "downloadMod",
    "slug",
    "downloadShader",
    "setSelectedVersion",
    "setTheme",
    "theme",
    "checkAll",
    "setGameDir",
    "dir",
    "getAutoMemory",
    "setCloseOnLaunch",
    "v",
    "setAutoMemoryEnabled",
    "setGameArgs",
    "setJvmArgs",
    "isModdedVersion",
    "openJavaFileDialog",
    "pickJava",
    "checkFileChanges",
    "deleteMod",
    "deleteResourcePack",
    "deleteSave",
    "migrateVersion",
    "openConfigFolder",
    "openCrashLog",
    "openLatestLog",
    "openLogsFolder",
    "openModsFolder",
    "openResourcePacksFolder",
    "openSavesFolder",
    "openScreenshotsFolder",
    "openShaderPacksFolder",
    "removeGameDir",
    "pauseInstall",
    "resumeInstall",
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
    "isOnline",
    "accountUuid",
    "skinPath",
    "offlineUsernames",
    "lastLoginMode",
    "javaPath",
    "javaVersion",
    "javaMajor",
    "javaInstalled",
    "javaReady",
    "minMemoryMb",
    "maxMemoryMb",
    "closeAfterLaunch",
    "isolationEnabled",
    "availableJavaList",
    "selectedVersion",
    "versionIds",
    "versionList",
    "releaseVersions",
    "snapshotVersions",
    "oldVersions",
    "installedVersions",
    "installing",
    "installProgress",
    "installTotal",
    "installFile",
    "installVersionId",
    "installPhase",
    "installSpeed",
    "installBytesDownloaded",
    "installBytesTotal",
    "installPaused",
    "launching",
    "launchProgress",
    "launchStatus",
    "isRunning",
    "launchVersion",
    "launchUsername",
    "downloading",
    "gameDir",
    "dataDir",
    "devMode",
    "versionDetails",
    "currentVersionSummary",
    "downloadQueue",
    "activeDownloads",
    "gameDirInfo",
    "gameDirectories",
    "diskFree",
    "diskPercent",
    "closeOnLaunch",
    "autoMemoryEnabled",
    "systemMemoryInfo",
    "gameArgs",
    "jvmArgs",
    "javaCompatibility",
    "verifyRunning",
    "verifyChecked",
    "verifyTotal",
    "verifyResultOk",
    "verifyResultText"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSShadowLauncherSCOPEShadowBackendENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
     120,   14, // methods
      63,  982, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      45,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  734,    2, 0x06,   64 /* Public */,
       3,    0,  735,    2, 0x06,   65 /* Public */,
       4,    0,  736,    2, 0x06,   66 /* Public */,
       5,    0,  737,    2, 0x06,   67 /* Public */,
       6,    0,  738,    2, 0x06,   68 /* Public */,
       7,    0,  739,    2, 0x06,   69 /* Public */,
       8,    0,  740,    2, 0x06,   70 /* Public */,
       9,    0,  741,    2, 0x06,   71 /* Public */,
      10,    0,  742,    2, 0x06,   72 /* Public */,
      11,    0,  743,    2, 0x06,   73 /* Public */,
      12,    0,  744,    2, 0x06,   74 /* Public */,
      13,    0,  745,    2, 0x06,   75 /* Public */,
      14,    0,  746,    2, 0x06,   76 /* Public */,
      15,    0,  747,    2, 0x06,   77 /* Public */,
      16,    0,  748,    2, 0x06,   78 /* Public */,
      17,    0,  749,    2, 0x06,   79 /* Public */,
      18,    2,  750,    2, 0x06,   80 /* Public */,
      21,    0,  755,    2, 0x06,   83 /* Public */,
      22,    1,  756,    2, 0x06,   84 /* Public */,
      24,    0,  759,    2, 0x06,   86 /* Public */,
      25,    2,  760,    2, 0x06,   87 /* Public */,
      28,    0,  765,    2, 0x06,   90 /* Public */,
      29,    0,  766,    2, 0x06,   91 /* Public */,
      30,    0,  767,    2, 0x06,   92 /* Public */,
      31,    0,  768,    2, 0x06,   93 /* Public */,
      32,    0,  769,    2, 0x06,   94 /* Public */,
      33,    3,  770,    2, 0x06,   95 /* Public */,
      36,    1,  777,    2, 0x06,   99 /* Public */,
      37,    0,  780,    2, 0x06,  101 /* Public */,
      38,    0,  781,    2, 0x06,  102 /* Public */,
      39,    0,  782,    2, 0x06,  103 /* Public */,
      40,    0,  783,    2, 0x06,  104 /* Public */,
      41,    1,  784,    2, 0x06,  105 /* Public */,
      43,    0,  787,    2, 0x06,  107 /* Public */,
      44,    0,  788,    2, 0x06,  108 /* Public */,
      45,    0,  789,    2, 0x06,  109 /* Public */,
      46,    1,  790,    2, 0x06,  110 /* Public */,
      48,    0,  793,    2, 0x06,  112 /* Public */,
      49,    2,  794,    2, 0x06,  113 /* Public */,
      51,    1,  799,    2, 0x06,  116 /* Public */,
      53,    1,  802,    2, 0x06,  118 /* Public */,
      55,    1,  805,    2, 0x06,  120 /* Public */,
      57,    2,  808,    2, 0x06,  122 /* Public */,
      60,    1,  813,    2, 0x06,  125 /* Public */,
      62,    1,  816,    2, 0x06,  127 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      64,    0,  819,    2, 0x102,  129 /* Public | MethodIsConst  */,
      65,    0,  820,    2, 0x102,  130 /* Public | MethodIsConst  */,
      66,    0,  821,    2, 0x102,  131 /* Public | MethodIsConst  */,
      67,    1,  822,    2, 0x02,  132 /* Public */,
      69,    0,  825,    2, 0x02,  134 /* Public */,
      70,    1,  826,    2, 0x102,  135 /* Public | MethodIsConst  */,
      70,    0,  829,    2, 0x122,  137 /* Public | MethodCloned | MethodIsConst  */,
      71,    0,  830,    2, 0x02,  138 /* Public */,
      72,    0,  831,    2, 0x02,  139 /* Public */,
      73,    0,  832,    2, 0x02,  140 /* Public */,
      74,    0,  833,    2, 0x02,  141 /* Public */,
      75,    1,  834,    2, 0x02,  142 /* Public */,
      77,    0,  837,    2, 0x02,  144 /* Public */,
      78,    1,  838,    2, 0x02,  145 /* Public */,
      80,    1,  841,    2, 0x02,  147 /* Public */,
      81,    1,  844,    2, 0x02,  149 /* Public */,
      83,    1,  847,    2, 0x102,  151 /* Public | MethodIsConst  */,
      85,    1,  850,    2, 0x02,  153 /* Public */,
      86,    0,  853,    2, 0x02,  155 /* Public */,
      87,    1,  854,    2, 0x02,  156 /* Public */,
      88,    1,  857,    2, 0x02,  158 /* Public */,
      89,    0,  860,    2, 0x02,  160 /* Public */,
      90,    0,  861,    2, 0x02,  161 /* Public */,
      91,    0,  862,    2, 0x02,  162 /* Public */,
      92,    0,  863,    2, 0x02,  163 /* Public */,
      93,    0,  864,    2, 0x02,  164 /* Public */,
      94,    2,  865,    2, 0x02,  165 /* Public */,
      94,    1,  870,    2, 0x22,  168 /* Public | MethodCloned */,
      96,    0,  873,    2, 0x02,  170 /* Public */,
      97,    1,  874,    2, 0x02,  171 /* Public */,
      98,    0,  877,    2, 0x02,  173 /* Public */,
      99,    0,  878,    2, 0x02,  174 /* Public */,
     100,    0,  879,    2, 0x02,  175 /* Public */,
     101,    1,  880,    2, 0x02,  176 /* Public */,
     103,    0,  883,    2, 0x02,  178 /* Public */,
     104,    2,  884,    2, 0x02,  179 /* Public */,
     104,    1,  889,    2, 0x22,  182 /* Public | MethodCloned */,
     106,    2,  892,    2, 0x02,  184 /* Public */,
     108,    1,  897,    2, 0x02,  187 /* Public */,
     109,    1,  900,    2, 0x02,  189 /* Public */,
     110,    1,  903,    2, 0x02,  191 /* Public */,
     112,    1,  906,    2, 0x02,  193 /* Public */,
     113,    1,  909,    2, 0x02,  195 /* Public */,
     115,    0,  912,    2, 0x02,  197 /* Public */,
     116,    1,  913,    2, 0x02,  198 /* Public */,
     118,    1,  916,    2, 0x02,  200 /* Public */,
     119,    1,  919,    2, 0x02,  202 /* Public */,
     120,    1,  922,    2, 0x02,  204 /* Public */,
     121,    1,  925,    2, 0x102,  206 /* Public | MethodIsConst  */,
     122,    0,  928,    2, 0x02,  208 /* Public */,
     123,    0,  929,    2, 0x02,  209 /* Public */,
     124,    0,  930,    2, 0x02,  210 /* Public */,
     125,    1,  931,    2, 0x02,  211 /* Public */,
     126,    1,  934,    2, 0x02,  213 /* Public */,
     127,    1,  937,    2, 0x02,  215 /* Public */,
     128,    1,  940,    2, 0x02,  217 /* Public */,
     129,    0,  943,    2, 0x02,  219 /* Public */,
     130,    0,  944,    2, 0x02,  220 /* Public */,
     131,    0,  945,    2, 0x02,  221 /* Public */,
     132,    0,  946,    2, 0x02,  222 /* Public */,
     133,    0,  947,    2, 0x02,  223 /* Public */,
     134,    0,  948,    2, 0x02,  224 /* Public */,
     135,    0,  949,    2, 0x02,  225 /* Public */,
     136,    0,  950,    2, 0x02,  226 /* Public */,
     137,    0,  951,    2, 0x02,  227 /* Public */,
     138,    1,  952,    2, 0x02,  228 /* Public */,
     139,    0,  955,    2, 0x02,  230 /* Public */,
     140,    0,  956,    2, 0x02,  231 /* Public */,
     141,    1,  957,    2, 0x02,  232 /* Public */,
     142,    1,  960,    2, 0x02,  234 /* Public */,
     143,    1,  963,    2, 0x02,  236 /* Public */,
     144,    2,  966,    2, 0x02,  238 /* Public */,
     147,    2,  971,    2, 0x02,  241 /* Public */,
     149,    1,  976,    2, 0x02,  244 /* Public */,
     150,    1,  979,    2, 0x02,  246 /* Public */,

 // signals: parameters
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
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::LongLong, QMetaType::LongLong,   19,   20,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   23,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,   26,   27,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::QString,   34,   20,   35,
    QMetaType::Void, QMetaType::Bool,   23,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QVariantList,   42,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   47,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   50,   20,
    QMetaType::Void, QMetaType::Bool,   52,
    QMetaType::Void, QMetaType::QStringList,   54,
    QMetaType::Void, QMetaType::QString,   56,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   58,   59,
    QMetaType::Void, QMetaType::QStringList,   61,
    QMetaType::Void, QMetaType::QString,   63,

 // methods: parameters
    QMetaType::QVariantList,
    QMetaType::QVariantList,
    QMetaType::QVariantList,
    QMetaType::Void, QMetaType::QString,   68,
    QMetaType::Void,
    QMetaType::QString, QMetaType::QString,   68,
    QMetaType::QString,
    QMetaType::QVariantList,
    QMetaType::QString,
    QMetaType::QString,
    QMetaType::QString,
    QMetaType::Void, QMetaType::Int,   76,
    QMetaType::QVariantMap,
    QMetaType::Void, QMetaType::Int,   79,
    QMetaType::Void, QMetaType::Int,   79,
    QMetaType::Void, QMetaType::Bool,   82,
    QMetaType::QString, QMetaType::QString,   84,
    QMetaType::Void, QMetaType::QString,   84,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   84,
    QMetaType::Void, QMetaType::QString,   84,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,   84,   95,
    QMetaType::Void, QMetaType::QString,   84,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   84,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::QVariantList, QMetaType::QString,  102,
    QMetaType::QVariantList,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,  105,  102,
    QMetaType::Void, QMetaType::QString,  105,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,  107,  102,
    QMetaType::Void, QMetaType::QString,  107,
    QMetaType::Void, QMetaType::QString,   84,
    QMetaType::Void, QMetaType::QString,  111,
    QMetaType::QVariantMap, QMetaType::QString,   84,
    QMetaType::Void, QMetaType::QString,  114,
    QMetaType::Int,
    QMetaType::Void, QMetaType::Bool,  117,
    QMetaType::Void, QMetaType::Bool,    2,
    QMetaType::Void, QMetaType::QString,    2,
    QMetaType::Void, QMetaType::QString,    2,
    QMetaType::Bool, QMetaType::QString,   84,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    2,
    QMetaType::Void, QMetaType::QString,    2,
    QMetaType::Void, QMetaType::QString,    2,
    QMetaType::Void, QMetaType::QString,    2,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    2,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   84,
    QMetaType::Void, QMetaType::QString,   84,
    QMetaType::Void, QMetaType::QString,   84,
    QMetaType::Bool, QMetaType::QString, QMetaType::QString,  145,  146,
    QMetaType::Bool, QMetaType::QString, QMetaType::QString,  148,  146,
    QMetaType::QString, QMetaType::QString,   84,
    QMetaType::Void, QMetaType::QString,    2,

 // properties: name, type, flags
      68, QMetaType::QString, 0x00015001, uint(0), 0,
     151, QMetaType::Bool, 0x00015001, uint(0), 0,
     152, QMetaType::QString, 0x00015001, uint(0), 0,
     153, QMetaType::QString, 0x00015001, uint(1), 0,
     154, QMetaType::QStringList, 0x00015001, uint(2), 0,
     155, QMetaType::Int, 0x00015103, uint(35), 0,
     156, QMetaType::QString, 0x00015001, uint(3), 0,
     157, QMetaType::QString, 0x00015001, uint(3), 0,
     158, QMetaType::Int, 0x00015001, uint(3), 0,
     159, QMetaType::Bool, 0x00015001, uint(3), 0,
     160, QMetaType::Bool, 0x00015001, uint(3), 0,
     161, QMetaType::Int, 0x00015001, uint(5), 0,
     162, QMetaType::Int, 0x00015001, uint(5), 0,
     163, QMetaType::Bool, 0x00015001, uint(6), 0,
     164, QMetaType::Bool, 0x00015001, uint(7), 0,
     165, QMetaType::QVariantList, 0x00015001, uint(3), 0,
     166, QMetaType::QString, 0x00015001, uint(11), 0,
     167, QMetaType::QStringList, 0x00015001, uint(8), 0,
     168, QMetaType::QVariantList, 0x00015001, uint(8), 0,
     169, QMetaType::QStringList, 0x00015001, uint(8), 0,
     170, QMetaType::QStringList, 0x00015001, uint(8), 0,
     171, QMetaType::QStringList, 0x00015001, uint(8), 0,
     172, QMetaType::QStringList, 0x00015001, uint(10), 0,
     173, QMetaType::Bool, 0x00015001, uint(12), 0,
     174, QMetaType::Int, 0x00015001, uint(13), 0,
     175, QMetaType::Int, 0x00015001, uint(14), 0,
     176, QMetaType::QString, 0x00015001, uint(15), 0,
     177, QMetaType::QString, 0x00015001, uint(12), 0,
     178, QMetaType::QString, 0x00015001, uint(17), 0,
     179, QMetaType::LongLong, 0x00015001, uint(19), 0,
     180, QMetaType::LongLong, 0x00015001, uint(16), 0,
     181, QMetaType::LongLong, 0x00015001, uint(16), 0,
     182, QMetaType::Bool, 0x00015001, uint(31), 0,
     183, QMetaType::Bool, 0x00015001, uint(21), 0,
     184, QMetaType::Int, 0x00015001, uint(20), 0,
     185, QMetaType::QString, 0x00015001, uint(20), 0,
     186, QMetaType::Bool, 0x00015001, uint(24), 0,
     187, QMetaType::QString, 0x00015401, uint(-1), 0,
     188, QMetaType::QString, 0x00015401, uint(-1), 0,
     189, QMetaType::Bool, 0x00015001, uint(25), 0,
     190, QMetaType::QString, 0x00015001, uint(33), 0,
     191, QMetaType::QString, 0x00015401, uint(-1), 0,
     111, QMetaType::QString, 0x00015001, uint(34), 0,
     192, QMetaType::Bool, 0x00015401, uint(-1), 0,
     193, QMetaType::QVariantList, 0x00015001, uint(9), 0,
     194, QMetaType::QString, 0x00015401, uint(-1), 0,
     195, QMetaType::QVariantList, 0x00015401, uint(-1), 0,
     196, QMetaType::QVariantList, 0x00015401, uint(-1), 0,
     197, QMetaType::QVariantMap, 0x00015001, uint(33), 0,
     198, QMetaType::QVariantList, 0x00015401, uint(-1), 0,
     199, QMetaType::LongLong, 0x00015401, uint(-1), 0,
     200, QMetaType::Int, 0x00015401, uint(-1), 0,
     201, QMetaType::Bool, 0x00015001, uint(6), 0,
     202, QMetaType::Bool, 0x00015001, uint(5), 0,
     203, QMetaType::QVariantMap, 0x00015001, uint(5), 0,
     204, QMetaType::QString, 0x00015401, uint(-1), 0,
     205, QMetaType::QString, 0x00015401, uint(-1), 0,
     206, QMetaType::QString, 0x00015401, uint(-1), 0,
     207, QMetaType::Bool, 0x00015001, uint(28), 0,
     208, QMetaType::Int, 0x00015001, uint(29), 0,
     209, QMetaType::Int, 0x00015001, uint(30), 0,
     210, QMetaType::Bool, 0x00015401, uint(-1), 0,
     211, QMetaType::QString, 0x00015401, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::ShadowBackend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSShadowLauncherSCOPEShadowBackendENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSShadowLauncherSCOPEShadowBackendENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSShadowLauncherSCOPEShadowBackendENDCLASS_t,
        // property 'username'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'isOnline'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'accountUuid'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'skinPath'
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
        // property 'closeAfterLaunch'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'isolationEnabled'
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
        // property 'installedVersions'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'installing'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'installProgress'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'installTotal'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'installFile'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'installVersionId'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'installPhase'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'installSpeed'
        QtPrivate::TypeAndForceComplete<qint64, std::true_type>,
        // property 'installBytesDownloaded'
        QtPrivate::TypeAndForceComplete<qint64, std::true_type>,
        // property 'installBytesTotal'
        QtPrivate::TypeAndForceComplete<qint64, std::true_type>,
        // property 'installPaused'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'launching'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'launchProgress'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'launchStatus'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'isRunning'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'launchVersion'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'launchUsername'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'downloading'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'gameDir'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'dataDir'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'theme'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'devMode'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'versionDetails'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // property 'currentVersionSummary'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
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
        // property 'closeOnLaunch'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'autoMemoryEnabled'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'systemMemoryInfo'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::true_type>,
        // property 'gameArgs'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'jvmArgs'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'javaCompatibility'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'verifyRunning'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'verifyChecked'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'verifyTotal'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'verifyResultOk'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'verifyResultText'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<ShadowBackend, std::true_type>,
        // method 'accountChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'skinReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'offlineHistoryChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'javaPathChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'javaReadyChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'memorySettingsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'generalSettingsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'isolationChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'versionListReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'versionDetailsReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'installedVersionsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectedVersionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'installStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'installProgressChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'installTotalChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'installFileProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'installBytesProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        // method 'installPhaseChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'installFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'installSpeedChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
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
        // method 'isRunningChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'resourceDownloadStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
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
        // method 'verifyCheckedChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'verifyTotalChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'installPausedChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'searchResultsReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantList &, std::false_type>,
        // method 'gameDirChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'themeChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'loginModeChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'logMessage'
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
        // method 'listMods'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        // method 'listResourcePacks'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        // method 'listSaves'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        // method 'offlineLogin'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'logout'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'getSkinUrl'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'getSkinUrl'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
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
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
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
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'installVersion'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cancelInstall'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'launch'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cancelLaunch'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'killGameProcess'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'killMinecraft'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
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
        // method 'downloadMod'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'downloadShader'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
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
        // method 'setCloseOnLaunch'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'setAutoMemoryEnabled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'setGameArgs'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setJvmArgs'
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
        // method 'deleteResourcePack'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'deleteSave'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'migrateVersion'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'openConfigFolder'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openCrashLog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openLatestLog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openLogsFolder'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openModsFolder'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openResourcePacksFolder'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openSavesFolder'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openScreenshotsFolder'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openShaderPacksFolder'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'removeGameDir'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'pauseInstall'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'resumeInstall'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
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
        // method 'cloneVersion'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'copyVersionPath'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'repairVersion'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::ShadowBackend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ShadowBackend *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->accountChanged(); break;
        case 1: _t->skinReady(); break;
        case 2: _t->offlineHistoryChanged(); break;
        case 3: _t->javaPathChanged(); break;
        case 4: _t->javaReadyChanged(); break;
        case 5: _t->memorySettingsChanged(); break;
        case 6: _t->generalSettingsChanged(); break;
        case 7: _t->isolationChanged(); break;
        case 8: _t->versionListReady(); break;
        case 9: _t->versionDetailsReady(); break;
        case 10: _t->installedVersionsChanged(); break;
        case 11: _t->selectedVersionChanged(); break;
        case 12: _t->installStateChanged(); break;
        case 13: _t->installProgressChanged(); break;
        case 14: _t->installTotalChanged(); break;
        case 15: _t->installFileProgress(); break;
        case 16: _t->installBytesProgress((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[2]))); break;
        case 17: _t->installPhaseChanged(); break;
        case 18: _t->installFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 19: _t->installSpeedChanged(); break;
        case 20: _t->launchProgressChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 21: _t->launchStateChanged(); break;
        case 22: _t->minecraftStarted(); break;
        case 23: _t->minecraftStopped(); break;
        case 24: _t->isRunningChanged(); break;
        case 25: _t->resourceDownloadStateChanged(); break;
        case 26: _t->resourceDownloadProgress((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 27: _t->resourceDownloadDone((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 28: _t->verifyRunningChanged(); break;
        case 29: _t->verifyCheckedChanged(); break;
        case 30: _t->verifyTotalChanged(); break;
        case 31: _t->installPausedChanged(); break;
        case 32: _t->searchResultsReady((*reinterpret_cast< std::add_pointer_t<QVariantList>>(_a[1]))); break;
        case 33: _t->gameDirChanged(); break;
        case 34: _t->themeChanged(); break;
        case 35: _t->loginModeChanged(); break;
        case 36: _t->logMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 37: _t->verifyStarted(); break;
        case 38: _t->verifyProgress((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 39: _t->verifyFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 40: _t->verifyFailedFiles((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 41: _t->launchCheckProgress((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 42: _t->launchCheckFailed((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 43: _t->launchCheckMissingFiles((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 44: _t->launchCheckWarning((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 45: { QVariantList _r = _t->listMods();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 46: { QVariantList _r = _t->listResourcePacks();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 47: { QVariantList _r = _t->listSaves();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 48: _t->offlineLogin((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 49: _t->logout(); break;
        case 50: { QString _r = _t->getSkinUrl((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 51: { QString _r = _t->getSkinUrl();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 52: { QVariantList _r = _t->scanJavaInstallations();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 53: { QString _r = _t->autoSelectJava();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 54: { QString _r = _t->detectJava();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 55: { QString _r = _t->browseJava();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 56: _t->selectJavaByIndex((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 57: { QVariantMap _r = _t->getMemoryStatus();
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 58: _t->setMinMemory((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 59: _t->setMaxMemory((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 60: _t->setIsolationEnabled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 61: { QString _r = _t->getVersionGameDir((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 62: _t->migrateVersionToIsolated((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 63: _t->openGameDir(); break;
        case 64: _t->openVersionDir((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 65: _t->deleteVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 66: _t->refreshVersionList(); break;
        case 67: _t->refreshInstalled(); break;
        case 68: _t->refreshInstalledList(); break;
        case 69: _t->refreshVersionDetails(); break;
        case 70: _t->refreshGameDirInfo(); break;
        case 71: _t->installVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 72: _t->installVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 73: _t->cancelInstall(); break;
        case 74: _t->launch((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 75: _t->cancelLaunch(); break;
        case 76: _t->killGameProcess(); break;
        case 77: _t->killMinecraft(); break;
        case 78: { QVariantList _r = _t->getPopularMods((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 79: { QVariantList _r = _t->getShaderList();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 80: _t->searchMods((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 81: _t->searchMods((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 82: _t->downloadMod((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 83: _t->downloadShader((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 84: _t->setSelectedVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 85: _t->setTheme((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 86: { QVariantMap _r = _t->checkAll((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 87: _t->setGameDir((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 88: { int _r = _t->getAutoMemory();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 89: _t->setCloseOnLaunch((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 90: _t->setAutoMemoryEnabled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 91: _t->setGameArgs((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 92: _t->setJvmArgs((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 93: { bool _r = _t->isModdedVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 94: _t->openJavaFileDialog(); break;
        case 95: _t->pickJava(); break;
        case 96: _t->checkFileChanges(); break;
        case 97: _t->deleteMod((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 98: _t->deleteResourcePack((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 99: _t->deleteSave((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 100: _t->migrateVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 101: _t->openConfigFolder(); break;
        case 102: _t->openCrashLog(); break;
        case 103: _t->openLatestLog(); break;
        case 104: _t->openLogsFolder(); break;
        case 105: _t->openModsFolder(); break;
        case 106: _t->openResourcePacksFolder(); break;
        case 107: _t->openSavesFolder(); break;
        case 108: _t->openScreenshotsFolder(); break;
        case 109: _t->openShaderPacksFolder(); break;
        case 110: _t->removeGameDir((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 111: _t->pauseInstall(); break;
        case 112: _t->resumeInstall(); break;
        case 113: _t->cancelQueuedDownload((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 114: _t->verifyVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 115: _t->cleanCorruptVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 116: { bool _r = _t->renameVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 117: { bool _r = _t->cloneVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 118: { QString _r = _t->copyVersionPath((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 119: _t->repairVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::accountChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::skinReady; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::offlineHistoryChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::javaPathChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::javaReadyChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::memorySettingsChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::generalSettingsChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::isolationChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::versionListReady; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 8;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::versionDetailsReady; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 9;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::installedVersionsChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 10;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::selectedVersionChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 11;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::installStateChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 12;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::installProgressChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 13;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::installTotalChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 14;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::installFileProgress; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 15;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)(qint64 , qint64 );
            if (_t _q_method = &ShadowBackend::installBytesProgress; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 16;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::installPhaseChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 17;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)(bool );
            if (_t _q_method = &ShadowBackend::installFinished; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 18;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::installSpeedChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 19;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)(int , const QString & );
            if (_t _q_method = &ShadowBackend::launchProgressChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 20;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::launchStateChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 21;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::minecraftStarted; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 22;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::minecraftStopped; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 23;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::isRunningChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 24;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::resourceDownloadStateChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 25;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)(int , int , const QString & );
            if (_t _q_method = &ShadowBackend::resourceDownloadProgress; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 26;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)(bool );
            if (_t _q_method = &ShadowBackend::resourceDownloadDone; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 27;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::verifyRunningChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 28;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::verifyCheckedChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 29;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::verifyTotalChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 30;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::installPausedChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 31;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)(const QVariantList & );
            if (_t _q_method = &ShadowBackend::searchResultsReady; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 32;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::gameDirChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 33;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::themeChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 34;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::loginModeChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 35;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)(const QString & );
            if (_t _q_method = &ShadowBackend::logMessage; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 36;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)();
            if (_t _q_method = &ShadowBackend::verifyStarted; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 37;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)(int , int );
            if (_t _q_method = &ShadowBackend::verifyProgress; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 38;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)(bool );
            if (_t _q_method = &ShadowBackend::verifyFinished; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 39;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)(const QStringList & );
            if (_t _q_method = &ShadowBackend::verifyFailedFiles; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 40;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)(const QString & );
            if (_t _q_method = &ShadowBackend::launchCheckProgress; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 41;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)(const QString & , const QString & );
            if (_t _q_method = &ShadowBackend::launchCheckFailed; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 42;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)(const QStringList & );
            if (_t _q_method = &ShadowBackend::launchCheckMissingFiles; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 43;
                return;
            }
        }
        {
            using _t = void (ShadowBackend::*)(const QString & );
            if (_t _q_method = &ShadowBackend::launchCheckWarning; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 44;
                return;
            }
        }
    }else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<ShadowBackend *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->username(); break;
        case 1: *reinterpret_cast< bool*>(_v) = _t->isOnline(); break;
        case 2: *reinterpret_cast< QString*>(_v) = _t->accountUuid(); break;
        case 3: *reinterpret_cast< QString*>(_v) = _t->skinPath(); break;
        case 4: *reinterpret_cast< QStringList*>(_v) = _t->offlineUsernames(); break;
        case 5: *reinterpret_cast< int*>(_v) = _t->lastLoginMode(); break;
        case 6: *reinterpret_cast< QString*>(_v) = _t->javaPath(); break;
        case 7: *reinterpret_cast< QString*>(_v) = _t->javaVersion(); break;
        case 8: *reinterpret_cast< int*>(_v) = _t->javaMajor(); break;
        case 9: *reinterpret_cast< bool*>(_v) = _t->javaInstalled(); break;
        case 10: *reinterpret_cast< bool*>(_v) = _t->javaInstalled(); break;
        case 11: *reinterpret_cast< int*>(_v) = _t->minMemoryMb(); break;
        case 12: *reinterpret_cast< int*>(_v) = _t->maxMemoryMb(); break;
        case 13: *reinterpret_cast< bool*>(_v) = _t->closeAfterLaunch(); break;
        case 14: *reinterpret_cast< bool*>(_v) = _t->isolationEnabled(); break;
        case 15: *reinterpret_cast< QVariantList*>(_v) = _t->availableJavaList(); break;
        case 16: *reinterpret_cast< QString*>(_v) = _t->selectedVersion(); break;
        case 17: *reinterpret_cast< QStringList*>(_v) = _t->versionIds(); break;
        case 18: *reinterpret_cast< QVariantList*>(_v) = _t->versionList(); break;
        case 19: *reinterpret_cast< QStringList*>(_v) = _t->releaseVersions(); break;
        case 20: *reinterpret_cast< QStringList*>(_v) = _t->snapshotVersions(); break;
        case 21: *reinterpret_cast< QStringList*>(_v) = _t->oldVersions(); break;
        case 22: *reinterpret_cast< QStringList*>(_v) = _t->installedVersions(); break;
        case 23: *reinterpret_cast< bool*>(_v) = _t->isInstalling(); break;
        case 24: *reinterpret_cast< int*>(_v) = _t->installProgress(); break;
        case 25: *reinterpret_cast< int*>(_v) = _t->installTotal(); break;
        case 26: *reinterpret_cast< QString*>(_v) = _t->installFile(); break;
        case 27: *reinterpret_cast< QString*>(_v) = _t->installVersionId(); break;
        case 28: *reinterpret_cast< QString*>(_v) = _t->installPhase(); break;
        case 29: *reinterpret_cast< qint64*>(_v) = _t->installSpeed(); break;
        case 30: *reinterpret_cast< qint64*>(_v) = _t->installBytesDownloaded(); break;
        case 31: *reinterpret_cast< qint64*>(_v) = _t->installBytesTotal(); break;
        case 32: *reinterpret_cast< bool*>(_v) = _t->installPaused(); break;
        case 33: *reinterpret_cast< bool*>(_v) = _t->isLaunching(); break;
        case 34: *reinterpret_cast< int*>(_v) = _t->launchProgress(); break;
        case 35: *reinterpret_cast< QString*>(_v) = _t->launchStatus(); break;
        case 36: *reinterpret_cast< bool*>(_v) = _t->isRunning(); break;
        case 37: *reinterpret_cast< QString*>(_v) = _t->launchVersion(); break;
        case 38: *reinterpret_cast< QString*>(_v) = _t->launchUsername(); break;
        case 39: *reinterpret_cast< bool*>(_v) = _t->isResourceDownloading(); break;
        case 40: *reinterpret_cast< QString*>(_v) = _t->gameDir(); break;
        case 41: *reinterpret_cast< QString*>(_v) = _t->appDataDir(); break;
        case 42: *reinterpret_cast< QString*>(_v) = _t->theme(); break;
        case 43: *reinterpret_cast< bool*>(_v) = _t->devMode(); break;
        case 44: *reinterpret_cast< QVariantList*>(_v) = _t->versionDetails(); break;
        case 45: *reinterpret_cast< QString*>(_v) = _t->currentVersionSummary(); break;
        case 46: *reinterpret_cast< QVariantList*>(_v) = _t->downloadQueue(); break;
        case 47: *reinterpret_cast< QVariantList*>(_v) = _t->activeDownloads(); break;
        case 48: *reinterpret_cast< QVariantMap*>(_v) = _t->gameDirInfo(); break;
        case 49: *reinterpret_cast< QVariantList*>(_v) = _t->gameDirectories(); break;
        case 50: *reinterpret_cast< qint64*>(_v) = _t->diskFree(); break;
        case 51: *reinterpret_cast< int*>(_v) = _t->diskPercent(); break;
        case 52: *reinterpret_cast< bool*>(_v) = _t->closeOnLaunch(); break;
        case 53: *reinterpret_cast< bool*>(_v) = _t->autoMemoryEnabled(); break;
        case 54: *reinterpret_cast< QVariantMap*>(_v) = _t->systemMemoryInfo(); break;
        case 55: *reinterpret_cast< QString*>(_v) = _t->gameArgs(); break;
        case 56: *reinterpret_cast< QString*>(_v) = _t->jvmArgs(); break;
        case 57: *reinterpret_cast< QString*>(_v) = _t->javaCompatibility(); break;
        case 58: *reinterpret_cast< bool*>(_v) = _t->verifyRunning(); break;
        case 59: *reinterpret_cast< int*>(_v) = _t->verifyChecked(); break;
        case 60: *reinterpret_cast< int*>(_v) = _t->verifyTotal(); break;
        case 61: *reinterpret_cast< bool*>(_v) = _t->verifyResultOk(); break;
        case 62: *reinterpret_cast< QString*>(_v) = _t->verifyResultText(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
        auto *_t = static_cast<ShadowBackend *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 5: _t->setLastLoginMode(*reinterpret_cast< int*>(_v)); break;
        default: break;
        }
    } else if (_c == QMetaObject::ResetProperty) {
    } else if (_c == QMetaObject::BindableProperty) {
    }
}

const QMetaObject *ShadowLauncher::ShadowBackend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::ShadowBackend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSShadowLauncherSCOPEShadowBackendENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::ShadowBackend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 120)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 120;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 120)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 120;
    }else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 63;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::ShadowBackend::accountChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ShadowLauncher::ShadowBackend::skinReady()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ShadowLauncher::ShadowBackend::offlineHistoryChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void ShadowLauncher::ShadowBackend::javaPathChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void ShadowLauncher::ShadowBackend::javaReadyChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void ShadowLauncher::ShadowBackend::memorySettingsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void ShadowLauncher::ShadowBackend::generalSettingsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void ShadowLauncher::ShadowBackend::isolationChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void ShadowLauncher::ShadowBackend::versionListReady()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void ShadowLauncher::ShadowBackend::versionDetailsReady()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void ShadowLauncher::ShadowBackend::installedVersionsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}

// SIGNAL 11
void ShadowLauncher::ShadowBackend::selectedVersionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 11, nullptr);
}

// SIGNAL 12
void ShadowLauncher::ShadowBackend::installStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 12, nullptr);
}

// SIGNAL 13
void ShadowLauncher::ShadowBackend::installProgressChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 13, nullptr);
}

// SIGNAL 14
void ShadowLauncher::ShadowBackend::installTotalChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 14, nullptr);
}

// SIGNAL 15
void ShadowLauncher::ShadowBackend::installFileProgress()
{
    QMetaObject::activate(this, &staticMetaObject, 15, nullptr);
}

// SIGNAL 16
void ShadowLauncher::ShadowBackend::installBytesProgress(qint64 _t1, qint64 _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 16, _a);
}

// SIGNAL 17
void ShadowLauncher::ShadowBackend::installPhaseChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 17, nullptr);
}

// SIGNAL 18
void ShadowLauncher::ShadowBackend::installFinished(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 18, _a);
}

// SIGNAL 19
void ShadowLauncher::ShadowBackend::installSpeedChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 19, nullptr);
}

// SIGNAL 20
void ShadowLauncher::ShadowBackend::launchProgressChanged(int _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 20, _a);
}

// SIGNAL 21
void ShadowLauncher::ShadowBackend::launchStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 21, nullptr);
}

// SIGNAL 22
void ShadowLauncher::ShadowBackend::minecraftStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 22, nullptr);
}

// SIGNAL 23
void ShadowLauncher::ShadowBackend::minecraftStopped()
{
    QMetaObject::activate(this, &staticMetaObject, 23, nullptr);
}

// SIGNAL 24
void ShadowLauncher::ShadowBackend::isRunningChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 24, nullptr);
}

// SIGNAL 25
void ShadowLauncher::ShadowBackend::resourceDownloadStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 25, nullptr);
}

// SIGNAL 26
void ShadowLauncher::ShadowBackend::resourceDownloadProgress(int _t1, int _t2, const QString & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 26, _a);
}

// SIGNAL 27
void ShadowLauncher::ShadowBackend::resourceDownloadDone(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 27, _a);
}

// SIGNAL 28
void ShadowLauncher::ShadowBackend::verifyRunningChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 28, nullptr);
}

// SIGNAL 29
void ShadowLauncher::ShadowBackend::verifyCheckedChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 29, nullptr);
}

// SIGNAL 30
void ShadowLauncher::ShadowBackend::verifyTotalChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 30, nullptr);
}

// SIGNAL 31
void ShadowLauncher::ShadowBackend::installPausedChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 31, nullptr);
}

// SIGNAL 32
void ShadowLauncher::ShadowBackend::searchResultsReady(const QVariantList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 32, _a);
}

// SIGNAL 33
void ShadowLauncher::ShadowBackend::gameDirChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 33, nullptr);
}

// SIGNAL 34
void ShadowLauncher::ShadowBackend::themeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 34, nullptr);
}

// SIGNAL 35
void ShadowLauncher::ShadowBackend::loginModeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 35, nullptr);
}

// SIGNAL 36
void ShadowLauncher::ShadowBackend::logMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 36, _a);
}

// SIGNAL 37
void ShadowLauncher::ShadowBackend::verifyStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 37, nullptr);
}

// SIGNAL 38
void ShadowLauncher::ShadowBackend::verifyProgress(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 38, _a);
}

// SIGNAL 39
void ShadowLauncher::ShadowBackend::verifyFinished(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 39, _a);
}

// SIGNAL 40
void ShadowLauncher::ShadowBackend::verifyFailedFiles(const QStringList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 40, _a);
}

// SIGNAL 41
void ShadowLauncher::ShadowBackend::launchCheckProgress(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 41, _a);
}

// SIGNAL 42
void ShadowLauncher::ShadowBackend::launchCheckFailed(const QString & _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 42, _a);
}

// SIGNAL 43
void ShadowLauncher::ShadowBackend::launchCheckMissingFiles(const QStringList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 43, _a);
}

// SIGNAL 44
void ShadowLauncher::ShadowBackend::launchCheckWarning(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 44, _a);
}
QT_WARNING_POP
