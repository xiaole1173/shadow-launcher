/****************************************************************************
** Meta object code from reading C++ file 'crash_detector.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/core/crash_detector.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'crash_detector.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14ShadowLauncher11CrashReportE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN14ShadowLauncher11CrashReportE = QtMocHelpers::stringData(
    "ShadowLauncher::CrashReport",
    "type",
    "reason",
    "description",
    "suspectedMods",
    "filePath",
    "timestamp",
    "isValid"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN14ShadowLauncher11CrashReportE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       7,   14, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       4,       // flags
       0,       // signalCount

 // properties: name, type, flags, notifyId, revision
       1, QMetaType::QString, 0x00015003, uint(-1), 0,
       2, QMetaType::QString, 0x00015003, uint(-1), 0,
       3, QMetaType::QString, 0x00015003, uint(-1), 0,
       4, QMetaType::QStringList, 0x00015003, uint(-1), 0,
       5, QMetaType::QString, 0x00015003, uint(-1), 0,
       6, QMetaType::QDateTime, 0x00015003, uint(-1), 0,
       7, QMetaType::Bool, 0x00015003, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::CrashReport::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_ZN14ShadowLauncher11CrashReportE.offsetsAndSizes,
    qt_meta_data_ZN14ShadowLauncher11CrashReportE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN14ShadowLauncher11CrashReportE_t,
        // property 'type'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'reason'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'description'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'suspectedMods'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'filePath'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'timestamp'
        QtPrivate::TypeAndForceComplete<QDateTime, std::true_type>,
        // property 'isValid'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<CrashReport, std::true_type>
    >,
    nullptr
} };

void ShadowLauncher::CrashReport::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = reinterpret_cast<CrashReport *>(_o);
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->type; break;
        case 1: *reinterpret_cast< QString*>(_v) = _t->reason; break;
        case 2: *reinterpret_cast< QString*>(_v) = _t->description; break;
        case 3: *reinterpret_cast< QStringList*>(_v) = _t->suspectedMods; break;
        case 4: *reinterpret_cast< QString*>(_v) = _t->filePath; break;
        case 5: *reinterpret_cast< QDateTime*>(_v) = _t->timestamp; break;
        case 6: *reinterpret_cast< bool*>(_v) = _t->isValid; break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0:
            if (_t->type != *reinterpret_cast< QString*>(_v)) {
                _t->type = *reinterpret_cast< QString*>(_v);
            }
            break;
        case 1:
            if (_t->reason != *reinterpret_cast< QString*>(_v)) {
                _t->reason = *reinterpret_cast< QString*>(_v);
            }
            break;
        case 2:
            if (_t->description != *reinterpret_cast< QString*>(_v)) {
                _t->description = *reinterpret_cast< QString*>(_v);
            }
            break;
        case 3:
            if (_t->suspectedMods != *reinterpret_cast< QStringList*>(_v)) {
                _t->suspectedMods = *reinterpret_cast< QStringList*>(_v);
            }
            break;
        case 4:
            if (_t->filePath != *reinterpret_cast< QString*>(_v)) {
                _t->filePath = *reinterpret_cast< QString*>(_v);
            }
            break;
        case 5:
            if (_t->timestamp != *reinterpret_cast< QDateTime*>(_v)) {
                _t->timestamp = *reinterpret_cast< QDateTime*>(_v);
            }
            break;
        case 6:
            if (_t->isValid != *reinterpret_cast< bool*>(_v)) {
                _t->isValid = *reinterpret_cast< bool*>(_v);
            }
            break;
        default: break;
        }
    }
}
namespace {
struct qt_meta_tag_ZN14ShadowLauncher13CrashDetectorE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN14ShadowLauncher13CrashDetectorE = QtMocHelpers::stringData(
    "ShadowLauncher::CrashDetector"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN14ShadowLauncher13CrashDetectorE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::CrashDetector::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN14ShadowLauncher13CrashDetectorE.offsetsAndSizes,
    qt_meta_data_ZN14ShadowLauncher13CrashDetectorE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN14ShadowLauncher13CrashDetectorE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<CrashDetector, std::true_type>
    >,
    nullptr
} };

void ShadowLauncher::CrashDetector::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<CrashDetector *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *ShadowLauncher::CrashDetector::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::CrashDetector::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN14ShadowLauncher13CrashDetectorE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::CrashDetector::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    return _id;
}
QT_WARNING_POP
