/****************************************************************************
** Meta object code from reading C++ file 'launcher.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/core/launcher.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'launcher.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSShadowLauncherSCOPELauncherENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSShadowLauncherSCOPELauncherENDCLASS = QtMocHelpers::stringData(
    "ShadowLauncher::Launcher",
    "launchProgress",
    "",
    "message",
    "launchFinished",
    "success",
    "errorMsg",
    "launchStarted",
    "onProcessStarted",
    "onReadyReadStdout",
    "onReadyReadStderr",
    "onProcessFinished",
    "exitCode",
    "QProcess::ExitStatus",
    "exitStatus",
    "onProcessError",
    "QProcess::ProcessError",
    "error"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSShadowLauncherSCOPELauncherENDCLASS_t {
    uint offsetsAndSizes[36];
    char stringdata0[25];
    char stringdata1[15];
    char stringdata2[1];
    char stringdata3[8];
    char stringdata4[15];
    char stringdata5[8];
    char stringdata6[9];
    char stringdata7[14];
    char stringdata8[17];
    char stringdata9[18];
    char stringdata10[18];
    char stringdata11[18];
    char stringdata12[9];
    char stringdata13[21];
    char stringdata14[11];
    char stringdata15[15];
    char stringdata16[23];
    char stringdata17[6];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSShadowLauncherSCOPELauncherENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSShadowLauncherSCOPELauncherENDCLASS_t qt_meta_stringdata_CLASSShadowLauncherSCOPELauncherENDCLASS = {
    {
        QT_MOC_LITERAL(0, 24),  // "ShadowLauncher::Launcher"
        QT_MOC_LITERAL(25, 14),  // "launchProgress"
        QT_MOC_LITERAL(40, 0),  // ""
        QT_MOC_LITERAL(41, 7),  // "message"
        QT_MOC_LITERAL(49, 14),  // "launchFinished"
        QT_MOC_LITERAL(64, 7),  // "success"
        QT_MOC_LITERAL(72, 8),  // "errorMsg"
        QT_MOC_LITERAL(81, 13),  // "launchStarted"
        QT_MOC_LITERAL(95, 16),  // "onProcessStarted"
        QT_MOC_LITERAL(112, 17),  // "onReadyReadStdout"
        QT_MOC_LITERAL(130, 17),  // "onReadyReadStderr"
        QT_MOC_LITERAL(148, 17),  // "onProcessFinished"
        QT_MOC_LITERAL(166, 8),  // "exitCode"
        QT_MOC_LITERAL(175, 20),  // "QProcess::ExitStatus"
        QT_MOC_LITERAL(196, 10),  // "exitStatus"
        QT_MOC_LITERAL(207, 14),  // "onProcessError"
        QT_MOC_LITERAL(222, 22),  // "QProcess::ProcessError"
        QT_MOC_LITERAL(245, 5)   // "error"
    },
    "ShadowLauncher::Launcher",
    "launchProgress",
    "",
    "message",
    "launchFinished",
    "success",
    "errorMsg",
    "launchStarted",
    "onProcessStarted",
    "onReadyReadStdout",
    "onReadyReadStderr",
    "onProcessFinished",
    "exitCode",
    "QProcess::ExitStatus",
    "exitStatus",
    "onProcessError",
    "QProcess::ProcessError",
    "error"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSShadowLauncherSCOPELauncherENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   62,    2, 0x06,    1 /* Public */,
       4,    2,   65,    2, 0x06,    3 /* Public */,
       7,    0,   70,    2, 0x06,    6 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       8,    0,   71,    2, 0x08,    7 /* Private */,
       9,    0,   72,    2, 0x08,    8 /* Private */,
      10,    0,   73,    2, 0x08,    9 /* Private */,
      11,    2,   74,    2, 0x08,   10 /* Private */,
      15,    1,   79,    2, 0x08,   13 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,    5,    6,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 13,   12,   14,
    QMetaType::Void, 0x80000000 | 16,   17,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::Launcher::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSShadowLauncherSCOPELauncherENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSShadowLauncherSCOPELauncherENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSShadowLauncherSCOPELauncherENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<Launcher, std::true_type>,
        // method 'launchProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'launchFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'launchStarted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onProcessStarted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onReadyReadStdout'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onReadyReadStderr'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onProcessFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<QProcess::ExitStatus, std::false_type>,
        // method 'onProcessError'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QProcess::ProcessError, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::Launcher::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<Launcher *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->launchProgress((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->launchFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 2: _t->launchStarted(); break;
        case 3: _t->onProcessStarted(); break;
        case 4: _t->onReadyReadStdout(); break;
        case 5: _t->onReadyReadStderr(); break;
        case 6: _t->onProcessFinished((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QProcess::ExitStatus>>(_a[2]))); break;
        case 7: _t->onProcessError((*reinterpret_cast< std::add_pointer_t<QProcess::ProcessError>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (Launcher::*)(const QString & );
            if (_t _q_method = &Launcher::launchProgress; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (Launcher::*)(bool , const QString & );
            if (_t _q_method = &Launcher::launchFinished; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (Launcher::*)();
            if (_t _q_method = &Launcher::launchStarted; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
    }
}

const QMetaObject *ShadowLauncher::Launcher::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::Launcher::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSShadowLauncherSCOPELauncherENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::Launcher::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::Launcher::launchProgress(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void ShadowLauncher::Launcher::launchFinished(bool _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void ShadowLauncher::Launcher::launchStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}
QT_WARNING_POP
