/****************************************************************************
** Meta object code from reading C++ file 'debug_logger.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/backend/debug_logger.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'debug_logger.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14ShadowLauncher11DebugLoggerE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN14ShadowLauncher11DebugLoggerE = QtMocHelpers::stringData(
    "ShadowLauncher::DebugLogger",
    "QML.Element",
    "auto",
    "logTextChanged",
    "",
    "visibleChanged",
    "entryCountChanged",
    "info",
    "msg",
    "ok",
    "warn",
    "error",
    "net",
    "url",
    "status",
    "size",
    "timeMs",
    "speed",
    "bytesPerSec",
    "clear",
    "toggle",
    "logText",
    "visible",
    "entryCount"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN14ShadowLauncher11DebugLoggerE[] = {

 // content:
      12,       // revision
       0,       // classname
       1,   14, // classinfo
      11,   16, // methods
       3,  111, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // classinfo: key, value
       1,    2,

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       3,    0,   82,    4, 0x06,    4 /* Public */,
       5,    0,   83,    4, 0x06,    5 /* Public */,
       6,    0,   84,    4, 0x06,    6 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       7,    1,   85,    4, 0x0a,    7 /* Public */,
       9,    1,   88,    4, 0x0a,    9 /* Public */,
      10,    1,   91,    4, 0x0a,   11 /* Public */,
      11,    1,   94,    4, 0x0a,   13 /* Public */,
      12,    4,   97,    4, 0x0a,   15 /* Public */,
      17,    1,  106,    4, 0x0a,   20 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      19,    0,  109,    4, 0x02,   22 /* Public */,
      20,    0,  110,    4, 0x02,   23 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, QMetaType::QString,    8,
    QMetaType::Void, QMetaType::QString,    8,
    QMetaType::Void, QMetaType::QString,    8,
    QMetaType::Void, QMetaType::QString,    8,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::LongLong, QMetaType::LongLong,   13,   14,   15,   16,
    QMetaType::Void, QMetaType::LongLong,   18,

 // methods: parameters
    QMetaType::Void,
    QMetaType::Void,

 // properties: name, type, flags, notifyId, revision
      21, QMetaType::QString, 0x00015001, uint(0), 0,
      22, QMetaType::Bool, 0x00015103, uint(1), 0,
      23, QMetaType::Int, 0x00015001, uint(2), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::DebugLogger::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN14ShadowLauncher11DebugLoggerE.offsetsAndSizes,
    qt_meta_data_ZN14ShadowLauncher11DebugLoggerE,
    qt_static_metacall,
    nullptr,
    qt_metaTypeArray<
        // property 'logText'
        QString,
        // property 'visible'
        bool,
        // property 'entryCount'
        int,
        // Q_OBJECT / Q_GADGET
        DebugLogger,
        // method 'logTextChanged'
        void,
        // method 'visibleChanged'
        void,
        // method 'entryCountChanged'
        void,
        // method 'info'
        void,
        const QString &,
        // method 'ok'
        void,
        const QString &,
        // method 'warn'
        void,
        const QString &,
        // method 'error'
        void,
        const QString &,
        // method 'net'
        void,
        const QString &,
        const QString &,
        qint64,
        qint64,
        // method 'speed'
        void,
        qint64,
        // method 'clear'
        void,
        // method 'toggle'
        void
    >,
    nullptr
} };

void ShadowLauncher::DebugLogger::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DebugLogger *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->logTextChanged(); break;
        case 1: _t->visibleChanged(); break;
        case 2: _t->entryCountChanged(); break;
        case 3: _t->info((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->ok((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->warn((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->error((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->net((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[4]))); break;
        case 8: _t->speed((*reinterpret_cast< std::add_pointer_t<qint64>>(_a[1]))); break;
        case 9: _t->clear(); break;
        case 10: _t->toggle(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (DebugLogger::*)();
            if (_q_method_type _q_method = &DebugLogger::logTextChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (DebugLogger::*)();
            if (_q_method_type _q_method = &DebugLogger::visibleChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (DebugLogger::*)();
            if (_q_method_type _q_method = &DebugLogger::entryCountChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->logText(); break;
        case 1: *reinterpret_cast< bool*>(_v) = _t->visible(); break;
        case 2: *reinterpret_cast< int*>(_v) = _t->entryCount(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 1: _t->setVisible(*reinterpret_cast< bool*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *ShadowLauncher::DebugLogger::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::DebugLogger::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN14ShadowLauncher11DebugLoggerE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::DebugLogger::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 11;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::DebugLogger::logTextChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ShadowLauncher::DebugLogger::visibleChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ShadowLauncher::DebugLogger::entryCountChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}
QT_WARNING_POP
