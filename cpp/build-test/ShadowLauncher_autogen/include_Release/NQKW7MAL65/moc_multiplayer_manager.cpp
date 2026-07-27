/****************************************************************************
** Meta object code from reading C++ file 'multiplayer_manager.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/multiplayer/multiplayer_manager.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'multiplayer_manager.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14ShadowLauncher18MultiplayerManagerE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN14ShadowLauncher18MultiplayerManagerE = QtMocHelpers::stringData(
    "ShadowLauncher::MultiplayerManager",
    "roomCodeChanged",
    "",
    "stateChanged",
    "stateTextChanged",
    "playersChanged",
    "roleChanged",
    "minecraftPortReady",
    "port",
    "errorOccurred",
    "msg",
    "playerNameChanged",
    "onNetworkReady",
    "virtualIp",
    "onEasyTierError",
    "onSocketConnected",
    "onSocketDisconnected",
    "onSocketError",
    "QAbstractSocket::SocketError",
    "err",
    "onSocketReadyRead",
    "onNewConnection",
    "onGuestSocketReadyRead",
    "onGuestDisconnected",
    "sendHeartbeat",
    "sendPing",
    "doDiscoverCenter",
    "onPeerListReady",
    "onDiscoverTimeout",
    "onIdleTimeout",
    "playerHeadPath",
    "name",
    "dataDir",
    "createRoom",
    "restoreHostSession",
    "networkName",
    "networkKey",
    "roomCode",
    "mcPort",
    "hostname",
    "joinRoom",
    "code",
    "restoreGuestSession",
    "leaveRoom",
    "copyRoomCode",
    "prepareServerProperties",
    "gameDir",
    "versionId",
    "setPlayerName",
    "state",
    "State",
    "stateText",
    "maxPlayers",
    "players",
    "QVariantList",
    "role",
    "Role",
    "playerName",
    "Idle",
    "CreatingRoom",
    "JoiningNetwork",
    "Discovering",
    "Connecting",
    "Connected",
    "WaitingForGuests",
    "Error",
    "None",
    "Host",
    "Guest"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN14ShadowLauncher18MultiplayerManagerE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      32,   14, // methods
       7,  276, // properties
       2,  311, // enums/sets
       0,    0, // constructors
       0,       // flags
       8,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  206,    2, 0x06,   10 /* Public */,
       3,    0,  207,    2, 0x06,   11 /* Public */,
       4,    0,  208,    2, 0x06,   12 /* Public */,
       5,    0,  209,    2, 0x06,   13 /* Public */,
       6,    0,  210,    2, 0x06,   14 /* Public */,
       7,    1,  211,    2, 0x06,   15 /* Public */,
       9,    1,  214,    2, 0x06,   17 /* Public */,
      11,    0,  217,    2, 0x06,   19 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      12,    1,  218,    2, 0x08,   20 /* Private */,
      14,    1,  221,    2, 0x08,   22 /* Private */,
      15,    0,  224,    2, 0x08,   24 /* Private */,
      16,    0,  225,    2, 0x08,   25 /* Private */,
      17,    1,  226,    2, 0x08,   26 /* Private */,
      20,    0,  229,    2, 0x08,   28 /* Private */,
      21,    0,  230,    2, 0x08,   29 /* Private */,
      22,    0,  231,    2, 0x08,   30 /* Private */,
      23,    0,  232,    2, 0x08,   31 /* Private */,
      24,    0,  233,    2, 0x08,   32 /* Private */,
      25,    0,  234,    2, 0x08,   33 /* Private */,
      26,    0,  235,    2, 0x08,   34 /* Private */,
      27,    0,  236,    2, 0x08,   35 /* Private */,
      28,    0,  237,    2, 0x08,   36 /* Private */,
      29,    0,  238,    2, 0x08,   37 /* Private */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      30,    2,  239,    2, 0x02,   38 /* Public */,
      33,    0,  244,    2, 0x02,   41 /* Public */,
      34,    5,  245,    2, 0x02,   42 /* Public */,
      40,    1,  256,    2, 0x02,   48 /* Public */,
      42,    3,  259,    2, 0x02,   50 /* Public */,
      43,    0,  266,    2, 0x02,   54 /* Public */,
      44,    0,  267,    2, 0x02,   55 /* Public */,
      45,    2,  268,    2, 0x02,   56 /* Public */,
      48,    1,  273,    2, 0x02,   59 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    8,
    QMetaType::Void, QMetaType::QString,   10,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, QMetaType::QString,   13,
    QMetaType::Void, QMetaType::QString,   10,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 18,   19,
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

 // methods: parameters
    QMetaType::QString, QMetaType::QString, QMetaType::QString,   31,   32,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::UShort, QMetaType::QString,   35,   36,   37,   38,   39,
    QMetaType::Void, QMetaType::QString,   41,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString,   35,   36,   37,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   46,   47,
    QMetaType::Void, QMetaType::QString,   31,

 // properties: name, type, flags, notifyId, revision
      37, QMetaType::QString, 0x00015001, uint(0), 0,
      49, 0x80000000 | 50, 0x00015009, uint(1), 0,
      51, QMetaType::QString, 0x00015001, uint(2), 0,
      52, QMetaType::Int, 0x00015401, uint(-1), 0,
      53, 0x80000000 | 54, 0x00015009, uint(3), 0,
      55, 0x80000000 | 56, 0x00015009, uint(4), 0,
      57, QMetaType::QString, 0x00015001, uint(7), 0,

 // enums: name, alias, flags, count, data
      50,   50, 0x0,    8,  321,
      56,   56, 0x0,    3,  337,

 // enum data: key, value
      58, uint(ShadowLauncher::MultiplayerManager::Idle),
      59, uint(ShadowLauncher::MultiplayerManager::CreatingRoom),
      60, uint(ShadowLauncher::MultiplayerManager::JoiningNetwork),
      61, uint(ShadowLauncher::MultiplayerManager::Discovering),
      62, uint(ShadowLauncher::MultiplayerManager::Connecting),
      63, uint(ShadowLauncher::MultiplayerManager::Connected),
      64, uint(ShadowLauncher::MultiplayerManager::WaitingForGuests),
      65, uint(ShadowLauncher::MultiplayerManager::Error),
      66, uint(ShadowLauncher::MultiplayerManager::None),
      67, uint(ShadowLauncher::MultiplayerManager::Host),
      68, uint(ShadowLauncher::MultiplayerManager::Guest),

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::MultiplayerManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN14ShadowLauncher18MultiplayerManagerE.offsetsAndSizes,
    qt_meta_data_ZN14ShadowLauncher18MultiplayerManagerE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN14ShadowLauncher18MultiplayerManagerE_t,
        // property 'roomCode'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'state'
        QtPrivate::TypeAndForceComplete<State, std::true_type>,
        // property 'stateText'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'maxPlayers'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'players'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // property 'role'
        QtPrivate::TypeAndForceComplete<Role, std::true_type>,
        // property 'playerName'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // enum 'State'
        QtPrivate::TypeAndForceComplete<MultiplayerManager::State, std::true_type>,
        // enum 'Role'
        QtPrivate::TypeAndForceComplete<MultiplayerManager::Role, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<MultiplayerManager, std::true_type>,
        // method 'roomCodeChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'stateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'stateTextChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'playersChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'roleChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'minecraftPortReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'errorOccurred'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'playerNameChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onNetworkReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onEasyTierError'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onSocketConnected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onSocketDisconnected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onSocketError'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QAbstractSocket::SocketError, std::false_type>,
        // method 'onSocketReadyRead'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onNewConnection'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onGuestSocketReadyRead'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onGuestDisconnected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'sendHeartbeat'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'sendPing'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'doDiscoverCenter'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onPeerListReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDiscoverTimeout'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onIdleTimeout'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'playerHeadPath'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'createRoom'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'restoreHostSession'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint16, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'joinRoom'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'restoreGuestSession'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'leaveRoom'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'copyRoomCode'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'prepareServerProperties'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setPlayerName'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::MultiplayerManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MultiplayerManager *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->roomCodeChanged(); break;
        case 1: _t->stateChanged(); break;
        case 2: _t->stateTextChanged(); break;
        case 3: _t->playersChanged(); break;
        case 4: _t->roleChanged(); break;
        case 5: _t->minecraftPortReady((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->errorOccurred((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->playerNameChanged(); break;
        case 8: _t->onNetworkReady((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->onEasyTierError((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 10: _t->onSocketConnected(); break;
        case 11: _t->onSocketDisconnected(); break;
        case 12: _t->onSocketError((*reinterpret_cast< std::add_pointer_t<QAbstractSocket::SocketError>>(_a[1]))); break;
        case 13: _t->onSocketReadyRead(); break;
        case 14: _t->onNewConnection(); break;
        case 15: _t->onGuestSocketReadyRead(); break;
        case 16: _t->onGuestDisconnected(); break;
        case 17: _t->sendHeartbeat(); break;
        case 18: _t->sendPing(); break;
        case 19: _t->doDiscoverCenter(); break;
        case 20: _t->onPeerListReady(); break;
        case 21: _t->onDiscoverTimeout(); break;
        case 22: _t->onIdleTimeout(); break;
        case 23: { QString _r = _t->playerHeadPath((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 24: _t->createRoom(); break;
        case 25: _t->restoreHostSession((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<quint16>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5]))); break;
        case 26: _t->joinRoom((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 27: _t->restoreGuestSession((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 28: _t->leaveRoom(); break;
        case 29: _t->copyRoomCode(); break;
        case 30: _t->prepareServerProperties((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 31: _t->setPlayerName((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 12:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAbstractSocket::SocketError >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (MultiplayerManager::*)();
            if (_q_method_type _q_method = &MultiplayerManager::roomCodeChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (MultiplayerManager::*)();
            if (_q_method_type _q_method = &MultiplayerManager::stateChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (MultiplayerManager::*)();
            if (_q_method_type _q_method = &MultiplayerManager::stateTextChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (MultiplayerManager::*)();
            if (_q_method_type _q_method = &MultiplayerManager::playersChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (MultiplayerManager::*)();
            if (_q_method_type _q_method = &MultiplayerManager::roleChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (MultiplayerManager::*)(int );
            if (_q_method_type _q_method = &MultiplayerManager::minecraftPortReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (MultiplayerManager::*)(const QString & );
            if (_q_method_type _q_method = &MultiplayerManager::errorOccurred; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _q_method_type = void (MultiplayerManager::*)();
            if (_q_method_type _q_method = &MultiplayerManager::playerNameChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->roomCode(); break;
        case 1: *reinterpret_cast< State*>(_v) = _t->state(); break;
        case 2: *reinterpret_cast< QString*>(_v) = _t->stateText(); break;
        case 3: *reinterpret_cast< int*>(_v) = _t->maxPlayers(); break;
        case 4: *reinterpret_cast< QVariantList*>(_v) = _t->players(); break;
        case 5: *reinterpret_cast< Role*>(_v) = _t->role(); break;
        case 6: *reinterpret_cast< QString*>(_v) = _t->playerName(); break;
        default: break;
        }
    }
}

const QMetaObject *ShadowLauncher::MultiplayerManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::MultiplayerManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN14ShadowLauncher18MultiplayerManagerE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::MultiplayerManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 32)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 32;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 32)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 32;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::MultiplayerManager::roomCodeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ShadowLauncher::MultiplayerManager::stateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ShadowLauncher::MultiplayerManager::stateTextChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void ShadowLauncher::MultiplayerManager::playersChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void ShadowLauncher::MultiplayerManager::roleChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void ShadowLauncher::MultiplayerManager::minecraftPortReady(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void ShadowLauncher::MultiplayerManager::errorOccurred(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void ShadowLauncher::MultiplayerManager::playerNameChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}
QT_WARNING_POP
