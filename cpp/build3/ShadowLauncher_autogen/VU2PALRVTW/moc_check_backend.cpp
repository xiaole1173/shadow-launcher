/****************************************************************************
** Meta object code from reading C++ file 'check_backend.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/backend/check_backend.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'check_backend.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSShadowLauncherSCOPECheckBackendENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSShadowLauncherSCOPECheckBackendENDCLASS = QtMocHelpers::stringData(
    "ShadowLauncher::CheckBackend",
    "logMessage",
    "",
    "msg",
    "checkJava",
    "javaPath",
    "checkVersionJar",
    "versionId",
    "gameDir",
    "checkVersionJson",
    "checkMemory",
    "maxMemoryMB",
    "checkAll"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSShadowLauncherSCOPECheckBackendENDCLASS_t {
    uint offsetsAndSizes[26];
    char stringdata0[29];
    char stringdata1[11];
    char stringdata2[1];
    char stringdata3[4];
    char stringdata4[10];
    char stringdata5[9];
    char stringdata6[16];
    char stringdata7[10];
    char stringdata8[8];
    char stringdata9[17];
    char stringdata10[12];
    char stringdata11[12];
    char stringdata12[9];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSShadowLauncherSCOPECheckBackendENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSShadowLauncherSCOPECheckBackendENDCLASS_t qt_meta_stringdata_CLASSShadowLauncherSCOPECheckBackendENDCLASS = {
    {
        QT_MOC_LITERAL(0, 28),  // "ShadowLauncher::CheckBackend"
        QT_MOC_LITERAL(29, 10),  // "logMessage"
        QT_MOC_LITERAL(40, 0),  // ""
        QT_MOC_LITERAL(41, 3),  // "msg"
        QT_MOC_LITERAL(45, 9),  // "checkJava"
        QT_MOC_LITERAL(55, 8),  // "javaPath"
        QT_MOC_LITERAL(64, 15),  // "checkVersionJar"
        QT_MOC_LITERAL(80, 9),  // "versionId"
        QT_MOC_LITERAL(90, 7),  // "gameDir"
        QT_MOC_LITERAL(98, 16),  // "checkVersionJson"
        QT_MOC_LITERAL(115, 11),  // "checkMemory"
        QT_MOC_LITERAL(127, 11),  // "maxMemoryMB"
        QT_MOC_LITERAL(139, 8)   // "checkAll"
    },
    "ShadowLauncher::CheckBackend",
    "logMessage",
    "",
    "msg",
    "checkJava",
    "javaPath",
    "checkVersionJar",
    "versionId",
    "gameDir",
    "checkVersionJson",
    "checkMemory",
    "maxMemoryMB",
    "checkAll"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSShadowLauncherSCOPECheckBackendENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   50,    2, 0x06,    1 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
       4,    1,   53,    2, 0x02,    3 /* Public */,
       6,    2,   56,    2, 0x02,    5 /* Public */,
       9,    2,   61,    2, 0x02,    8 /* Public */,
      10,    1,   66,    2, 0x02,   11 /* Public */,
      12,    4,   69,    2, 0x02,   13 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    3,

 // methods: parameters
    QMetaType::QVariantMap, QMetaType::QString,    5,
    QMetaType::QVariantMap, QMetaType::QString, QMetaType::QString,    7,    8,
    QMetaType::QVariantMap, QMetaType::QString, QMetaType::QString,    7,    8,
    QMetaType::QVariantMap, QMetaType::Int,   11,
    QMetaType::QVariantMap, QMetaType::QString, QMetaType::QString, QMetaType::Int, QMetaType::QString,    7,    5,   11,    8,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::CheckBackend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSShadowLauncherSCOPECheckBackendENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSShadowLauncherSCOPECheckBackendENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSShadowLauncherSCOPECheckBackendENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<CheckBackend, std::true_type>,
        // method 'logMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'checkJava'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'checkVersionJar'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'checkVersionJson'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'checkMemory'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'checkAll'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::CheckBackend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<CheckBackend *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->logMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: { QVariantMap _r = _t->checkJava((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 2: { QVariantMap _r = _t->checkVersionJar((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 3: { QVariantMap _r = _t->checkVersionJson((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 4: { QVariantMap _r = _t->checkMemory((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 5: { QVariantMap _r = _t->checkAll((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4])));
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (CheckBackend::*)(const QString & );
            if (_t _q_method = &CheckBackend::logMessage; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject *ShadowLauncher::CheckBackend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::CheckBackend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSShadowLauncherSCOPECheckBackendENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::CheckBackend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::CheckBackend::logMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
