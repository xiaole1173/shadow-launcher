/****************************************************************************
** Meta object code from reading C++ file 'launch_backend.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/backend/launch_backend.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'launch_backend.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14ShadowLauncher13LaunchBackendE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN14ShadowLauncher13LaunchBackendE = QtMocHelpers::stringData(
    "ShadowLauncher::LaunchBackend",
    "launchProgressChanged",
    "",
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
    "onLaunchProgress",
    "message",
    "runNextCheck",
    "abortCheck",
    "reason",
    "beginTokenRefreshAttempt",
    "launch",
    "versionId",
    "username",
    "javaPath",
    "maxMemoryMB",
    "jvmArgs",
    "gameArgs",
    "highPerfGpu",
    "windowWidth",
    "windowHeight",
    "cancelLaunch",
    "killGameProcess",
    "killGameByPid",
    "pid",
    "runningGames",
    "QVariantList",
    "getAutoMemory",
    "getSystemMemory",
    "getMemoryStatus",
    "checkJavaArchitecture",
    "checkVersionHasNatives",
    "checkVersionMissingNatives",
    "checkVersionLibraries",
    "launching",
    "launchProgress",
    "launchStatus",
    "isRunning",
    "runningCount"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN14ShadowLauncher13LaunchBackendE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      33,   14, // methods
       5,  357, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      12,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    2,  212,    2, 0x06,    6 /* Public */,
       5,    0,  217,    2, 0x06,    9 /* Public */,
       6,    0,  218,    2, 0x06,   10 /* Public */,
       7,    0,  219,    2, 0x06,   11 /* Public */,
       8,    1,  220,    2, 0x06,   12 /* Public */,
      11,    0,  223,    2, 0x06,   14 /* Public */,
      12,    0,  224,    2, 0x06,   15 /* Public */,
      13,    1,  225,    2, 0x06,   16 /* Public */,
      15,    1,  228,    2, 0x06,   18 /* Public */,
      17,    2,  231,    2, 0x06,   20 /* Public */,
      20,    1,  236,    2, 0x06,   23 /* Public */,
      22,    1,  239,    2, 0x06,   25 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      24,    1,  242,    2, 0x08,   27 /* Private */,
      26,    0,  245,    2, 0x08,   29 /* Private */,
      27,    2,  246,    2, 0x08,   30 /* Private */,
      29,    0,  251,    2, 0x08,   33 /* Private */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      30,    9,  252,    2, 0x02,   34 /* Public */,
      30,    8,  271,    2, 0x22,   44 /* Public | MethodCloned */,
      30,    7,  288,    2, 0x22,   53 /* Public | MethodCloned */,
      30,    6,  303,    2, 0x22,   61 /* Public | MethodCloned */,
      30,    5,  316,    2, 0x22,   68 /* Public | MethodCloned */,
      30,    4,  327,    2, 0x22,   74 /* Public | MethodCloned */,
      40,    0,  336,    2, 0x02,   79 /* Public */,
      41,    0,  337,    2, 0x02,   80 /* Public */,
      42,    1,  338,    2, 0x02,   81 /* Public */,
      44,    0,  341,    2, 0x102,   83 /* Public | MethodIsConst  */,
      46,    0,  342,    2, 0x02,   84 /* Public */,
      47,    0,  343,    2, 0x02,   85 /* Public */,
      48,    0,  344,    2, 0x02,   86 /* Public */,
      49,    1,  345,    2, 0x02,   87 /* Public */,
      50,    1,  348,    2, 0x02,   89 /* Public */,
      51,    1,  351,    2, 0x02,   91 /* Public */,
      52,    1,  354,    2, 0x02,   93 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::QString,    3,    4,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 9,   10,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   14,
    QMetaType::Void, QMetaType::QString,   16,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   18,   19,
    QMetaType::Void, QMetaType::QStringList,   21,
    QMetaType::Void, QMetaType::QString,   23,

 // slots: parameters
    QMetaType::Void, QMetaType::QString,   25,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   18,   28,
    QMetaType::Void,

 // methods: parameters
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::Int, QMetaType::QString, QMetaType::QString, QMetaType::Bool, QMetaType::Int, QMetaType::Int,   31,   32,   33,   34,   35,   36,   37,   38,   39,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::Int, QMetaType::QString, QMetaType::QString, QMetaType::Bool, QMetaType::Int,   31,   32,   33,   34,   35,   36,   37,   38,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::Int, QMetaType::QString, QMetaType::QString, QMetaType::Bool,   31,   32,   33,   34,   35,   36,   37,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::Int, QMetaType::QString, QMetaType::QString,   31,   32,   33,   34,   35,   36,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::Int, QMetaType::QString,   31,   32,   33,   34,   35,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::Int,   31,   32,   33,   34,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::LongLong,   43,
    0x80000000 | 45,
    QMetaType::Int,
    QMetaType::Int,
    0x80000000 | 9,
    QMetaType::QString, QMetaType::QString,   33,
    QMetaType::Bool, QMetaType::QString,   31,
    QMetaType::QStringList, QMetaType::QString,   31,
    QMetaType::QStringList, QMetaType::QString,   31,

 // properties: name, type, flags, notifyId, revision
      53, QMetaType::Bool, 0x00015001, uint(1), 0,
      54, QMetaType::Int, 0x00015001, uint(0), 0,
      55, QMetaType::QString, 0x00015001, uint(0), 0,
      56, QMetaType::Bool, 0x00015001, uint(5), 0,
      57, QMetaType::Int, 0x00015001, uint(6), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::LaunchBackend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN14ShadowLauncher13LaunchBackendE.offsetsAndSizes,
    qt_meta_data_ZN14ShadowLauncher13LaunchBackendE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN14ShadowLauncher13LaunchBackendE_t,
        // property 'launching'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'launchProgress'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'launchStatus'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'isRunning'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'runningCount'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
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
        // method 'crashDetected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'isRunningChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'runningCountChanged'
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
        // method 'onLaunchProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'runNextCheck'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'abortCheck'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'beginTokenRefreshAttempt'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'launch'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'launch'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'launch'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'launch'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'launch'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
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
        // method 'killGameByPid'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        // method 'runningGames'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
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
        // method 'checkVersionMissingNatives'
        QtPrivate::TypeAndForceComplete<QStringList, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'checkVersionLibraries'
        QtPrivate::TypeAndForceComplete<QStringList, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::LaunchBackend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<LaunchBackend *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->launchProgressChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 1: _t->launchStateChanged(); break;
        case 2: _t->minecraftStarted(); break;
        case 3: _t->minecraftStopped(); break;
        case 4: _t->crashDetected((*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 5: _t->isRunningChanged(); break;
        case 6: _t->runningCountChanged(); break;
        case 7: _t->logMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->launchCheckProgress((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->launchCheckFailed((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 10: _t->launchCheckMissingFiles((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 11: _t->launchCheckWarning((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 12: _t->onLaunchProgress((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 13: _t->runNextCheck(); break;
        case 14: _t->abortCheck((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 15: _t->beginTokenRefreshAttempt(); break;
        case 16: _t->launch((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[6])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[7])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[8])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[9]))); break;
        case 17: _t->launch((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[6])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[7])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[8]))); break;
        case 18: _t->launch((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[6])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[7]))); break;
        case 19: _t->launch((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[6]))); break;
        case 20: _t->launch((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5]))); break;
        case 21: _t->launch((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        case 22: _t->cancelLaunch(); break;
        case 23: _t->killGameProcess(); break;
        case 24: _t->killGameByPid((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1]))); break;
        case 25: { QVariantList _r = _t->runningGames();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 26: { int _r = _t->getAutoMemory();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 27: { int _r = _t->getSystemMemory();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 28: { QVariantMap _r = _t->getMemoryStatus();
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 29: { QString _r = _t->checkJavaArchitecture((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 30: { bool _r = _t->checkVersionHasNatives((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 31: { QStringList _r = _t->checkVersionMissingNatives((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QStringList*>(_a[0]) = std::move(_r); }  break;
        case 32: { QStringList _r = _t->checkVersionLibraries((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QStringList*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (LaunchBackend::*)(int , const QString & );
            if (_q_method_type _q_method = &LaunchBackend::launchProgressChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (LaunchBackend::*)();
            if (_q_method_type _q_method = &LaunchBackend::launchStateChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (LaunchBackend::*)();
            if (_q_method_type _q_method = &LaunchBackend::minecraftStarted; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (LaunchBackend::*)();
            if (_q_method_type _q_method = &LaunchBackend::minecraftStopped; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (LaunchBackend::*)(const QVariantMap & );
            if (_q_method_type _q_method = &LaunchBackend::crashDetected; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (LaunchBackend::*)();
            if (_q_method_type _q_method = &LaunchBackend::isRunningChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (LaunchBackend::*)();
            if (_q_method_type _q_method = &LaunchBackend::runningCountChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _q_method_type = void (LaunchBackend::*)(const QString & );
            if (_q_method_type _q_method = &LaunchBackend::logMessage; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _q_method_type = void (LaunchBackend::*)(const QString & );
            if (_q_method_type _q_method = &LaunchBackend::launchCheckProgress; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 8;
                return;
            }
        }
        {
            using _q_method_type = void (LaunchBackend::*)(const QString & , const QString & );
            if (_q_method_type _q_method = &LaunchBackend::launchCheckFailed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 9;
                return;
            }
        }
        {
            using _q_method_type = void (LaunchBackend::*)(const QStringList & );
            if (_q_method_type _q_method = &LaunchBackend::launchCheckMissingFiles; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 10;
                return;
            }
        }
        {
            using _q_method_type = void (LaunchBackend::*)(const QString & );
            if (_q_method_type _q_method = &LaunchBackend::launchCheckWarning; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 11;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->isLaunching(); break;
        case 1: *reinterpret_cast< int*>(_v) = _t->launchProgress(); break;
        case 2: *reinterpret_cast< QString*>(_v) = _t->launchStatus(); break;
        case 3: *reinterpret_cast< bool*>(_v) = _t->isRunning(); break;
        case 4: *reinterpret_cast< int*>(_v) = _t->runningCount(); break;
        default: break;
        }
    }
}

const QMetaObject *ShadowLauncher::LaunchBackend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::LaunchBackend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN14ShadowLauncher13LaunchBackendE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::LaunchBackend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 33)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 33;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 33)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 33;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
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
void ShadowLauncher::LaunchBackend::crashDetected(const QVariantMap & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void ShadowLauncher::LaunchBackend::isRunningChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void ShadowLauncher::LaunchBackend::runningCountChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void ShadowLauncher::LaunchBackend::logMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 8
void ShadowLauncher::LaunchBackend::launchCheckProgress(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 8, _a);
}

// SIGNAL 9
void ShadowLauncher::LaunchBackend::launchCheckFailed(const QString & _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 9, _a);
}

// SIGNAL 10
void ShadowLauncher::LaunchBackend::launchCheckMissingFiles(const QStringList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 10, _a);
}

// SIGNAL 11
void ShadowLauncher::LaunchBackend::launchCheckWarning(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 11, _a);
}
QT_WARNING_POP
