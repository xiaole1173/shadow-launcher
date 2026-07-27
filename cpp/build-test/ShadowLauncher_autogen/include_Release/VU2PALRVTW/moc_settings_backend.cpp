/****************************************************************************
** Meta object code from reading C++ file 'settings_backend.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/backend/settings_backend.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'settings_backend.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14ShadowLauncher15SettingsBackendE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN14ShadowLauncher15SettingsBackendE = QtMocHelpers::stringData(
    "ShadowLauncher::SettingsBackend",
    "javaPathChanged",
    "",
    "javaReadyChanged",
    "memorySettingsChanged",
    "generalSettingsChanged",
    "isolationChanged",
    "embeddedLoginChanged",
    "languageChanged",
    "customBgChanged",
    "downloadSettingsChanged",
    "autoLangModeChanged",
    "windowSettingsChanged",
    "logMessage",
    "msg",
    "scanJavaInstallations",
    "QVariantList",
    "isJavaScanning",
    "autoSelectJava",
    "detectJava",
    "getMemoryStatus",
    "QVariantMap",
    "setMinMemory",
    "mb",
    "setMaxMemory",
    "setAutoMemoryEnabled",
    "enabled",
    "versionMemoryMode",
    "versionId",
    "setVersionMemoryMode",
    "mode",
    "versionMemoryManualMB",
    "setVersionMemoryManualMB",
    "versionJavaMode",
    "setVersionJavaMode",
    "versionJvmArgsMode",
    "setVersionJvmArgsMode",
    "versionJvmArgs",
    "setVersionJvmArgs",
    "args",
    "versionGameArgsMode",
    "setVersionGameArgsMode",
    "versionGameArgs",
    "setVersionGameArgs",
    "versionHighPerfGpuMode",
    "setVersionHighPerfGpuMode",
    "versionHighPerfGpu",
    "setVersionHighPerfGpu",
    "v",
    "availableJavaList",
    "selectJavaByIndex",
    "index",
    "findJavaForVersion",
    "requiredMajor",
    "getJavaMajorVersion",
    "path",
    "openJavaFileDialog",
    "browseJava",
    "setIsolationEnabled",
    "migrateVersionToIsolated",
    "getVersionGameDir",
    "isolationEnabled",
    "openGameDir",
    "openVersionDir",
    "deleteVersion",
    "openPath",
    "setEmbeddedLoginEnabled",
    "setLanguageIndex",
    "idx",
    "restartApp",
    "updateCrop",
    "x",
    "y",
    "setAutoLangMode",
    "javaPath",
    "javaVersion",
    "javaMajor",
    "minMemoryMB",
    "maxMemoryMB",
    "autoMemoryEnabled",
    "javaReady",
    "embeddedLoginEnabled",
    "languageIndex",
    "customBgPath",
    "sidebarOpacity",
    "contentOpacity",
    "cropX",
    "cropY",
    "fileDownloadSource",
    "listDownloadSource",
    "maxDownloadThreads",
    "downloadSpeedLimitMB",
    "autoLangMode",
    "windowWidth",
    "windowHeight"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN14ShadowLauncher15SettingsBackendE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      57,   14, // methods
      23,  503, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      12,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  356,    2, 0x06,   24 /* Public */,
       3,    0,  357,    2, 0x06,   25 /* Public */,
       4,    0,  358,    2, 0x06,   26 /* Public */,
       5,    0,  359,    2, 0x06,   27 /* Public */,
       6,    0,  360,    2, 0x06,   28 /* Public */,
       7,    0,  361,    2, 0x06,   29 /* Public */,
       8,    0,  362,    2, 0x06,   30 /* Public */,
       9,    0,  363,    2, 0x06,   31 /* Public */,
      10,    0,  364,    2, 0x06,   32 /* Public */,
      11,    0,  365,    2, 0x06,   33 /* Public */,
      12,    0,  366,    2, 0x06,   34 /* Public */,
      13,    1,  367,    2, 0x06,   35 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      15,    0,  370,    2, 0x02,   37 /* Public */,
      17,    0,  371,    2, 0x102,   38 /* Public | MethodIsConst  */,
      18,    0,  372,    2, 0x02,   39 /* Public */,
      19,    0,  373,    2, 0x02,   40 /* Public */,
      20,    0,  374,    2, 0x02,   41 /* Public */,
      22,    1,  375,    2, 0x02,   42 /* Public */,
      24,    1,  378,    2, 0x02,   44 /* Public */,
      25,    1,  381,    2, 0x02,   46 /* Public */,
      27,    1,  384,    2, 0x102,   48 /* Public | MethodIsConst  */,
      29,    2,  387,    2, 0x02,   50 /* Public */,
      31,    1,  392,    2, 0x102,   53 /* Public | MethodIsConst  */,
      32,    2,  395,    2, 0x02,   55 /* Public */,
      33,    1,  400,    2, 0x102,   58 /* Public | MethodIsConst  */,
      34,    2,  403,    2, 0x02,   60 /* Public */,
      35,    1,  408,    2, 0x102,   63 /* Public | MethodIsConst  */,
      36,    2,  411,    2, 0x02,   65 /* Public */,
      37,    1,  416,    2, 0x102,   68 /* Public | MethodIsConst  */,
      38,    2,  419,    2, 0x02,   70 /* Public */,
      40,    1,  424,    2, 0x102,   73 /* Public | MethodIsConst  */,
      41,    2,  427,    2, 0x02,   75 /* Public */,
      42,    1,  432,    2, 0x102,   78 /* Public | MethodIsConst  */,
      43,    2,  435,    2, 0x02,   80 /* Public */,
      44,    1,  440,    2, 0x102,   83 /* Public | MethodIsConst  */,
      45,    2,  443,    2, 0x02,   85 /* Public */,
      46,    1,  448,    2, 0x102,   88 /* Public | MethodIsConst  */,
      47,    2,  451,    2, 0x02,   90 /* Public */,
      49,    0,  456,    2, 0x02,   93 /* Public */,
      50,    1,  457,    2, 0x02,   94 /* Public */,
      52,    1,  460,    2, 0x02,   96 /* Public */,
      54,    1,  463,    2, 0x02,   98 /* Public */,
      56,    0,  466,    2, 0x02,  100 /* Public */,
      57,    0,  467,    2, 0x02,  101 /* Public */,
      58,    1,  468,    2, 0x02,  102 /* Public */,
      59,    1,  471,    2, 0x02,  104 /* Public */,
      60,    1,  474,    2, 0x102,  106 /* Public | MethodIsConst  */,
      61,    0,  477,    2, 0x102,  108 /* Public | MethodIsConst  */,
      62,    0,  478,    2, 0x02,  109 /* Public */,
      63,    1,  479,    2, 0x02,  110 /* Public */,
      64,    1,  482,    2, 0x02,  112 /* Public */,
      65,    1,  485,    2, 0x02,  114 /* Public */,
      66,    1,  488,    2, 0x02,  116 /* Public */,
      67,    1,  491,    2, 0x02,  118 /* Public */,
      69,    0,  494,    2, 0x02,  120 /* Public */,
      70,    2,  495,    2, 0x02,  121 /* Public */,
      73,    1,  500,    2, 0x02,  124 /* Public */,

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
    QMetaType::Void, QMetaType::QString,   14,

 // methods: parameters
    0x80000000 | 16,
    QMetaType::Bool,
    QMetaType::QString,
    QMetaType::QString,
    0x80000000 | 21,
    QMetaType::Void, QMetaType::Int,   23,
    QMetaType::Void, QMetaType::Int,   23,
    QMetaType::Void, QMetaType::Bool,   26,
    QMetaType::Int, QMetaType::QString,   28,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,   28,   30,
    QMetaType::Int, QMetaType::QString,   28,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,   28,   23,
    QMetaType::Int, QMetaType::QString,   28,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,   28,   30,
    QMetaType::Int, QMetaType::QString,   28,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,   28,   30,
    QMetaType::QString, QMetaType::QString,   28,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   28,   39,
    QMetaType::Int, QMetaType::QString,   28,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,   28,   30,
    QMetaType::QString, QMetaType::QString,   28,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   28,   39,
    QMetaType::Int, QMetaType::QString,   28,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,   28,   30,
    QMetaType::Bool, QMetaType::QString,   28,
    QMetaType::Void, QMetaType::QString, QMetaType::Bool,   28,   48,
    0x80000000 | 16,
    QMetaType::Void, QMetaType::Int,   51,
    QMetaType::QString, QMetaType::Int,   53,
    QMetaType::Int, QMetaType::QString,   55,
    QMetaType::QString,
    QMetaType::QString,
    QMetaType::Void, QMetaType::Bool,   26,
    QMetaType::Void, QMetaType::QString,   28,
    QMetaType::QString, QMetaType::QString,   28,
    QMetaType::Bool,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   28,
    QMetaType::Void, QMetaType::QString,   28,
    QMetaType::Void, QMetaType::QString,   55,
    QMetaType::Void, QMetaType::Bool,   48,
    QMetaType::Void, QMetaType::Int,   68,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QReal, QMetaType::QReal,   71,   72,
    QMetaType::Void, QMetaType::Int,   30,

 // properties: name, type, flags, notifyId, revision
      74, QMetaType::QString, 0x00015103, uint(0), 0,
      75, QMetaType::QString, 0x00015001, uint(0), 0,
      76, QMetaType::Int, 0x00015001, uint(0), 0,
      77, QMetaType::Int, 0x00015001, uint(2), 0,
      78, QMetaType::Int, 0x00015001, uint(2), 0,
      79, QMetaType::Bool, 0x00015103, uint(2), 0,
      80, QMetaType::Bool, 0x00015001, uint(0), 0,
      61, QMetaType::Bool, 0x00015001, uint(4), 0,
      81, QMetaType::Bool, 0x00015103, uint(5), 0,
      82, QMetaType::Int, 0x00015103, uint(6), 0,
       8, QMetaType::Bool, 0x00015001, uint(6), 0,
      83, QMetaType::QString, 0x00015103, uint(7), 0,
      84, QMetaType::QReal, 0x00015103, uint(7), 0,
      85, QMetaType::QReal, 0x00015103, uint(7), 0,
      86, QMetaType::QReal, 0x00015103, uint(7), 0,
      87, QMetaType::QReal, 0x00015103, uint(7), 0,
      88, QMetaType::Int, 0x00015103, uint(8), 0,
      89, QMetaType::Int, 0x00015103, uint(8), 0,
      90, QMetaType::Int, 0x00015103, uint(8), 0,
      91, QMetaType::Double, 0x00015103, uint(8), 0,
      92, QMetaType::Int, 0x00015103, uint(9), 0,
      93, QMetaType::Int, 0x00015103, uint(10), 0,
      94, QMetaType::Int, 0x00015103, uint(10), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::SettingsBackend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN14ShadowLauncher15SettingsBackendE.offsetsAndSizes,
    qt_meta_data_ZN14ShadowLauncher15SettingsBackendE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN14ShadowLauncher15SettingsBackendE_t,
        // property 'javaPath'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'javaVersion'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'javaMajor'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'minMemoryMB'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'maxMemoryMB'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'autoMemoryEnabled'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'javaReady'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'isolationEnabled'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'embeddedLoginEnabled'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'languageIndex'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'languageChanged'
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
        // property 'fileDownloadSource'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'listDownloadSource'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'maxDownloadThreads'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'downloadSpeedLimitMB'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'autoLangMode'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'windowWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'windowHeight'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<SettingsBackend, std::true_type>,
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
        // method 'embeddedLoginChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'languageChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'customBgChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'downloadSettingsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'autoLangModeChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'windowSettingsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'logMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'scanJavaInstallations'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        // method 'isJavaScanning'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'autoSelectJava'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'detectJava'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'getMemoryStatus'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::false_type>,
        // method 'setMinMemory'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setMaxMemory'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
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
        // method 'availableJavaList'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        // method 'selectJavaByIndex'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'findJavaForVersion'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'getJavaMajorVersion'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'openJavaFileDialog'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'browseJava'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'setIsolationEnabled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'migrateVersionToIsolated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'getVersionGameDir'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'isolationEnabled'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'openGameDir'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openVersionDir'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'deleteVersion'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'openPath'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setEmbeddedLoginEnabled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'setLanguageIndex'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'restartApp'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateCrop'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qreal, std::false_type>,
        QtPrivate::TypeAndForceComplete<qreal, std::false_type>,
        // method 'setAutoLangMode'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::SettingsBackend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SettingsBackend *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->javaPathChanged(); break;
        case 1: _t->javaReadyChanged(); break;
        case 2: _t->memorySettingsChanged(); break;
        case 3: _t->generalSettingsChanged(); break;
        case 4: _t->isolationChanged(); break;
        case 5: _t->embeddedLoginChanged(); break;
        case 6: _t->languageChanged(); break;
        case 7: _t->customBgChanged(); break;
        case 8: _t->downloadSettingsChanged(); break;
        case 9: _t->autoLangModeChanged(); break;
        case 10: _t->windowSettingsChanged(); break;
        case 11: _t->logMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 12: { QVariantList _r = _t->scanJavaInstallations();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 13: { bool _r = _t->isJavaScanning();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 14: { QString _r = _t->autoSelectJava();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 15: { QString _r = _t->detectJava();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 16: { QVariantMap _r = _t->getMemoryStatus();
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 17: _t->setMinMemory((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 18: _t->setMaxMemory((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 19: _t->setAutoMemoryEnabled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 20: { int _r = _t->versionMemoryMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 21: _t->setVersionMemoryMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 22: { int _r = _t->versionMemoryManualMB((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 23: _t->setVersionMemoryManualMB((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 24: { int _r = _t->versionJavaMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 25: _t->setVersionJavaMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 26: { int _r = _t->versionJvmArgsMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 27: _t->setVersionJvmArgsMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 28: { QString _r = _t->versionJvmArgs((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 29: _t->setVersionJvmArgs((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 30: { int _r = _t->versionGameArgsMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 31: _t->setVersionGameArgsMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 32: { QString _r = _t->versionGameArgs((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 33: _t->setVersionGameArgs((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 34: { int _r = _t->versionHighPerfGpuMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 35: _t->setVersionHighPerfGpuMode((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 36: { bool _r = _t->versionHighPerfGpu((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 37: _t->setVersionHighPerfGpu((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 38: { QVariantList _r = _t->availableJavaList();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 39: _t->selectJavaByIndex((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 40: { QString _r = _t->findJavaForVersion((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 41: { int _r = _t->getJavaMajorVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 42: { QString _r = _t->openJavaFileDialog();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 43: { QString _r = _t->browseJava();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 44: _t->setIsolationEnabled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 45: _t->migrateVersionToIsolated((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 46: { QString _r = _t->getVersionGameDir((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 47: { bool _r = _t->isolationEnabled();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 48: _t->openGameDir(); break;
        case 49: _t->openVersionDir((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 50: _t->deleteVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 51: _t->openPath((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 52: _t->setEmbeddedLoginEnabled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 53: _t->setLanguageIndex((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 54: _t->restartApp(); break;
        case 55: _t->updateCrop((*reinterpret_cast< std::add_pointer_t<qreal>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<qreal>>(_a[2]))); break;
        case 56: _t->setAutoLangMode((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (SettingsBackend::*)();
            if (_q_method_type _q_method = &SettingsBackend::javaPathChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (SettingsBackend::*)();
            if (_q_method_type _q_method = &SettingsBackend::javaReadyChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (SettingsBackend::*)();
            if (_q_method_type _q_method = &SettingsBackend::memorySettingsChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (SettingsBackend::*)();
            if (_q_method_type _q_method = &SettingsBackend::generalSettingsChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (SettingsBackend::*)();
            if (_q_method_type _q_method = &SettingsBackend::isolationChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (SettingsBackend::*)();
            if (_q_method_type _q_method = &SettingsBackend::embeddedLoginChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (SettingsBackend::*)();
            if (_q_method_type _q_method = &SettingsBackend::languageChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _q_method_type = void (SettingsBackend::*)();
            if (_q_method_type _q_method = &SettingsBackend::customBgChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _q_method_type = void (SettingsBackend::*)();
            if (_q_method_type _q_method = &SettingsBackend::downloadSettingsChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 8;
                return;
            }
        }
        {
            using _q_method_type = void (SettingsBackend::*)();
            if (_q_method_type _q_method = &SettingsBackend::autoLangModeChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 9;
                return;
            }
        }
        {
            using _q_method_type = void (SettingsBackend::*)();
            if (_q_method_type _q_method = &SettingsBackend::windowSettingsChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 10;
                return;
            }
        }
        {
            using _q_method_type = void (SettingsBackend::*)(const QString & );
            if (_q_method_type _q_method = &SettingsBackend::logMessage; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 11;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->javaPath(); break;
        case 1: *reinterpret_cast< QString*>(_v) = _t->javaVersion(); break;
        case 2: *reinterpret_cast< int*>(_v) = _t->javaMajor(); break;
        case 3: *reinterpret_cast< int*>(_v) = _t->minMemoryMB(); break;
        case 4: *reinterpret_cast< int*>(_v) = _t->maxMemoryMB(); break;
        case 5: *reinterpret_cast< bool*>(_v) = _t->autoMemoryEnabled(); break;
        case 6: *reinterpret_cast< bool*>(_v) = _t->isJavaReady(); break;
        case 7: *reinterpret_cast< bool*>(_v) = _t->isolationEnabled(); break;
        case 8: *reinterpret_cast< bool*>(_v) = _t->embeddedLoginEnabled(); break;
        case 9: *reinterpret_cast< int*>(_v) = _t->languageIndex(); break;
        case 10: *reinterpret_cast< bool*>(_v) = _t->isLanguageChanged(); break;
        case 11: *reinterpret_cast< QString*>(_v) = _t->customBgPath(); break;
        case 12: *reinterpret_cast< qreal*>(_v) = _t->sidebarOpacity(); break;
        case 13: *reinterpret_cast< qreal*>(_v) = _t->contentOpacity(); break;
        case 14: *reinterpret_cast< qreal*>(_v) = _t->cropX(); break;
        case 15: *reinterpret_cast< qreal*>(_v) = _t->cropY(); break;
        case 16: *reinterpret_cast< int*>(_v) = _t->fileDownloadSource(); break;
        case 17: *reinterpret_cast< int*>(_v) = _t->listDownloadSource(); break;
        case 18: *reinterpret_cast< int*>(_v) = _t->maxDownloadThreads(); break;
        case 19: *reinterpret_cast< double*>(_v) = _t->downloadSpeedLimitMB(); break;
        case 20: *reinterpret_cast< int*>(_v) = _t->autoLangMode(); break;
        case 21: *reinterpret_cast< int*>(_v) = _t->windowWidth(); break;
        case 22: *reinterpret_cast< int*>(_v) = _t->windowHeight(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setJavaPath(*reinterpret_cast< QString*>(_v)); break;
        case 5: _t->setAutoMemoryEnabled(*reinterpret_cast< bool*>(_v)); break;
        case 8: _t->setEmbeddedLoginEnabled(*reinterpret_cast< bool*>(_v)); break;
        case 9: _t->setLanguageIndex(*reinterpret_cast< int*>(_v)); break;
        case 11: _t->setCustomBgPath(*reinterpret_cast< QString*>(_v)); break;
        case 12: _t->setSidebarOpacity(*reinterpret_cast< qreal*>(_v)); break;
        case 13: _t->setContentOpacity(*reinterpret_cast< qreal*>(_v)); break;
        case 14: _t->setCropX(*reinterpret_cast< qreal*>(_v)); break;
        case 15: _t->setCropY(*reinterpret_cast< qreal*>(_v)); break;
        case 16: _t->setFileDownloadSource(*reinterpret_cast< int*>(_v)); break;
        case 17: _t->setListDownloadSource(*reinterpret_cast< int*>(_v)); break;
        case 18: _t->setMaxDownloadThreads(*reinterpret_cast< int*>(_v)); break;
        case 19: _t->setDownloadSpeedLimitMB(*reinterpret_cast< double*>(_v)); break;
        case 20: _t->setAutoLangMode(*reinterpret_cast< int*>(_v)); break;
        case 21: _t->setWindowWidth(*reinterpret_cast< int*>(_v)); break;
        case 22: _t->setWindowHeight(*reinterpret_cast< int*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *ShadowLauncher::SettingsBackend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::SettingsBackend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN14ShadowLauncher15SettingsBackendE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::SettingsBackend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 57)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 57;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 57)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 57;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 23;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::SettingsBackend::javaPathChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ShadowLauncher::SettingsBackend::javaReadyChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ShadowLauncher::SettingsBackend::memorySettingsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void ShadowLauncher::SettingsBackend::generalSettingsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void ShadowLauncher::SettingsBackend::isolationChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void ShadowLauncher::SettingsBackend::embeddedLoginChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void ShadowLauncher::SettingsBackend::languageChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void ShadowLauncher::SettingsBackend::customBgChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void ShadowLauncher::SettingsBackend::downloadSettingsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void ShadowLauncher::SettingsBackend::autoLangModeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void ShadowLauncher::SettingsBackend::windowSettingsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}

// SIGNAL 11
void ShadowLauncher::SettingsBackend::logMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 11, _a);
}
QT_WARNING_POP
