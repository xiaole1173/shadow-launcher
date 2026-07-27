/****************************************************************************
** Meta object code from reading C++ file 'modpack_importer.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/core/modpack_importer.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'modpack_importer.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14ShadowLauncher15ModpackImporterE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN14ShadowLauncher15ModpackImporterE = QtMocHelpers::stringData(
    "ShadowLauncher::ModpackImporter",
    "busyChanged",
    "",
    "progressChanged",
    "hasResultChanged",
    "modListChanged",
    "importFinished",
    "success",
    "versionName",
    "error",
    "startImport",
    "zipFilePath",
    "cancelImport",
    "dismissResult",
    "busy",
    "statusText",
    "progress",
    "currentFile",
    "hasResult",
    "resultName",
    "resultVersionId",
    "modItems",
    "QVariantList",
    "Format",
    "Unknown",
    "Modrinth",
    "CurseForge",
    "Step",
    "StepNone",
    "StepParsing",
    "StepInstallMc",
    "StepInstallLoader",
    "StepDownloadMods",
    "StepExtractOverrides",
    "StepFinished",
    "StepError"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN14ShadowLauncher15ModpackImporterE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
       8,   78, // properties
       2,  118, // enums/sets
       0,    0, // constructors
       0,       // flags
       5,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   62,    2, 0x06,   11 /* Public */,
       3,    0,   63,    2, 0x06,   12 /* Public */,
       4,    0,   64,    2, 0x06,   13 /* Public */,
       5,    0,   65,    2, 0x06,   14 /* Public */,
       6,    3,   66,    2, 0x06,   15 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      10,    1,   73,    2, 0x02,   19 /* Public */,
      12,    0,   76,    2, 0x02,   21 /* Public */,
      13,    0,   77,    2, 0x02,   22 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString, QMetaType::QString,    7,    8,    9,

 // methods: parameters
    QMetaType::Void, QMetaType::QString,   11,
    QMetaType::Void,
    QMetaType::Void,

 // properties: name, type, flags, notifyId, revision
      14, QMetaType::Bool, 0x00015001, uint(0), 0,
      15, QMetaType::QString, 0x00015001, uint(1), 0,
      16, QMetaType::QReal, 0x00015001, uint(1), 0,
      17, QMetaType::QString, 0x00015001, uint(1), 0,
      18, QMetaType::Bool, 0x00015001, uint(2), 0,
      19, QMetaType::QString, 0x00015001, uint(2), 0,
      20, QMetaType::QString, 0x00015001, uint(2), 0,
      21, 0x80000000 | 22, 0x00015009, uint(3), 0,

 // enums: name, alias, flags, count, data
      23,   23, 0x0,    3,  128,
      27,   27, 0x0,    8,  134,

 // enum data: key, value
      24, uint(ShadowLauncher::ModpackImporter::Unknown),
      25, uint(ShadowLauncher::ModpackImporter::Modrinth),
      26, uint(ShadowLauncher::ModpackImporter::CurseForge),
      28, uint(ShadowLauncher::ModpackImporter::StepNone),
      29, uint(ShadowLauncher::ModpackImporter::StepParsing),
      30, uint(ShadowLauncher::ModpackImporter::StepInstallMc),
      31, uint(ShadowLauncher::ModpackImporter::StepInstallLoader),
      32, uint(ShadowLauncher::ModpackImporter::StepDownloadMods),
      33, uint(ShadowLauncher::ModpackImporter::StepExtractOverrides),
      34, uint(ShadowLauncher::ModpackImporter::StepFinished),
      35, uint(ShadowLauncher::ModpackImporter::StepError),

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::ModpackImporter::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN14ShadowLauncher15ModpackImporterE.offsetsAndSizes,
    qt_meta_data_ZN14ShadowLauncher15ModpackImporterE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN14ShadowLauncher15ModpackImporterE_t,
        // property 'busy'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'statusText'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'progress'
        QtPrivate::TypeAndForceComplete<qreal, std::true_type>,
        // property 'currentFile'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'hasResult'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'resultName'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'resultVersionId'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'modItems'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // enum 'Format'
        QtPrivate::TypeAndForceComplete<ModpackImporter::Format, std::true_type>,
        // enum 'Step'
        QtPrivate::TypeAndForceComplete<ModpackImporter::Step, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<ModpackImporter, std::true_type>,
        // method 'busyChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'progressChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'hasResultChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'modListChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'importFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'startImport'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cancelImport'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'dismissResult'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::ModpackImporter::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ModpackImporter *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->busyChanged(); break;
        case 1: _t->progressChanged(); break;
        case 2: _t->hasResultChanged(); break;
        case 3: _t->modListChanged(); break;
        case 4: _t->importFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 5: _t->startImport((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->cancelImport(); break;
        case 7: _t->dismissResult(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (ModpackImporter::*)();
            if (_q_method_type _q_method = &ModpackImporter::busyChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (ModpackImporter::*)();
            if (_q_method_type _q_method = &ModpackImporter::progressChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (ModpackImporter::*)();
            if (_q_method_type _q_method = &ModpackImporter::hasResultChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (ModpackImporter::*)();
            if (_q_method_type _q_method = &ModpackImporter::modListChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (ModpackImporter::*)(bool , const QString & , const QString & );
            if (_q_method_type _q_method = &ModpackImporter::importFinished; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->isBusy(); break;
        case 1: *reinterpret_cast< QString*>(_v) = _t->statusText(); break;
        case 2: *reinterpret_cast< qreal*>(_v) = _t->progress(); break;
        case 3: *reinterpret_cast< QString*>(_v) = _t->currentFile(); break;
        case 4: *reinterpret_cast< bool*>(_v) = _t->hasResult(); break;
        case 5: *reinterpret_cast< QString*>(_v) = _t->resultName(); break;
        case 6: *reinterpret_cast< QString*>(_v) = _t->resultVersionId(); break;
        case 7: *reinterpret_cast< QVariantList*>(_v) = _t->modItems(); break;
        default: break;
        }
    }
}

const QMetaObject *ShadowLauncher::ModpackImporter::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::ModpackImporter::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN14ShadowLauncher15ModpackImporterE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::ModpackImporter::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 8;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::ModpackImporter::busyChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ShadowLauncher::ModpackImporter::progressChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ShadowLauncher::ModpackImporter::hasResultChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void ShadowLauncher::ModpackImporter::modListChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void ShadowLauncher::ModpackImporter::importFinished(bool _t1, const QString & _t2, const QString & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}
QT_WARNING_POP
