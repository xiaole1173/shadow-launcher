/****************************************************************************
** Meta object code from reading C++ file 'version_manager.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/core/version_manager.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'version_manager.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSShadowLauncherSCOPEVersionManagerENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSShadowLauncherSCOPEVersionManagerENDCLASS = QtMocHelpers::stringData(
    "ShadowLauncher::VersionManager",
    "versionsReady",
    "",
    "QList<McVersion>",
    "versions",
    "fetchError",
    "error"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSShadowLauncherSCOPEVersionManagerENDCLASS_t {
    uint offsetsAndSizes[14];
    char stringdata0[31];
    char stringdata1[14];
    char stringdata2[1];
    char stringdata3[17];
    char stringdata4[9];
    char stringdata5[11];
    char stringdata6[6];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSShadowLauncherSCOPEVersionManagerENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSShadowLauncherSCOPEVersionManagerENDCLASS_t qt_meta_stringdata_CLASSShadowLauncherSCOPEVersionManagerENDCLASS = {
    {
        QT_MOC_LITERAL(0, 30),  // "ShadowLauncher::VersionManager"
        QT_MOC_LITERAL(31, 13),  // "versionsReady"
        QT_MOC_LITERAL(45, 0),  // ""
        QT_MOC_LITERAL(46, 16),  // "QList<McVersion>"
        QT_MOC_LITERAL(63, 8),  // "versions"
        QT_MOC_LITERAL(72, 10),  // "fetchError"
        QT_MOC_LITERAL(83, 5)   // "error"
    },
    "ShadowLauncher::VersionManager",
    "versionsReady",
    "",
    "QList<McVersion>",
    "versions",
    "fetchError",
    "error"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSShadowLauncherSCOPEVersionManagerENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   26,    2, 0x06,    1 /* Public */,
       5,    1,   29,    2, 0x06,    3 /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, QMetaType::QString,    6,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::VersionManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSShadowLauncherSCOPEVersionManagerENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSShadowLauncherSCOPEVersionManagerENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSShadowLauncherSCOPEVersionManagerENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<VersionManager, std::true_type>,
        // method 'versionsReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVector<McVersion> &, std::false_type>,
        // method 'fetchError'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::VersionManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<VersionManager *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->versionsReady((*reinterpret_cast< std::add_pointer_t<QList<McVersion>>>(_a[1]))); break;
        case 1: _t->fetchError((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (VersionManager::*)(const QVector<McVersion> & );
            if (_t _q_method = &VersionManager::versionsReady; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (VersionManager::*)(const QString & );
            if (_t _q_method = &VersionManager::fetchError; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject *ShadowLauncher::VersionManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::VersionManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSShadowLauncherSCOPEVersionManagerENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::VersionManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::VersionManager::versionsReady(const QVector<McVersion> & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void ShadowLauncher::VersionManager::fetchError(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
