/****************************************************************************
** Meta object code from reading C++ file 'account_backend.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/backend/account_backend.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'account_backend.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSShadowLauncherSCOPEAccountBackendENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSShadowLauncherSCOPEAccountBackendENDCLASS = QtMocHelpers::stringData(
    "ShadowLauncher::AccountBackend",
    "accountChanged",
    "",
    "skinReady",
    "offlineHistoryChanged",
    "logMessage",
    "msg",
    "offlineLogin",
    "username",
    "logout",
    "getSkinUrl",
    "isLoggedIn",
    "isOnline",
    "accountUuid",
    "skinPath",
    "offlineUsernames"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSShadowLauncherSCOPEAccountBackendENDCLASS_t {
    uint offsetsAndSizes[32];
    char stringdata0[31];
    char stringdata1[15];
    char stringdata2[1];
    char stringdata3[10];
    char stringdata4[22];
    char stringdata5[11];
    char stringdata6[4];
    char stringdata7[13];
    char stringdata8[9];
    char stringdata9[7];
    char stringdata10[11];
    char stringdata11[11];
    char stringdata12[9];
    char stringdata13[12];
    char stringdata14[9];
    char stringdata15[17];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSShadowLauncherSCOPEAccountBackendENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSShadowLauncherSCOPEAccountBackendENDCLASS_t qt_meta_stringdata_CLASSShadowLauncherSCOPEAccountBackendENDCLASS = {
    {
        QT_MOC_LITERAL(0, 30),  // "ShadowLauncher::AccountBackend"
        QT_MOC_LITERAL(31, 14),  // "accountChanged"
        QT_MOC_LITERAL(46, 0),  // ""
        QT_MOC_LITERAL(47, 9),  // "skinReady"
        QT_MOC_LITERAL(57, 21),  // "offlineHistoryChanged"
        QT_MOC_LITERAL(79, 10),  // "logMessage"
        QT_MOC_LITERAL(90, 3),  // "msg"
        QT_MOC_LITERAL(94, 12),  // "offlineLogin"
        QT_MOC_LITERAL(107, 8),  // "username"
        QT_MOC_LITERAL(116, 6),  // "logout"
        QT_MOC_LITERAL(123, 10),  // "getSkinUrl"
        QT_MOC_LITERAL(134, 10),  // "isLoggedIn"
        QT_MOC_LITERAL(145, 8),  // "isOnline"
        QT_MOC_LITERAL(154, 11),  // "accountUuid"
        QT_MOC_LITERAL(166, 8),  // "skinPath"
        QT_MOC_LITERAL(175, 16)   // "offlineUsernames"
    },
    "ShadowLauncher::AccountBackend",
    "accountChanged",
    "",
    "skinReady",
    "offlineHistoryChanged",
    "logMessage",
    "msg",
    "offlineLogin",
    "username",
    "logout",
    "getSkinUrl",
    "isLoggedIn",
    "isOnline",
    "accountUuid",
    "skinPath",
    "offlineUsernames"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSShadowLauncherSCOPEAccountBackendENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
       6,   76, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   62,    2, 0x06,    7 /* Public */,
       3,    0,   63,    2, 0x06,    8 /* Public */,
       4,    0,   64,    2, 0x06,    9 /* Public */,
       5,    1,   65,    2, 0x06,   10 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
       7,    1,   68,    2, 0x02,   12 /* Public */,
       9,    0,   71,    2, 0x02,   14 /* Public */,
      10,    1,   72,    2, 0x102,   15 /* Public | MethodIsConst  */,
      10,    0,   75,    2, 0x122,   17 /* Public | MethodCloned | MethodIsConst  */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    6,

 // methods: parameters
    QMetaType::Void, QMetaType::QString,    8,
    QMetaType::Void,
    QMetaType::QString, QMetaType::QString,    8,
    QMetaType::QString,

 // properties: name, type, flags
       8, QMetaType::QString, 0x00015001, uint(0), 0,
      11, QMetaType::Bool, 0x00015001, uint(0), 0,
      12, QMetaType::Bool, 0x00015001, uint(0), 0,
      13, QMetaType::QString, 0x00015001, uint(0), 0,
      14, QMetaType::QString, 0x00015001, uint(1), 0,
      15, QMetaType::QStringList, 0x00015001, uint(2), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::AccountBackend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSShadowLauncherSCOPEAccountBackendENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSShadowLauncherSCOPEAccountBackendENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSShadowLauncherSCOPEAccountBackendENDCLASS_t,
        // property 'username'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'isLoggedIn'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'isOnline'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'accountUuid'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'skinPath'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'offlineUsernames'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<AccountBackend, std::true_type>,
        // method 'accountChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'skinReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'offlineHistoryChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'logMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'offlineLogin'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'logout'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'getSkinUrl'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'getSkinUrl'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::AccountBackend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<AccountBackend *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->accountChanged(); break;
        case 1: _t->skinReady(); break;
        case 2: _t->offlineHistoryChanged(); break;
        case 3: _t->logMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->offlineLogin((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->logout(); break;
        case 6: { QString _r = _t->getSkinUrl((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 7: { QString _r = _t->getSkinUrl();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (AccountBackend::*)();
            if (_t _q_method = &AccountBackend::accountChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (AccountBackend::*)();
            if (_t _q_method = &AccountBackend::skinReady; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (AccountBackend::*)();
            if (_t _q_method = &AccountBackend::offlineHistoryChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (AccountBackend::*)(const QString & );
            if (_t _q_method = &AccountBackend::logMessage; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
    }else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<AccountBackend *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->username(); break;
        case 1: *reinterpret_cast< bool*>(_v) = _t->isLoggedIn(); break;
        case 2: *reinterpret_cast< bool*>(_v) = _t->isOnline(); break;
        case 3: *reinterpret_cast< QString*>(_v) = _t->accountUuid(); break;
        case 4: *reinterpret_cast< QString*>(_v) = _t->skinPath(); break;
        case 5: *reinterpret_cast< QStringList*>(_v) = _t->offlineUsernames(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
    } else if (_c == QMetaObject::ResetProperty) {
    } else if (_c == QMetaObject::BindableProperty) {
    }
}

const QMetaObject *ShadowLauncher::AccountBackend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::AccountBackend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSShadowLauncherSCOPEAccountBackendENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::AccountBackend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
    }else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::AccountBackend::accountChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ShadowLauncher::AccountBackend::skinReady()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ShadowLauncher::AccountBackend::offlineHistoryChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void ShadowLauncher::AccountBackend::logMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
