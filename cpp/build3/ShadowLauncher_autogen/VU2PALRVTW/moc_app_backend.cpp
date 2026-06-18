/****************************************************************************
** Meta object code from reading C++ file 'app_backend.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/backend/app_backend.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'app_backend.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSShadowLauncherSCOPEAppBackendENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSShadowLauncherSCOPEAppBackendENDCLASS = QtMocHelpers::stringData(
    "ShadowLauncher::AppBackend",
    "themeChanged",
    "",
    "gameDirChanged",
    "devModeChanged",
    "logMessage",
    "msg",
    "resolvePath",
    "relativePath",
    "setDevMode",
    "enabled",
    "appVersion",
    "appName",
    "dataDir",
    "gameDir",
    "theme",
    "devMode"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSShadowLauncherSCOPEAppBackendENDCLASS_t {
    uint offsetsAndSizes[34];
    char stringdata0[27];
    char stringdata1[13];
    char stringdata2[1];
    char stringdata3[15];
    char stringdata4[15];
    char stringdata5[11];
    char stringdata6[4];
    char stringdata7[12];
    char stringdata8[13];
    char stringdata9[11];
    char stringdata10[8];
    char stringdata11[11];
    char stringdata12[8];
    char stringdata13[8];
    char stringdata14[8];
    char stringdata15[6];
    char stringdata16[8];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSShadowLauncherSCOPEAppBackendENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSShadowLauncherSCOPEAppBackendENDCLASS_t qt_meta_stringdata_CLASSShadowLauncherSCOPEAppBackendENDCLASS = {
    {
        QT_MOC_LITERAL(0, 26),  // "ShadowLauncher::AppBackend"
        QT_MOC_LITERAL(27, 12),  // "themeChanged"
        QT_MOC_LITERAL(40, 0),  // ""
        QT_MOC_LITERAL(41, 14),  // "gameDirChanged"
        QT_MOC_LITERAL(56, 14),  // "devModeChanged"
        QT_MOC_LITERAL(71, 10),  // "logMessage"
        QT_MOC_LITERAL(82, 3),  // "msg"
        QT_MOC_LITERAL(86, 11),  // "resolvePath"
        QT_MOC_LITERAL(98, 12),  // "relativePath"
        QT_MOC_LITERAL(111, 10),  // "setDevMode"
        QT_MOC_LITERAL(122, 7),  // "enabled"
        QT_MOC_LITERAL(130, 10),  // "appVersion"
        QT_MOC_LITERAL(141, 7),  // "appName"
        QT_MOC_LITERAL(149, 7),  // "dataDir"
        QT_MOC_LITERAL(157, 7),  // "gameDir"
        QT_MOC_LITERAL(165, 5),  // "theme"
        QT_MOC_LITERAL(171, 7)   // "devMode"
    },
    "ShadowLauncher::AppBackend",
    "themeChanged",
    "",
    "gameDirChanged",
    "devModeChanged",
    "logMessage",
    "msg",
    "resolvePath",
    "relativePath",
    "setDevMode",
    "enabled",
    "appVersion",
    "appName",
    "dataDir",
    "gameDir",
    "theme",
    "devMode"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSShadowLauncherSCOPEAppBackendENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       6,   62, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   50,    2, 0x06,    7 /* Public */,
       3,    0,   51,    2, 0x06,    8 /* Public */,
       4,    0,   52,    2, 0x06,    9 /* Public */,
       5,    1,   53,    2, 0x06,   10 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
       7,    1,   56,    2, 0x102,   12 /* Public | MethodIsConst  */,
       9,    1,   59,    2, 0x02,   14 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    6,

 // methods: parameters
    QMetaType::QString, QMetaType::QString,    8,
    QMetaType::Void, QMetaType::Bool,   10,

 // properties: name, type, flags
      11, QMetaType::QString, 0x00015401, uint(-1), 0,
      12, QMetaType::QString, 0x00015401, uint(-1), 0,
      13, QMetaType::QString, 0x00015401, uint(-1), 0,
      14, QMetaType::QString, 0x00015001, uint(1), 0,
      15, QMetaType::QString, 0x00015103, uint(0), 0,
      16, QMetaType::Bool, 0x00015001, uint(2), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::AppBackend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSShadowLauncherSCOPEAppBackendENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSShadowLauncherSCOPEAppBackendENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSShadowLauncherSCOPEAppBackendENDCLASS_t,
        // property 'appVersion'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'appName'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'dataDir'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'gameDir'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'theme'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'devMode'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<AppBackend, std::true_type>,
        // method 'themeChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'gameDirChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'devModeChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'logMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'resolvePath'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setDevMode'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::AppBackend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<AppBackend *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->themeChanged(); break;
        case 1: _t->gameDirChanged(); break;
        case 2: _t->devModeChanged(); break;
        case 3: _t->logMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: { QString _r = _t->resolvePath((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 5: _t->setDevMode((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (AppBackend::*)();
            if (_t _q_method = &AppBackend::themeChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (AppBackend::*)();
            if (_t _q_method = &AppBackend::gameDirChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (AppBackend::*)();
            if (_t _q_method = &AppBackend::devModeChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (AppBackend::*)(const QString & );
            if (_t _q_method = &AppBackend::logMessage; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
    }else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<AppBackend *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->appVersion(); break;
        case 1: *reinterpret_cast< QString*>(_v) = _t->appName(); break;
        case 2: *reinterpret_cast< QString*>(_v) = _t->dataDir(); break;
        case 3: *reinterpret_cast< QString*>(_v) = _t->gameDir(); break;
        case 4: *reinterpret_cast< QString*>(_v) = _t->theme(); break;
        case 5: *reinterpret_cast< bool*>(_v) = _t->devMode(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
        auto *_t = static_cast<AppBackend *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 4: _t->setTheme(*reinterpret_cast< QString*>(_v)); break;
        default: break;
        }
    } else if (_c == QMetaObject::ResetProperty) {
    } else if (_c == QMetaObject::BindableProperty) {
    }
}

const QMetaObject *ShadowLauncher::AppBackend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::AppBackend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSShadowLauncherSCOPEAppBackendENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::AppBackend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::AppBackend::themeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ShadowLauncher::AppBackend::gameDirChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ShadowLauncher::AppBackend::devModeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void ShadowLauncher::AppBackend::logMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
