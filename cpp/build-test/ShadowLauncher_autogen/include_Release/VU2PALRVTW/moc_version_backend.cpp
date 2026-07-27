/****************************************************************************
** Meta object code from reading C++ file 'version_backend.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/backend/version_backend.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'version_backend.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14ShadowLauncher16InstallCardModelE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN14ShadowLauncher16InstallCardModelE = QtMocHelpers::stringData(
    "ShadowLauncher::InstallCardModel",
    "generationChanged",
    "",
    "cardData",
    "QVariantMap",
    "row",
    "count",
    "generation"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN14ShadowLauncher16InstallCardModelE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       2,   30, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   26,    2, 0x06,    3 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
       3,    1,   27,    2, 0x102,    4 /* Public | MethodIsConst  */,

 // signals: parameters
    QMetaType::Void,

 // methods: parameters
    0x80000000 | 4, QMetaType::Int,    5,

 // properties: name, type, flags, notifyId, revision
       6, QMetaType::Int, 0x00015001, uint(0), 0,
       7, QMetaType::Int, 0x00015001, uint(0), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::InstallCardModel::staticMetaObject = { {
    QMetaObject::SuperData::link<QAbstractListModel::staticMetaObject>(),
    qt_meta_stringdata_ZN14ShadowLauncher16InstallCardModelE.offsetsAndSizes,
    qt_meta_data_ZN14ShadowLauncher16InstallCardModelE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN14ShadowLauncher16InstallCardModelE_t,
        // property 'count'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'generation'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<InstallCardModel, std::true_type>,
        // method 'generationChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'cardData'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::InstallCardModel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<InstallCardModel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->generationChanged(); break;
        case 1: { QVariantMap _r = _t->cardData((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (InstallCardModel::*)();
            if (_q_method_type _q_method = &InstallCardModel::generationChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< int*>(_v) = _t->count(); break;
        case 1: *reinterpret_cast< int*>(_v) = _t->generation(); break;
        default: break;
        }
    }
}

const QMetaObject *ShadowLauncher::InstallCardModel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::InstallCardModel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN14ShadowLauncher16InstallCardModelE.stringdata0))
        return static_cast<void*>(this);
    return QAbstractListModel::qt_metacast(_clname);
}

int ShadowLauncher::InstallCardModel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QAbstractListModel::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::InstallCardModel::generationChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
namespace {
struct qt_meta_tag_ZN14ShadowLauncher14VersionBackendE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN14ShadowLauncher14VersionBackendE = QtMocHelpers::stringData(
    "ShadowLauncher::VersionBackend",
    "versionListReady",
    "",
    "installedVersionsChanged",
    "selectedVersionChanged",
    "installStateChanged",
    "installPhaseChanged",
    "phase",
    "installFinished",
    "success",
    "installComplete",
    "installId",
    "logMessage",
    "msg",
    "mcJsonReady",
    "versionId",
    "verifyStarted",
    "verifyProgress",
    "checked",
    "total",
    "verifyProgressChanged",
    "verifyFinished",
    "allPassed",
    "verifyFailedFiles",
    "failedFiles",
    "verifyCancelled",
    "repairRunningChanged",
    "downloadQueueChanged",
    "downloadQueueFull",
    "displayName",
    "onVersionDownloadLog",
    "onVersionDownloadFinished",
    "error",
    "setGameDir",
    "dir",
    "setSelectedVersion",
    "refreshVersionList",
    "refreshInstalled",
    "installVersion",
    "cancelInstall",
    "cancelCurrentInstall",
    "cancelVersionInstall",
    "cancelQueuedDownload",
    "downloadQueue",
    "QVariantList",
    "activeDownloads",
    "getVersionGameDir",
    "verifyVersion",
    "cancelVerify",
    "cleanCorruptVersion",
    "repairVersion",
    "renameVersion",
    "oldId",
    "newId",
    "cloneVersion",
    "sourceId",
    "copyVersionPath",
    "setPendingUserDataImport",
    "archivePath",
    "cancelPendingUserDataImport",
    "dismissCard",
    "dismissAllCompleted",
    "installModLoader",
    "mcVersion",
    "loaderType",
    "loaderVersion",
    "installName",
    "fabricApiVersion",
    "fabricApiUrl",
    "fabricApiSavePath",
    "forgeInstallerSha1",
    "forgeInstallerBranch",
    "installOptifine",
    "optifineVersion",
    "forgeVersion",
    "bmclType",
    "bmclPatch",
    "installOptifineJar",
    "cancelModLoaderInstall",
    "isModLoaderInstalling",
    "addResourceCard",
    "cardId",
    "updateResourceCard",
    "progress",
    "status",
    "speed",
    "removeResourceCard",
    "installing",
    "activeCount",
    "installVersionId",
    "installPhase",
    "activeInstallId",
    "installCardsModel",
    "verifyChecked",
    "verifyTotal",
    "versionIds",
    "installedIds",
    "selectedVersion"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN14ShadowLauncher14VersionBackendE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      61,   14, // methods
      11,  653, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      18,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  380,    2, 0x06,   12 /* Public */,
       3,    0,  381,    2, 0x06,   13 /* Public */,
       4,    0,  382,    2, 0x06,   14 /* Public */,
       5,    0,  383,    2, 0x06,   15 /* Public */,
       6,    1,  384,    2, 0x06,   16 /* Public */,
       8,    1,  387,    2, 0x06,   18 /* Public */,
      10,    1,  390,    2, 0x06,   20 /* Public */,
      12,    1,  393,    2, 0x06,   22 /* Public */,
      14,    1,  396,    2, 0x06,   24 /* Public */,
      16,    0,  399,    2, 0x06,   26 /* Public */,
      17,    2,  400,    2, 0x06,   27 /* Public */,
      20,    2,  405,    2, 0x06,   30 /* Public */,
      21,    1,  410,    2, 0x06,   33 /* Public */,
      23,    1,  413,    2, 0x06,   35 /* Public */,
      25,    0,  416,    2, 0x06,   37 /* Public */,
      26,    0,  417,    2, 0x06,   38 /* Public */,
      27,    0,  418,    2, 0x06,   39 /* Public */,
      28,    1,  419,    2, 0x06,   40 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      30,    1,  422,    2, 0x08,   42 /* Private */,
      31,    2,  425,    2, 0x08,   44 /* Private */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      33,    1,  430,    2, 0x02,   47 /* Public */,
      35,    1,  433,    2, 0x02,   49 /* Public */,
      36,    0,  436,    2, 0x02,   51 /* Public */,
      37,    0,  437,    2, 0x02,   52 /* Public */,
      38,    1,  438,    2, 0x02,   53 /* Public */,
      39,    0,  441,    2, 0x02,   55 /* Public */,
      40,    0,  442,    2, 0x02,   56 /* Public */,
      41,    1,  443,    2, 0x02,   57 /* Public */,
      42,    1,  446,    2, 0x02,   59 /* Public */,
      43,    0,  449,    2, 0x102,   61 /* Public | MethodIsConst  */,
      45,    0,  450,    2, 0x102,   62 /* Public | MethodIsConst  */,
      46,    1,  451,    2, 0x102,   63 /* Public | MethodIsConst  */,
      47,    1,  454,    2, 0x02,   65 /* Public */,
      48,    0,  457,    2, 0x02,   67 /* Public */,
      49,    1,  458,    2, 0x02,   68 /* Public */,
      50,    1,  461,    2, 0x02,   70 /* Public */,
      51,    2,  464,    2, 0x02,   72 /* Public */,
      54,    2,  469,    2, 0x02,   75 /* Public */,
      56,    1,  474,    2, 0x02,   78 /* Public */,
      57,    2,  477,    2, 0x02,   80 /* Public */,
      59,    1,  482,    2, 0x02,   83 /* Public */,
      60,    1,  485,    2, 0x02,   85 /* Public */,
      61,    0,  488,    2, 0x02,   87 /* Public */,
      62,    9,  489,    2, 0x02,   88 /* Public */,
      62,    8,  508,    2, 0x22,   98 /* Public | MethodCloned */,
      62,    7,  525,    2, 0x22,  107 /* Public | MethodCloned */,
      62,    6,  540,    2, 0x22,  115 /* Public | MethodCloned */,
      62,    5,  553,    2, 0x22,  122 /* Public | MethodCloned */,
      62,    4,  564,    2, 0x22,  128 /* Public | MethodCloned */,
      72,    6,  573,    2, 0x02,  133 /* Public */,
      72,    5,  586,    2, 0x22,  140 /* Public | MethodCloned */,
      72,    4,  597,    2, 0x22,  146 /* Public | MethodCloned */,
      77,    4,  606,    2, 0x02,  151 /* Public */,
      77,    3,  615,    2, 0x22,  156 /* Public | MethodCloned */,
      77,    2,  622,    2, 0x22,  160 /* Public | MethodCloned */,
      78,    0,  627,    2, 0x02,  163 /* Public */,
      79,    0,  628,    2, 0x102,  164 /* Public | MethodIsConst  */,
      80,    2,  629,    2, 0x02,  165 /* Public */,
      82,    4,  634,    2, 0x02,  168 /* Public */,
      82,    3,  643,    2, 0x22,  173 /* Public | MethodCloned */,
      86,    1,  650,    2, 0x02,  177 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    7,
    QMetaType::Void, QMetaType::Bool,    9,
    QMetaType::Void, QMetaType::QString,   11,
    QMetaType::Void, QMetaType::QString,   13,
    QMetaType::Void, QMetaType::QString,   15,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   18,   19,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   18,   19,
    QMetaType::Void, QMetaType::Bool,   22,
    QMetaType::Void, QMetaType::QStringList,   24,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   29,

 // slots: parameters
    QMetaType::Void, QMetaType::QString,   13,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,    9,   32,

 // methods: parameters
    QMetaType::Void, QMetaType::QString,   34,
    QMetaType::Void, QMetaType::QString,   15,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   15,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   15,
    QMetaType::Void, QMetaType::QString,   15,
    0x80000000 | 44,
    0x80000000 | 44,
    QMetaType::QString, QMetaType::QString,   15,
    QMetaType::Void, QMetaType::QString,   15,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   15,
    QMetaType::Void, QMetaType::QString,   15,
    QMetaType::Bool, QMetaType::QString, QMetaType::QString,   52,   53,
    QMetaType::Bool, QMetaType::QString, QMetaType::QString,   55,   53,
    QMetaType::QString, QMetaType::QString,   15,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   11,   58,
    QMetaType::Void, QMetaType::QString,   11,
    QMetaType::Void, QMetaType::QString,   11,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,   63,   64,   65,   66,   67,   68,   69,   70,   71,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,   63,   64,   65,   66,   67,   68,   69,   70,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,   63,   64,   65,   66,   67,   68,   69,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,   63,   64,   65,   66,   67,   68,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,   63,   64,   65,   66,   67,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,   63,   64,   65,   66,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,   63,   73,   74,   66,   75,   76,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,   63,   73,   74,   66,   75,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,   63,   73,   74,   66,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,   63,   73,   75,   76,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString,   63,   73,   75,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   63,   73,
    QMetaType::Void,
    QMetaType::Bool,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   81,   29,
    QMetaType::Void, QMetaType::QString, QMetaType::QReal, QMetaType::QString, QMetaType::LongLong,   81,   83,   84,   85,
    QMetaType::Void, QMetaType::QString, QMetaType::QReal, QMetaType::QString,   81,   83,   84,
    QMetaType::Void, QMetaType::QString,   81,

 // properties: name, type, flags, notifyId, revision
      87, QMetaType::Bool, 0x00015001, uint(3), 0,
      88, QMetaType::Int, 0x00015001, uint(3), 0,
      89, QMetaType::QString, 0x00015001, uint(3), 0,
      90, QMetaType::QString, 0x00015001, uint(4), 0,
      91, QMetaType::QString, 0x00015001, uint(3), 0,
      92, QMetaType::QObjectStar, 0x00015401, uint(-1), 0,
      93, QMetaType::Int, 0x00015001, uint(11), 0,
      94, QMetaType::Int, 0x00015001, uint(11), 0,
      95, QMetaType::QStringList, 0x00015001, uint(0), 0,
      96, QMetaType::QStringList, 0x00015001, uint(1), 0,
      97, QMetaType::QString, 0x00015001, uint(2), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::VersionBackend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN14ShadowLauncher14VersionBackendE.offsetsAndSizes,
    qt_meta_data_ZN14ShadowLauncher14VersionBackendE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN14ShadowLauncher14VersionBackendE_t,
        // property 'installing'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'activeCount'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'installVersionId'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'installPhase'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'activeInstallId'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'installCardsModel'
        QtPrivate::TypeAndForceComplete<QObject*, std::true_type>,
        // property 'verifyChecked'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'verifyTotal'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'versionIds'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'installedIds'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'selectedVersion'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<VersionBackend, std::true_type>,
        // method 'versionListReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'installedVersionsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectedVersionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'installStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'installPhaseChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'installFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'installComplete'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'logMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'mcJsonReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'verifyStarted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'verifyProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'verifyProgressChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'verifyFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'verifyFailedFiles'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        // method 'verifyCancelled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'repairRunningChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'downloadQueueChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'downloadQueueFull'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onVersionDownloadLog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onVersionDownloadFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setGameDir'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setSelectedVersion'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'refreshVersionList'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'refreshInstalled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'installVersion'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cancelInstall'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'cancelCurrentInstall'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'cancelVersionInstall'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cancelQueuedDownload'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'downloadQueue'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        // method 'activeDownloads'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        // method 'getVersionGameDir'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'verifyVersion'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cancelVerify'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'cleanCorruptVersion'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'repairVersion'
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
        // method 'setPendingUserDataImport'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cancelPendingUserDataImport'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'dismissCard'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'dismissAllCompleted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
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
        // method 'installOptifineJar'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'installOptifineJar'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'installOptifineJar'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cancelModLoaderInstall'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'isModLoaderInstalling'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'addResourceCard'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'updateResourceCard'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qreal, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        // method 'updateResourceCard'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qreal, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'removeResourceCard'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::VersionBackend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<VersionBackend *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->versionListReady(); break;
        case 1: _t->installedVersionsChanged(); break;
        case 2: _t->selectedVersionChanged(); break;
        case 3: _t->installStateChanged(); break;
        case 4: _t->installPhaseChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->installFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 6: _t->installComplete((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->logMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->mcJsonReady((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->verifyStarted(); break;
        case 10: _t->verifyProgress((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 11: _t->verifyProgressChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 12: _t->verifyFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 13: _t->verifyFailedFiles((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 14: _t->verifyCancelled(); break;
        case 15: _t->repairRunningChanged(); break;
        case 16: _t->downloadQueueChanged(); break;
        case 17: _t->downloadQueueFull((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 18: _t->onVersionDownloadLog((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 19: _t->onVersionDownloadFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 20: _t->setGameDir((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 21: _t->setSelectedVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 22: _t->refreshVersionList(); break;
        case 23: _t->refreshInstalled(); break;
        case 24: _t->installVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 25: _t->cancelInstall(); break;
        case 26: _t->cancelCurrentInstall(); break;
        case 27: _t->cancelVersionInstall((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 28: _t->cancelQueuedDownload((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 29: { QVariantList _r = _t->downloadQueue();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 30: { QVariantList _r = _t->activeDownloads();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 31: { QString _r = _t->getVersionGameDir((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 32: _t->verifyVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 33: _t->cancelVerify(); break;
        case 34: _t->cleanCorruptVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 35: _t->repairVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 36: { bool _r = _t->renameVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 37: { bool _r = _t->cloneVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 38: { QString _r = _t->copyVersionPath((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 39: _t->setPendingUserDataImport((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 40: _t->cancelPendingUserDataImport((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 41: _t->dismissCard((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 42: _t->dismissAllCompleted(); break;
        case 43: _t->installModLoader((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[6])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[7])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[8])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[9]))); break;
        case 44: _t->installModLoader((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[6])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[7])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[8]))); break;
        case 45: _t->installModLoader((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[6])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[7]))); break;
        case 46: _t->installModLoader((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[6]))); break;
        case 47: _t->installModLoader((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5]))); break;
        case 48: _t->installModLoader((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4]))); break;
        case 49: _t->installOptifine((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[6]))); break;
        case 50: _t->installOptifine((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5]))); break;
        case 51: _t->installOptifine((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4]))); break;
        case 52: _t->installOptifineJar((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4]))); break;
        case 53: _t->installOptifineJar((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 54: _t->installOptifineJar((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 55: _t->cancelModLoaderInstall(); break;
        case 56: { bool _r = _t->isModLoaderInstalling();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 57: _t->addResourceCard((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 58: _t->updateResourceCard((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<qreal>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[4]))); break;
        case 59: _t->updateResourceCard((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<qreal>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 60: _t->removeResourceCard((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (VersionBackend::*)();
            if (_q_method_type _q_method = &VersionBackend::versionListReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (VersionBackend::*)();
            if (_q_method_type _q_method = &VersionBackend::installedVersionsChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (VersionBackend::*)();
            if (_q_method_type _q_method = &VersionBackend::selectedVersionChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (VersionBackend::*)();
            if (_q_method_type _q_method = &VersionBackend::installStateChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (VersionBackend::*)(const QString & );
            if (_q_method_type _q_method = &VersionBackend::installPhaseChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (VersionBackend::*)(bool );
            if (_q_method_type _q_method = &VersionBackend::installFinished; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (VersionBackend::*)(const QString & );
            if (_q_method_type _q_method = &VersionBackend::installComplete; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _q_method_type = void (VersionBackend::*)(const QString & );
            if (_q_method_type _q_method = &VersionBackend::logMessage; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _q_method_type = void (VersionBackend::*)(const QString & );
            if (_q_method_type _q_method = &VersionBackend::mcJsonReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 8;
                return;
            }
        }
        {
            using _q_method_type = void (VersionBackend::*)();
            if (_q_method_type _q_method = &VersionBackend::verifyStarted; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 9;
                return;
            }
        }
        {
            using _q_method_type = void (VersionBackend::*)(int , int );
            if (_q_method_type _q_method = &VersionBackend::verifyProgress; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 10;
                return;
            }
        }
        {
            using _q_method_type = void (VersionBackend::*)(int , int );
            if (_q_method_type _q_method = &VersionBackend::verifyProgressChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 11;
                return;
            }
        }
        {
            using _q_method_type = void (VersionBackend::*)(bool );
            if (_q_method_type _q_method = &VersionBackend::verifyFinished; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 12;
                return;
            }
        }
        {
            using _q_method_type = void (VersionBackend::*)(const QStringList & );
            if (_q_method_type _q_method = &VersionBackend::verifyFailedFiles; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 13;
                return;
            }
        }
        {
            using _q_method_type = void (VersionBackend::*)();
            if (_q_method_type _q_method = &VersionBackend::verifyCancelled; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 14;
                return;
            }
        }
        {
            using _q_method_type = void (VersionBackend::*)();
            if (_q_method_type _q_method = &VersionBackend::repairRunningChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 15;
                return;
            }
        }
        {
            using _q_method_type = void (VersionBackend::*)();
            if (_q_method_type _q_method = &VersionBackend::downloadQueueChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 16;
                return;
            }
        }
        {
            using _q_method_type = void (VersionBackend::*)(const QString & );
            if (_q_method_type _q_method = &VersionBackend::downloadQueueFull; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 17;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->isInstalling(); break;
        case 1: *reinterpret_cast< int*>(_v) = _t->activeCount(); break;
        case 2: *reinterpret_cast< QString*>(_v) = _t->installVersionId(); break;
        case 3: *reinterpret_cast< QString*>(_v) = _t->installPhase(); break;
        case 4: *reinterpret_cast< QString*>(_v) = _t->activeInstallId(); break;
        case 5: *reinterpret_cast< QObject**>(_v) = _t->installCardsModel(); break;
        case 6: *reinterpret_cast< int*>(_v) = _t->verifyChecked(); break;
        case 7: *reinterpret_cast< int*>(_v) = _t->verifyTotal(); break;
        case 8: *reinterpret_cast< QStringList*>(_v) = _t->versionIds(); break;
        case 9: *reinterpret_cast< QStringList*>(_v) = _t->installedIds(); break;
        case 10: *reinterpret_cast< QString*>(_v) = _t->selectedVersion(); break;
        default: break;
        }
    }
}

const QMetaObject *ShadowLauncher::VersionBackend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::VersionBackend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN14ShadowLauncher14VersionBackendE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::VersionBackend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 61)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 61;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 61)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 61;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::VersionBackend::versionListReady()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ShadowLauncher::VersionBackend::installedVersionsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ShadowLauncher::VersionBackend::selectedVersionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void ShadowLauncher::VersionBackend::installStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void ShadowLauncher::VersionBackend::installPhaseChanged(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void ShadowLauncher::VersionBackend::installFinished(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void ShadowLauncher::VersionBackend::installComplete(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void ShadowLauncher::VersionBackend::logMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 8
void ShadowLauncher::VersionBackend::mcJsonReady(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 8, _a);
}

// SIGNAL 9
void ShadowLauncher::VersionBackend::verifyStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void ShadowLauncher::VersionBackend::verifyProgress(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 10, _a);
}

// SIGNAL 11
void ShadowLauncher::VersionBackend::verifyProgressChanged(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 11, _a);
}

// SIGNAL 12
void ShadowLauncher::VersionBackend::verifyFinished(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 12, _a);
}

// SIGNAL 13
void ShadowLauncher::VersionBackend::verifyFailedFiles(const QStringList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 13, _a);
}

// SIGNAL 14
void ShadowLauncher::VersionBackend::verifyCancelled()
{
    QMetaObject::activate(this, &staticMetaObject, 14, nullptr);
}

// SIGNAL 15
void ShadowLauncher::VersionBackend::repairRunningChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 15, nullptr);
}

// SIGNAL 16
void ShadowLauncher::VersionBackend::downloadQueueChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 16, nullptr);
}

// SIGNAL 17
void ShadowLauncher::VersionBackend::downloadQueueFull(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 17, _a);
}
namespace {
struct qt_meta_tag_ZN14ShadowLauncher14VersionBackend12VerifyWorkerE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN14ShadowLauncher14VersionBackend12VerifyWorkerE = QtMocHelpers::stringData(
    "ShadowLauncher::VersionBackend::VerifyWorker",
    "progressChecked",
    "",
    "checked",
    "total",
    "cancelled",
    "finished",
    "allPassed",
    "failedFiles",
    "failedPaths",
    "process"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN14ShadowLauncher14VersionBackend12VerifyWorkerE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    2,   38,    2, 0x06,    1 /* Public */,
       5,    2,   43,    2, 0x06,    4 /* Public */,
       6,    3,   48,    2, 0x06,    7 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      10,    0,   55,    2, 0x0a,   11 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    3,    4,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    3,    4,
    QMetaType::Void, QMetaType::Bool, QMetaType::QStringList, QMetaType::QStringList,    7,    8,    9,

 // slots: parameters
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::VersionBackend::VerifyWorker::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN14ShadowLauncher14VersionBackend12VerifyWorkerE.offsetsAndSizes,
    qt_meta_data_ZN14ShadowLauncher14VersionBackend12VerifyWorkerE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN14ShadowLauncher14VersionBackend12VerifyWorkerE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<VerifyWorker, std::true_type>,
        // method 'progressChecked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'cancelled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'finished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        // method 'process'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::VersionBackend::VerifyWorker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<VerifyWorker *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->progressChecked((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 1: _t->cancelled((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 2: _t->finished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[3]))); break;
        case 3: _t->process(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (VerifyWorker::*)(int , int );
            if (_q_method_type _q_method = &VerifyWorker::progressChecked; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (VerifyWorker::*)(int , int );
            if (_q_method_type _q_method = &VerifyWorker::cancelled; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (VerifyWorker::*)(bool , const QStringList & , const QStringList & );
            if (_q_method_type _q_method = &VerifyWorker::finished; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
    }
}

const QMetaObject *ShadowLauncher::VersionBackend::VerifyWorker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::VersionBackend::VerifyWorker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN14ShadowLauncher14VersionBackend12VerifyWorkerE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::VersionBackend::VerifyWorker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::VersionBackend::VerifyWorker::progressChecked(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void ShadowLauncher::VersionBackend::VerifyWorker::cancelled(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void ShadowLauncher::VersionBackend::VerifyWorker::finished(bool _t1, const QStringList & _t2, const QStringList & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}
QT_WARNING_POP
