/****************************************************************************
** Meta object code from reading C++ file 'launch_backend.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/backend/launch_backend.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'launch_backend.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSShadowLauncherSCOPELaunchBackendENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSShadowLauncherSCOPELaunchBackendENDCLASS = QtMocHelpers::stringData(
    "ShadowLauncher::LaunchBackend",
    "launchProgressChanged",
    "",
    "progress",
    "status",
    "launchStateChanged",
    "minecraftStarted",
    "minecraftStopped",
    "isRunningChanged",
    "logMessage",
    "msg",
    "launchCheckProgress",
    "step",
    "launchCheckFailed",
    "phase",
    "details",
    "launchCheckMissingFiles",
    "files",
    "launchCheckWarning",
    "warning",
    "onLaunchStarted",
    "onLaunchProgress",
    "message",
    "onLaunchFinished",
    "success",
    "errorMsg",
    "launch",
    "versionId",
    "username",
    "javaPath",
    "maxMemoryMB",
    "cancelLaunch",
    "killGameProcess",
    "getAutoMemory",
    "getSystemMemory",
    "getMemoryStatus",
    "checkJavaArchitecture",
    "checkVersionHasNatives",
    "checkVersionLibraries",
    "launching",
    "launchProgress",
    "launchStatus",
    "isRunning"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSShadowLauncherSCOPELaunchBackendENDCLASS_t {
    uint offsetsAndSizes[86];
    char stringdata0[30];
    char stringdata1[22];
    char stringdata2[1];
    char stringdata3[9];
    char stringdata4[7];
    char stringdata5[19];
    char stringdata6[17];
    char stringdata7[17];
    char stringdata8[17];
    char stringdata9[11];
    char stringdata10[4];
    char stringdata11[20];
    char stringdata12[5];
    char stringdata13[18];
    char stringdata14[6];
    char stringdata15[8];
    char stringdata16[24];
    char stringdata17[6];
    char stringdata18[19];
    char stringdata19[8];
    char stringdata20[16];
    char stringdata21[17];
    char stringdata22[8];
    char stringdata23[17];
    char stringdata24[8];
    char stringdata25[9];
    char stringdata26[7];
    char stringdata27[10];
    char stringdata28[9];
    char stringdata29[9];
    char stringdata30[12];
    char stringdata31[13];
    char stringdata32[16];
    char stringdata33[14];
    char stringdata34[16];
    char stringdata35[16];
    char stringdata36[22];
    char stringdata37[23];
    char stringdata38[22];
    char stringdata39[10];
    char stringdata40[15];
    char stringdata41[13];
    char stringdata42[10];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSShadowLauncherSCOPELaunchBackendENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSShadowLauncherSCOPELaunchBackendENDCLASS_t qt_meta_stringdata_CLASSShadowLauncherSCOPELaunchBackendENDCLASS = {
    {
        QT_MOC_LITERAL(0, 29),  // "ShadowLauncher::LaunchBackend"
        QT_MOC_LITERAL(30, 21),  // "launchProgressChanged"
        QT_MOC_LITERAL(52, 0),  // ""
        QT_MOC_LITERAL(53, 8),  // "progress"
        QT_MOC_LITERAL(62, 6),  // "status"
        QT_MOC_LITERAL(69, 18),  // "launchStateChanged"
        QT_MOC_LITERAL(88, 16),  // "minecraftStarted"
        QT_MOC_LITERAL(105, 16),  // "minecraftStopped"
        QT_MOC_LITERAL(122, 16),  // "isRunningChanged"
        QT_MOC_LITERAL(139, 10),  // "logMessage"
        QT_MOC_LITERAL(150, 3),  // "msg"
        QT_MOC_LITERAL(154, 19),  // "launchCheckProgress"
        QT_MOC_LITERAL(174, 4),  // "step"
        QT_MOC_LITERAL(179, 17),  // "launchCheckFailed"
        QT_MOC_LITERAL(197, 5),  // "phase"
        QT_MOC_LITERAL(203, 7),  // "details"
        QT_MOC_LITERAL(211, 23),  // "launchCheckMissingFiles"
        QT_MOC_LITERAL(235, 5),  // "files"
        QT_MOC_LITERAL(241, 18),  // "launchCheckWarning"
        QT_MOC_LITERAL(260, 7),  // "warning"
        QT_MOC_LITERAL(268, 15),  // "onLaunchStarted"
        QT_MOC_LITERAL(284, 16),  // "onLaunchProgress"
        QT_MOC_LITERAL(301, 7),  // "message"
        QT_MOC_LITERAL(309, 16),  // "onLaunchFinished"
        QT_MOC_LITERAL(326, 7),  // "success"
        QT_MOC_LITERAL(334, 8),  // "errorMsg"
        QT_MOC_LITERAL(343, 6),  // "launch"
        QT_MOC_LITERAL(350, 9),  // "versionId"
        QT_MOC_LITERAL(360, 8),  // "username"
        QT_MOC_LITERAL(369, 8),  // "javaPath"
        QT_MOC_LITERAL(378, 11),  // "maxMemoryMB"
        QT_MOC_LITERAL(390, 12),  // "cancelLaunch"
        QT_MOC_LITERAL(403, 15),  // "killGameProcess"
        QT_MOC_LITERAL(419, 13),  // "getAutoMemory"
        QT_MOC_LITERAL(433, 15),  // "getSystemMemory"
        QT_MOC_LITERAL(449, 15),  // "getMemoryStatus"
        QT_MOC_LITERAL(465, 21),  // "checkJavaArchitecture"
        QT_MOC_LITERAL(487, 22),  // "checkVersionHasNatives"
        QT_MOC_LITERAL(510, 21),  // "checkVersionLibraries"
        QT_MOC_LITERAL(532, 9),  // "launching"
        QT_MOC_LITERAL(542, 14),  // "launchProgress"
        QT_MOC_LITERAL(557, 12),  // "launchStatus"
        QT_MOC_LITERAL(570, 9)   // "isRunning"
    },
    "ShadowLauncher::LaunchBackend",
    "launchProgressChanged",
    "",
    "progress",
    "status",
    "launchStateChanged",
    "minecraftStarted",
    "minecraftStopped",
    "isRunningChanged",
    "logMessage",
    "msg",
    "launchCheckProgress",
    "step",
    "launchCheckFailed",
    "phase",
    "details",
    "launchCheckMissingFiles",
    "files",
    "launchCheckWarning",
    "warning",
    "onLaunchStarted",
    "onLaunchProgress",
    "message",
    "onLaunchFinished",
    "success",
    "errorMsg",
    "launch",
    "versionId",
    "username",
    "javaPath",
    "maxMemoryMB",
    "cancelLaunch",
    "killGameProcess",
    "getAutoMemory",
    "getSystemMemory",
    "getMemoryStatus",
    "checkJavaArchitecture",
    "checkVersionHasNatives",
    "checkVersionLibraries",
    "launching",
    "launchProgress",
    "launchStatus",
    "isRunning"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSShadowLauncherSCOPELaunchBackendENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
      22,   14, // methods
       4,  204, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      10,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    2,  146,    2, 0x06,    5 /* Public */,
       5,    0,  151,    2, 0x06,    8 /* Public */,
       6,    0,  152,    2, 0x06,    9 /* Public */,
       7,    0,  153,    2, 0x06,   10 /* Public */,
       8,    0,  154,    2, 0x06,   11 /* Public */,
       9,    1,  155,    2, 0x06,   12 /* Public */,
      11,    1,  158,    2, 0x06,   14 /* Public */,
      13,    2,  161,    2, 0x06,   16 /* Public */,
      16,    1,  166,    2, 0x06,   19 /* Public */,
      18,    1,  169,    2, 0x06,   21 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      20,    0,  172,    2, 0x08,   23 /* Private */,
      21,    1,  173,    2, 0x08,   24 /* Private */,
      23,    2,  176,    2, 0x08,   26 /* Private */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      26,    4,  181,    2, 0x02,   29 /* Public */,
      31,    0,  190,    2, 0x02,   34 /* Public */,
      32,    0,  191,    2, 0x02,   35 /* Public */,
      33,    0,  192,    2, 0x02,   36 /* Public */,
      34,    0,  193,    2, 0x02,   37 /* Public */,
      35,    0,  194,    2, 0x02,   38 /* Public */,
      36,    1,  195,    2, 0x02,   39 /* Public */,
      37,    1,  198,    2, 0x02,   41 /* Public */,
      38,    1,  201,    2, 0x02,   43 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::QString,    3,    4,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   10,
    QMetaType::Void, QMetaType::QString,   12,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   14,   15,
    QMetaType::Void, QMetaType::QStringList,   17,
    QMetaType::Void, QMetaType::QString,   19,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   22,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,   24,   25,

 // methods: parameters
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::Int,   27,   28,   29,   30,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Int,
    QMetaType::Int,
    QMetaType::QVariantMap,
    QMetaType::QString, QMetaType::QString,   29,
    QMetaType::Bool, QMetaType::QString,   27,
    QMetaType::QStringList, QMetaType::QString,   27,

 // properties: name, type, flags
      39, QMetaType::Bool, 0x00015001, uint(1), 0,
      40, QMetaType::Int, 0x00015001, uint(0), 0,
      41, QMetaType::QString, 0x00015001, uint(0), 0,
      42, QMetaType::Bool, 0x00015001, uint(4), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::LaunchBackend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSShadowLauncherSCOPELaunchBackendENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSShadowLauncherSCOPELaunchBackendENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSShadowLauncherSCOPELaunchBackendENDCLASS_t,
        // property 'launching'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'launchProgress'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'launchStatus'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'isRunning'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<LaunchBackend, std::true_type>,
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
        // method 'logMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
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
        // method 'onLaunchStarted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onLaunchProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onLaunchFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'launch'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'cancelLaunch'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'killGameProcess'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'getAutoMemory'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'getSystemMemory'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'getMemoryStatus'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::false_type>,
        // method 'checkJavaArchitecture'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'checkVersionHasNatives'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'checkVersionLibraries'
        QtPrivate::TypeAndForceComplete<QStringList, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::LaunchBackend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<LaunchBackend *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->launchProgressChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 1: _t->launchStateChanged(); break;
        case 2: _t->minecraftStarted(); break;
        case 3: _t->minecraftStopped(); break;
        case 4: _t->isRunningChanged(); break;
        case 5: _t->logMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->launchCheckProgress((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->launchCheckFailed((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 8: _t->launchCheckMissingFiles((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 9: _t->launchCheckWarning((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 10: _t->onLaunchStarted(); break;
        case 11: _t->onLaunchProgress((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 12: _t->onLaunchFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 13: _t->launch((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        case 14: _t->cancelLaunch(); break;
        case 15: _t->killGameProcess(); break;
        case 16: { int _r = _t->getAutoMemory();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 17: { int _r = _t->getSystemMemory();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 18: { QVariantMap _r = _t->getMemoryStatus();
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 19: { QString _r = _t->checkJavaArchitecture((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 20: { bool _r = _t->checkVersionHasNatives((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 21: { QStringList _r = _t->checkVersionLibraries((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QStringList*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (LaunchBackend::*)(int , const QString & );
            if (_t _q_method = &LaunchBackend::launchProgressChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (LaunchBackend::*)();
            if (_t _q_method = &LaunchBackend::launchStateChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (LaunchBackend::*)();
            if (_t _q_method = &LaunchBackend::minecraftStarted; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (LaunchBackend::*)();
            if (_t _q_method = &LaunchBackend::minecraftStopped; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (LaunchBackend::*)();
            if (_t _q_method = &LaunchBackend::isRunningChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (LaunchBackend::*)(const QString & );
            if (_t _q_method = &LaunchBackend::logMessage; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (LaunchBackend::*)(const QString & );
            if (_t _q_method = &LaunchBackend::launchCheckProgress; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _t = void (LaunchBackend::*)(const QString & , const QString & );
            if (_t _q_method = &LaunchBackend::launchCheckFailed; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _t = void (LaunchBackend::*)(const QStringList & );
            if (_t _q_method = &LaunchBackend::launchCheckMissingFiles; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 8;
                return;
            }
        }
        {
            using _t = void (LaunchBackend::*)(const QString & );
            if (_t _q_method = &LaunchBackend::launchCheckWarning; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 9;
                return;
            }
        }
    }else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<LaunchBackend *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->isLaunching(); break;
        case 1: *reinterpret_cast< int*>(_v) = _t->launchProgress(); break;
        case 2: *reinterpret_cast< QString*>(_v) = _t->launchStatus(); break;
        case 3: *reinterpret_cast< bool*>(_v) = _t->isRunning(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
    } else if (_c == QMetaObject::ResetProperty) {
    } else if (_c == QMetaObject::BindableProperty) {
    }
}

const QMetaObject *ShadowLauncher::LaunchBackend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::LaunchBackend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSShadowLauncherSCOPELaunchBackendENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::LaunchBackend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 22)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 22;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 22)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 22;
    }else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::LaunchBackend::launchProgressChanged(int _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void ShadowLauncher::LaunchBackend::launchStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ShadowLauncher::LaunchBackend::minecraftStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void ShadowLauncher::LaunchBackend::minecraftStopped()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void ShadowLauncher::LaunchBackend::isRunningChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void ShadowLauncher::LaunchBackend::logMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void ShadowLauncher::LaunchBackend::launchCheckProgress(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void ShadowLauncher::LaunchBackend::launchCheckFailed(const QString & _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 8
void ShadowLauncher::LaunchBackend::launchCheckMissingFiles(const QStringList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 8, _a);
}

// SIGNAL 9
void ShadowLauncher::LaunchBackend::launchCheckWarning(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 9, _a);
}
QT_WARNING_POP
