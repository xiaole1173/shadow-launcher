/****************************************************************************
** Meta object code from reading C++ file 'yggdrasil_backend.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/backend/yggdrasil_backend.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'yggdrasil_backend.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14ShadowLauncher16YggdrasilBackendE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN14ShadowLauncher16YggdrasilBackendE = QtMocHelpers::stringData(
    "ShadowLauncher::YggdrasilBackend",
    "metaReady",
    "",
    "metaFailed",
    "error",
    "loginSuccess",
    "loginFailed",
    "showProfileSelection",
    "stateChanged",
    "profilesChanged",
    "statusMessageChanged",
    "serverAddressChanged",
    "serverNameChanged",
    "autoJoinServerChanged",
    "profileHeadsChanged",
    "onMetaReply",
    "onAuthenticateReply",
    "onRefreshReply",
    "onLogoutReply",
    "onSkinPreloaded",
    "preloadProfileSkins",
    "fetchMeta",
    "apiRoot",
    "login",
    "email",
    "password",
    "refreshToken",
    "logout",
    "selectProfile",
    "index",
    "cancelLogin",
    "fetchSkin",
    "saveSession",
    "loadSession",
    "deleteSavedSession",
    "loggedIn",
    "username",
    "uuid",
    "accessToken",
    "clientToken",
    "metaServerName",
    "registerUrl",
    "profiles",
    "QVariantList",
    "profileIndex",
    "statusMessage",
    "hadSavedSession",
    "serverAddress",
    "serverAddressValid",
    "serverName",
    "autoJoinServer",
    "skinFetcher",
    "profileHeadUrls"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN14ShadowLauncher16YggdrasilBackendE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      28,   14, // methods
      19,  224, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      12,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  182,    2, 0x06,   20 /* Public */,
       3,    1,  183,    2, 0x06,   21 /* Public */,
       5,    0,  186,    2, 0x06,   23 /* Public */,
       6,    1,  187,    2, 0x06,   24 /* Public */,
       7,    0,  190,    2, 0x06,   26 /* Public */,
       8,    0,  191,    2, 0x06,   27 /* Public */,
       9,    0,  192,    2, 0x06,   28 /* Public */,
      10,    0,  193,    2, 0x06,   29 /* Public */,
      11,    0,  194,    2, 0x06,   30 /* Public */,
      12,    0,  195,    2, 0x06,   31 /* Public */,
      13,    0,  196,    2, 0x06,   32 /* Public */,
      14,    0,  197,    2, 0x06,   33 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      15,    0,  198,    2, 0x08,   34 /* Private */,
      16,    0,  199,    2, 0x08,   35 /* Private */,
      17,    0,  200,    2, 0x08,   36 /* Private */,
      18,    0,  201,    2, 0x08,   37 /* Private */,
      19,    0,  202,    2, 0x08,   38 /* Private */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      20,    0,  203,    2, 0x00,   39 /* Private */,
      21,    1,  204,    2, 0x02,   40 /* Public */,
      23,    3,  207,    2, 0x02,   42 /* Public */,
      26,    0,  214,    2, 0x02,   46 /* Public */,
      27,    0,  215,    2, 0x02,   47 /* Public */,
      28,    1,  216,    2, 0x02,   48 /* Public */,
      30,    0,  219,    2, 0x02,   50 /* Public */,
      31,    0,  220,    2, 0x02,   51 /* Public */,
      32,    0,  221,    2, 0x02,   52 /* Public */,
      33,    0,  222,    2, 0x02,   53 /* Public */,
      34,    0,  223,    2, 0x02,   54 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    4,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    4,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // methods: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   22,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString,   22,   24,   25,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   29,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // properties: name, type, flags, notifyId, revision
      35, QMetaType::Bool, 0x00015001, uint(5), 0,
      22, QMetaType::QString, 0x00015001, uint(5), 0,
      24, QMetaType::QString, 0x00015001, uint(5), 0,
      36, QMetaType::QString, 0x00015001, uint(5), 0,
      37, QMetaType::QString, 0x00015001, uint(5), 0,
      38, QMetaType::QString, 0x00015001, uint(5), 0,
      39, QMetaType::QString, 0x00015001, uint(5), 0,
      40, QMetaType::QString, 0x00015001, uint(0), 0,
      41, QMetaType::QString, 0x00015001, uint(0), 0,
      42, 0x80000000 | 43, 0x00015009, uint(6), 0,
      44, QMetaType::Int, 0x00015001, uint(6), 0,
      45, QMetaType::QString, 0x00015001, uint(7), 0,
      46, QMetaType::Bool, 0x00015401, uint(-1), 0,
      47, QMetaType::QString, 0x00015103, uint(8), 0,
      48, QMetaType::Bool, 0x00015001, uint(8), 0,
      49, QMetaType::QString, 0x00015103, uint(9), 0,
      50, QMetaType::Bool, 0x00015103, uint(10), 0,
      51, QMetaType::QObjectStar, 0x00015401, uint(-1), 0,
      52, QMetaType::QStringList, 0x00015001, uint(11), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::YggdrasilBackend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN14ShadowLauncher16YggdrasilBackendE.offsetsAndSizes,
    qt_meta_data_ZN14ShadowLauncher16YggdrasilBackendE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN14ShadowLauncher16YggdrasilBackendE_t,
        // property 'loggedIn'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'apiRoot'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'email'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'username'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'uuid'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'accessToken'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'clientToken'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'metaServerName'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'registerUrl'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'profiles'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // property 'profileIndex'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'statusMessage'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'hadSavedSession'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'serverAddress'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'serverAddressValid'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'serverName'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'autoJoinServer'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'skinFetcher'
        QtPrivate::TypeAndForceComplete<QObject*, std::true_type>,
        // property 'profileHeadUrls'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<YggdrasilBackend, std::true_type>,
        // method 'metaReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'metaFailed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'loginSuccess'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'loginFailed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'showProfileSelection'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'stateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'profilesChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'statusMessageChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'serverAddressChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'serverNameChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'autoJoinServerChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'profileHeadsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onMetaReply'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onAuthenticateReply'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onRefreshReply'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onLogoutReply'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onSkinPreloaded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'preloadProfileSkins'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'fetchMeta'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'login'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'refreshToken'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'logout'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectProfile'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'cancelLogin'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'fetchSkin'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'saveSession'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'loadSession'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'deleteSavedSession'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::YggdrasilBackend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<YggdrasilBackend *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->metaReady(); break;
        case 1: _t->metaFailed((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->loginSuccess(); break;
        case 3: _t->loginFailed((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->showProfileSelection(); break;
        case 5: _t->stateChanged(); break;
        case 6: _t->profilesChanged(); break;
        case 7: _t->statusMessageChanged(); break;
        case 8: _t->serverAddressChanged(); break;
        case 9: _t->serverNameChanged(); break;
        case 10: _t->autoJoinServerChanged(); break;
        case 11: _t->profileHeadsChanged(); break;
        case 12: _t->onMetaReply(); break;
        case 13: _t->onAuthenticateReply(); break;
        case 14: _t->onRefreshReply(); break;
        case 15: _t->onLogoutReply(); break;
        case 16: _t->onSkinPreloaded(); break;
        case 17: _t->preloadProfileSkins(); break;
        case 18: _t->fetchMeta((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 19: _t->login((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 20: _t->refreshToken(); break;
        case 21: _t->logout(); break;
        case 22: _t->selectProfile((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 23: _t->cancelLogin(); break;
        case 24: _t->fetchSkin(); break;
        case 25: _t->saveSession(); break;
        case 26: _t->loadSession(); break;
        case 27: _t->deleteSavedSession(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (YggdrasilBackend::*)();
            if (_q_method_type _q_method = &YggdrasilBackend::metaReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (YggdrasilBackend::*)(const QString & );
            if (_q_method_type _q_method = &YggdrasilBackend::metaFailed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (YggdrasilBackend::*)();
            if (_q_method_type _q_method = &YggdrasilBackend::loginSuccess; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (YggdrasilBackend::*)(const QString & );
            if (_q_method_type _q_method = &YggdrasilBackend::loginFailed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (YggdrasilBackend::*)();
            if (_q_method_type _q_method = &YggdrasilBackend::showProfileSelection; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (YggdrasilBackend::*)();
            if (_q_method_type _q_method = &YggdrasilBackend::stateChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (YggdrasilBackend::*)();
            if (_q_method_type _q_method = &YggdrasilBackend::profilesChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _q_method_type = void (YggdrasilBackend::*)();
            if (_q_method_type _q_method = &YggdrasilBackend::statusMessageChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _q_method_type = void (YggdrasilBackend::*)();
            if (_q_method_type _q_method = &YggdrasilBackend::serverAddressChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 8;
                return;
            }
        }
        {
            using _q_method_type = void (YggdrasilBackend::*)();
            if (_q_method_type _q_method = &YggdrasilBackend::serverNameChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 9;
                return;
            }
        }
        {
            using _q_method_type = void (YggdrasilBackend::*)();
            if (_q_method_type _q_method = &YggdrasilBackend::autoJoinServerChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 10;
                return;
            }
        }
        {
            using _q_method_type = void (YggdrasilBackend::*)();
            if (_q_method_type _q_method = &YggdrasilBackend::profileHeadsChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 11;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->loggedIn(); break;
        case 1: *reinterpret_cast< QString*>(_v) = _t->apiRoot(); break;
        case 2: *reinterpret_cast< QString*>(_v) = _t->email(); break;
        case 3: *reinterpret_cast< QString*>(_v) = _t->username(); break;
        case 4: *reinterpret_cast< QString*>(_v) = _t->uuid(); break;
        case 5: *reinterpret_cast< QString*>(_v) = _t->accessToken(); break;
        case 6: *reinterpret_cast< QString*>(_v) = _t->clientToken(); break;
        case 7: *reinterpret_cast< QString*>(_v) = _t->metaServerName(); break;
        case 8: *reinterpret_cast< QString*>(_v) = _t->registerUrl(); break;
        case 9: *reinterpret_cast< QVariantList*>(_v) = _t->profiles(); break;
        case 10: *reinterpret_cast< int*>(_v) = _t->profileIndex(); break;
        case 11: *reinterpret_cast< QString*>(_v) = _t->statusMessage(); break;
        case 12: *reinterpret_cast< bool*>(_v) = _t->hadSavedSession(); break;
        case 13: *reinterpret_cast< QString*>(_v) = _t->serverAddress(); break;
        case 14: *reinterpret_cast< bool*>(_v) = _t->serverAddressValid(); break;
        case 15: *reinterpret_cast< QString*>(_v) = _t->serverName(); break;
        case 16: *reinterpret_cast< bool*>(_v) = _t->autoJoinServer(); break;
        case 17: *reinterpret_cast< QObject**>(_v) = _t->skinFetcherObj(); break;
        case 18: *reinterpret_cast< QStringList*>(_v) = _t->profileHeadUrls(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 13: _t->setServerAddress(*reinterpret_cast< QString*>(_v)); break;
        case 15: _t->setServerName(*reinterpret_cast< QString*>(_v)); break;
        case 16: _t->setAutoJoinServer(*reinterpret_cast< bool*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *ShadowLauncher::YggdrasilBackend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::YggdrasilBackend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN14ShadowLauncher16YggdrasilBackendE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::YggdrasilBackend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 28)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 28;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 28)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 28;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 19;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::YggdrasilBackend::metaReady()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ShadowLauncher::YggdrasilBackend::metaFailed(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void ShadowLauncher::YggdrasilBackend::loginSuccess()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void ShadowLauncher::YggdrasilBackend::loginFailed(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void ShadowLauncher::YggdrasilBackend::showProfileSelection()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void ShadowLauncher::YggdrasilBackend::stateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void ShadowLauncher::YggdrasilBackend::profilesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void ShadowLauncher::YggdrasilBackend::statusMessageChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void ShadowLauncher::YggdrasilBackend::serverAddressChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void ShadowLauncher::YggdrasilBackend::serverNameChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void ShadowLauncher::YggdrasilBackend::autoJoinServerChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}

// SIGNAL 11
void ShadowLauncher::YggdrasilBackend::profileHeadsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 11, nullptr);
}
QT_WARNING_POP
