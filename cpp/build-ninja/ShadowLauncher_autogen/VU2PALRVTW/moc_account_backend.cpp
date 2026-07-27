/****************************************************************************
** Meta object code from reading C++ file 'account_backend.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/backend/account_backend.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'account_backend.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14ShadowLauncher8CapeInfoE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN14ShadowLauncher8CapeInfoE = QtMocHelpers::stringData(
    "ShadowLauncher::CapeInfo",
    "alias",
    "capeId",
    "localPath",
    "active"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN14ShadowLauncher8CapeInfoE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       4,   14, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       4,       // flags
       0,       // signalCount

 // properties: name, type, flags, notifyId, revision
       1, QMetaType::QString, 0x00015401, uint(-1), 0,
       2, QMetaType::QString, 0x00015401, uint(-1), 0,
       3, QMetaType::QString, 0x00015401, uint(-1), 0,
       4, QMetaType::Bool, 0x00015401, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::CapeInfo::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_ZN14ShadowLauncher8CapeInfoE.offsetsAndSizes,
    qt_meta_data_ZN14ShadowLauncher8CapeInfoE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN14ShadowLauncher8CapeInfoE_t,
        // property 'alias'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'capeId'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'localPath'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'active'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<CapeInfo, std::true_type>
    >,
    nullptr
} };

void ShadowLauncher::CapeInfo::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = reinterpret_cast<CapeInfo *>(_o);
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->alias; break;
        case 1: *reinterpret_cast< QString*>(_v) = _t->capeId; break;
        case 2: *reinterpret_cast< QString*>(_v) = _t->localPath; break;
        case 3: *reinterpret_cast< bool*>(_v) = _t->active; break;
        default: break;
        }
    }
}
namespace {
struct qt_meta_tag_ZN14ShadowLauncher14AccountBackendE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN14ShadowLauncher14AccountBackendE = QtMocHelpers::stringData(
    "ShadowLauncher::AccountBackend",
    "embeddedLoginChanged",
    "",
    "accountChanged",
    "skinReady",
    "offlineSkinReady",
    "capeReady",
    "capesReady",
    "skinVariantChanged",
    "capeChangeFinished",
    "success",
    "errorMsg",
    "skinUploadFinished",
    "offlineHistoryChanged",
    "logMessage",
    "msg",
    "microsoftLoginProgress",
    "step",
    "detail",
    "microsoftLoginSuccess",
    "username",
    "uuid",
    "microsoftLoginFailed",
    "error",
    "microsoftLoginBusyChanged",
    "tokenRefreshed",
    "ok",
    "tokenRefreshFailed",
    "tokenExpired",
    "reason",
    "offlineLogin",
    "microsoftLogin",
    "cancelMicrosoftLogin",
    "selectCape",
    "alias",
    "uploadSkin",
    "filePath",
    "variant",
    "refreshSkin",
    "updateOfflineSkin",
    "removeOfflineUsername",
    "logout",
    "isMicrosoftLoginBusy",
    "offlineUsername",
    "isLoggedIn",
    "isOnline",
    "accountUuid",
    "offlineUuid",
    "skinPath",
    "offlineSkinPath",
    "capePath",
    "capes",
    "QVariantList",
    "skinVariant",
    "offlineUsernames",
    "microsoftLoginBusy",
    "embeddedLoginEnabled",
    "msStatus"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN14ShadowLauncher14AccountBackendE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      27,   14, // methods
      15,  241, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      17,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  176,    2, 0x06,   16 /* Public */,
       3,    0,  177,    2, 0x06,   17 /* Public */,
       4,    0,  178,    2, 0x06,   18 /* Public */,
       5,    0,  179,    2, 0x06,   19 /* Public */,
       6,    0,  180,    2, 0x06,   20 /* Public */,
       7,    0,  181,    2, 0x06,   21 /* Public */,
       8,    0,  182,    2, 0x06,   22 /* Public */,
       9,    2,  183,    2, 0x06,   23 /* Public */,
      12,    2,  188,    2, 0x06,   26 /* Public */,
      13,    0,  193,    2, 0x06,   29 /* Public */,
      14,    1,  194,    2, 0x06,   30 /* Public */,
      16,    2,  197,    2, 0x06,   32 /* Public */,
      19,    2,  202,    2, 0x06,   35 /* Public */,
      22,    1,  207,    2, 0x06,   38 /* Public */,
      24,    0,  210,    2, 0x06,   40 /* Public */,
      25,    1,  211,    2, 0x06,   41 /* Public */,
      27,    2,  214,    2, 0x06,   43 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      30,    1,  219,    2, 0x02,   46 /* Public */,
      31,    0,  222,    2, 0x02,   48 /* Public */,
      32,    0,  223,    2, 0x02,   49 /* Public */,
      33,    1,  224,    2, 0x02,   50 /* Public */,
      35,    2,  227,    2, 0x02,   52 /* Public */,
      38,    0,  232,    2, 0x02,   55 /* Public */,
      39,    1,  233,    2, 0x02,   56 /* Public */,
      40,    1,  236,    2, 0x02,   58 /* Public */,
      41,    0,  239,    2, 0x02,   60 /* Public */,
      42,    0,  240,    2, 0x102,   61 /* Public | MethodIsConst  */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,   10,   11,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,   10,   11,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   15,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   17,   18,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   20,   21,
    QMetaType::Void, QMetaType::QString,   23,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   26,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,   28,   29,

 // methods: parameters
    QMetaType::Void, QMetaType::QString,   20,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   34,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   36,   37,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   20,
    QMetaType::Void, QMetaType::QString,   20,
    QMetaType::Void,
    QMetaType::Bool,

 // properties: name, type, flags, notifyId, revision
      20, QMetaType::QString, 0x00015001, uint(1), 0,
      43, QMetaType::QString, 0x00015001, uint(1), 0,
      44, QMetaType::Bool, 0x00015001, uint(1), 0,
      45, QMetaType::Bool, 0x00015001, uint(1), 0,
      46, QMetaType::QString, 0x00015001, uint(1), 0,
      47, QMetaType::QString, 0x00015001, uint(1), 0,
      48, QMetaType::QString, 0x00015001, uint(2), 0,
      49, QMetaType::QString, 0x00015001, uint(3), 0,
      50, QMetaType::QString, 0x00015001, uint(4), 0,
      51, 0x80000000 | 52, 0x00015009, uint(5), 0,
      53, QMetaType::QString, 0x00015001, uint(6), 0,
      54, QMetaType::QStringList, 0x00015001, uint(9), 0,
      55, QMetaType::Bool, 0x00015001, uint(14), 0,
      56, QMetaType::Bool, 0x00015103, uint(0), 0,
      57, QMetaType::QString, 0x00015001, uint(11), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::AccountBackend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN14ShadowLauncher14AccountBackendE.offsetsAndSizes,
    qt_meta_data_ZN14ShadowLauncher14AccountBackendE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN14ShadowLauncher14AccountBackendE_t,
        // property 'username'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'offlineUsername'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'isLoggedIn'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
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
        // property 'capePath'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'capes'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // property 'skinVariant'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'offlineUsernames'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'microsoftLoginBusy'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'embeddedLoginEnabled'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'msStatus'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<AccountBackend, std::true_type>,
        // method 'embeddedLoginChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'accountChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'skinReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'offlineSkinReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'capeReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'capesReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'skinVariantChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'capeChangeFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'skinUploadFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'offlineHistoryChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'logMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
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
        // method 'microsoftLoginBusyChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'tokenRefreshed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'tokenRefreshFailed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'offlineLogin'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'microsoftLogin'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'cancelMicrosoftLogin'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectCape'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'uploadSkin'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'refreshSkin'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateOfflineSkin'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'removeOfflineUsername'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'logout'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'isMicrosoftLoginBusy'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::AccountBackend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AccountBackend *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->embeddedLoginChanged(); break;
        case 1: _t->accountChanged(); break;
        case 2: _t->skinReady(); break;
        case 3: _t->offlineSkinReady(); break;
        case 4: _t->capeReady(); break;
        case 5: _t->capesReady(); break;
        case 6: _t->skinVariantChanged(); break;
        case 7: _t->capeChangeFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 8: _t->skinUploadFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 9: _t->offlineHistoryChanged(); break;
        case 10: _t->logMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 11: _t->microsoftLoginProgress((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 12: _t->microsoftLoginSuccess((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 13: _t->microsoftLoginFailed((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 14: _t->microsoftLoginBusyChanged(); break;
        case 15: _t->tokenRefreshed((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 16: _t->tokenRefreshFailed((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 17: _t->offlineLogin((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 18: _t->microsoftLogin(); break;
        case 19: _t->cancelMicrosoftLogin(); break;
        case 20: _t->selectCape((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 21: _t->uploadSkin((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 22: _t->refreshSkin(); break;
        case 23: _t->updateOfflineSkin((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 24: _t->removeOfflineUsername((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 25: _t->logout(); break;
        case 26: { bool _r = _t->isMicrosoftLoginBusy();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (AccountBackend::*)();
            if (_q_method_type _q_method = &AccountBackend::embeddedLoginChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (AccountBackend::*)();
            if (_q_method_type _q_method = &AccountBackend::accountChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (AccountBackend::*)();
            if (_q_method_type _q_method = &AccountBackend::skinReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (AccountBackend::*)();
            if (_q_method_type _q_method = &AccountBackend::offlineSkinReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (AccountBackend::*)();
            if (_q_method_type _q_method = &AccountBackend::capeReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (AccountBackend::*)();
            if (_q_method_type _q_method = &AccountBackend::capesReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (AccountBackend::*)();
            if (_q_method_type _q_method = &AccountBackend::skinVariantChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _q_method_type = void (AccountBackend::*)(bool , const QString & );
            if (_q_method_type _q_method = &AccountBackend::capeChangeFinished; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _q_method_type = void (AccountBackend::*)(bool , const QString & );
            if (_q_method_type _q_method = &AccountBackend::skinUploadFinished; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 8;
                return;
            }
        }
        {
            using _q_method_type = void (AccountBackend::*)();
            if (_q_method_type _q_method = &AccountBackend::offlineHistoryChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 9;
                return;
            }
        }
        {
            using _q_method_type = void (AccountBackend::*)(const QString & );
            if (_q_method_type _q_method = &AccountBackend::logMessage; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 10;
                return;
            }
        }
        {
            using _q_method_type = void (AccountBackend::*)(const QString & , const QString & );
            if (_q_method_type _q_method = &AccountBackend::microsoftLoginProgress; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 11;
                return;
            }
        }
        {
            using _q_method_type = void (AccountBackend::*)(const QString & , const QString & );
            if (_q_method_type _q_method = &AccountBackend::microsoftLoginSuccess; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 12;
                return;
            }
        }
        {
            using _q_method_type = void (AccountBackend::*)(const QString & );
            if (_q_method_type _q_method = &AccountBackend::microsoftLoginFailed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 13;
                return;
            }
        }
        {
            using _q_method_type = void (AccountBackend::*)();
            if (_q_method_type _q_method = &AccountBackend::microsoftLoginBusyChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 14;
                return;
            }
        }
        {
            using _q_method_type = void (AccountBackend::*)(bool );
            if (_q_method_type _q_method = &AccountBackend::tokenRefreshed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 15;
                return;
            }
        }
        {
            using _q_method_type = void (AccountBackend::*)(bool , const QString & );
            if (_q_method_type _q_method = &AccountBackend::tokenRefreshFailed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 16;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->username(); break;
        case 1: *reinterpret_cast< QString*>(_v) = _t->offlineUsername(); break;
        case 2: *reinterpret_cast< bool*>(_v) = _t->isLoggedIn(); break;
        case 3: *reinterpret_cast< bool*>(_v) = _t->isOnline(); break;
        case 4: *reinterpret_cast< QString*>(_v) = _t->accountUuid(); break;
        case 5: *reinterpret_cast< QString*>(_v) = _t->offlineUuid(); break;
        case 6: *reinterpret_cast< QString*>(_v) = _t->skinPath(); break;
        case 7: *reinterpret_cast< QString*>(_v) = _t->offlineSkinPath(); break;
        case 8: *reinterpret_cast< QString*>(_v) = _t->capePath(); break;
        case 9: *reinterpret_cast< QVariantList*>(_v) = _t->capes(); break;
        case 10: *reinterpret_cast< QString*>(_v) = _t->skinVariant(); break;
        case 11: *reinterpret_cast< QStringList*>(_v) = _t->offlineUsernames(); break;
        case 12: *reinterpret_cast< bool*>(_v) = _t->isMicrosoftLoginBusy(); break;
        case 13: *reinterpret_cast< bool*>(_v) = _t->embeddedLoginEnabled(); break;
        case 14: *reinterpret_cast< QString*>(_v) = _t->msStatus(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 13: _t->setEmbeddedLoginEnabled(*reinterpret_cast< bool*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *ShadowLauncher::AccountBackend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::AccountBackend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN14ShadowLauncher14AccountBackendE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::AccountBackend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 27)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 27;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 27)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 27;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::AccountBackend::embeddedLoginChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ShadowLauncher::AccountBackend::accountChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ShadowLauncher::AccountBackend::skinReady()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void ShadowLauncher::AccountBackend::offlineSkinReady()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void ShadowLauncher::AccountBackend::capeReady()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void ShadowLauncher::AccountBackend::capesReady()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void ShadowLauncher::AccountBackend::skinVariantChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void ShadowLauncher::AccountBackend::capeChangeFinished(bool _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 8
void ShadowLauncher::AccountBackend::skinUploadFinished(bool _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 8, _a);
}

// SIGNAL 9
void ShadowLauncher::AccountBackend::offlineHistoryChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void ShadowLauncher::AccountBackend::logMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 10, _a);
}

// SIGNAL 11
void ShadowLauncher::AccountBackend::microsoftLoginProgress(const QString & _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 11, _a);
}

// SIGNAL 12
void ShadowLauncher::AccountBackend::microsoftLoginSuccess(const QString & _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 12, _a);
}

// SIGNAL 13
void ShadowLauncher::AccountBackend::microsoftLoginFailed(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 13, _a);
}

// SIGNAL 14
void ShadowLauncher::AccountBackend::microsoftLoginBusyChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 14, nullptr);
}

// SIGNAL 15
void ShadowLauncher::AccountBackend::tokenRefreshed(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 15, _a);
}

// SIGNAL 16
void ShadowLauncher::AccountBackend::tokenRefreshFailed(bool _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 16, _a);
}
QT_WARNING_POP
