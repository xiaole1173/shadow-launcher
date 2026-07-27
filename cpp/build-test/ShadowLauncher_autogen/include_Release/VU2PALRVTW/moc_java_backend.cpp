/****************************************************************************
** Meta object code from reading C++ file 'java_backend.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/backend/java_backend.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'java_backend.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14ShadowLauncher11JavaBackendE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN14ShadowLauncher11JavaBackendE = QtMocHelpers::stringData(
    "ShadowLauncher::JavaBackend",
    "javaVersionsChanged",
    "",
    "javaTypesChanged",
    "javaArchsChanged",
    "javaOSesChanged",
    "javaFilesChanged",
    "fetchingChanged",
    "selectedVersionChanged",
    "selectedTypeChanged",
    "selectedArchChanged",
    "selectedOSChanged",
    "downloadProgress",
    "pct",
    "dlBytes",
    "totalBytes",
    "speedMBps",
    "downloadFinished",
    "ok",
    "path",
    "logMessage",
    "msg",
    "refreshVersions",
    "fetchTypes",
    "fetchArchs",
    "fetchOSes",
    "fetchFiles",
    "fileDownloadUrl",
    "filename",
    "downloadJavaFile",
    "downloadJavaFileTo",
    "outDir",
    "expectedSize",
    "javaInstallDir",
    "cancelDownload",
    "javaVersions",
    "javaTypes",
    "javaArchs",
    "javaOSes",
    "javaFiles",
    "QVariantList",
    "fetching",
    "selectedVersion",
    "selectedType",
    "selectedArch",
    "selectedOS"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN14ShadowLauncher11JavaBackendE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      24,   14, // methods
      10,  210, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      13,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  158,    2, 0x06,   11 /* Public */,
       3,    0,  159,    2, 0x06,   12 /* Public */,
       4,    0,  160,    2, 0x06,   13 /* Public */,
       5,    0,  161,    2, 0x06,   14 /* Public */,
       6,    0,  162,    2, 0x06,   15 /* Public */,
       7,    0,  163,    2, 0x06,   16 /* Public */,
       8,    0,  164,    2, 0x06,   17 /* Public */,
       9,    0,  165,    2, 0x06,   18 /* Public */,
      10,    0,  166,    2, 0x06,   19 /* Public */,
      11,    0,  167,    2, 0x06,   20 /* Public */,
      12,    4,  168,    2, 0x06,   21 /* Public */,
      17,    2,  177,    2, 0x06,   26 /* Public */,
      20,    1,  182,    2, 0x06,   29 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      22,    0,  185,    2, 0x02,   31 /* Public */,
      23,    0,  186,    2, 0x02,   32 /* Public */,
      24,    0,  187,    2, 0x02,   33 /* Public */,
      25,    0,  188,    2, 0x02,   34 /* Public */,
      26,    0,  189,    2, 0x02,   35 /* Public */,
      27,    1,  190,    2, 0x102,   36 /* Public | MethodIsConst  */,
      29,    1,  193,    2, 0x02,   38 /* Public */,
      30,    3,  196,    2, 0x02,   40 /* Public */,
      30,    2,  203,    2, 0x22,   44 /* Public | MethodCloned */,
      33,    0,  208,    2, 0x102,   47 /* Public | MethodIsConst  */,
      34,    0,  209,    2, 0x02,   48 /* Public */,

 // signals: parameters
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
    QMetaType::Void, QMetaType::Int, QMetaType::LongLong, QMetaType::LongLong, QMetaType::Double,   13,   14,   15,   16,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,   18,   19,
    QMetaType::Void, QMetaType::QString,   21,

 // methods: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::QString, QMetaType::QString,   28,
    QMetaType::Void, QMetaType::QString,   28,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::LongLong,   28,   31,   32,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   28,   31,
    QMetaType::QString,
    QMetaType::Void,

 // properties: name, type, flags, notifyId, revision
      35, QMetaType::QStringList, 0x00015001, uint(0), 0,
      36, QMetaType::QStringList, 0x00015001, uint(1), 0,
      37, QMetaType::QStringList, 0x00015001, uint(2), 0,
      38, QMetaType::QStringList, 0x00015001, uint(3), 0,
      39, 0x80000000 | 40, 0x00015009, uint(4), 0,
      41, QMetaType::Bool, 0x00015001, uint(5), 0,
      42, QMetaType::QString, 0x00015103, uint(6), 0,
      43, QMetaType::QString, 0x00015103, uint(7), 0,
      44, QMetaType::QString, 0x00015103, uint(8), 0,
      45, QMetaType::QString, 0x00015103, uint(9), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::JavaBackend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN14ShadowLauncher11JavaBackendE.offsetsAndSizes,
    qt_meta_data_ZN14ShadowLauncher11JavaBackendE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN14ShadowLauncher11JavaBackendE_t,
        // property 'javaVersions'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'javaTypes'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'javaArchs'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'javaOSes'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'javaFiles'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // property 'fetching'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'selectedVersion'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'selectedType'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'selectedArch'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'selectedOS'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<JavaBackend, std::true_type>,
        // method 'javaVersionsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'javaTypesChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'javaArchsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'javaOSesChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'javaFilesChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'fetchingChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectedVersionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectedTypeChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectedArchChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectedOSChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'downloadProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'downloadFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'logMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'refreshVersions'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'fetchTypes'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'fetchArchs'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'fetchOSes'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'fetchFiles'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'fileDownloadUrl'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'downloadJavaFile'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'downloadJavaFileTo'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        // method 'downloadJavaFileTo'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'javaInstallDir'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'cancelDownload'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::JavaBackend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<JavaBackend *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->javaVersionsChanged(); break;
        case 1: _t->javaTypesChanged(); break;
        case 2: _t->javaArchsChanged(); break;
        case 3: _t->javaOSesChanged(); break;
        case 4: _t->javaFilesChanged(); break;
        case 5: _t->fetchingChanged(); break;
        case 6: _t->selectedVersionChanged(); break;
        case 7: _t->selectedTypeChanged(); break;
        case 8: _t->selectedArchChanged(); break;
        case 9: _t->selectedOSChanged(); break;
        case 10: _t->downloadProgress((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[4]))); break;
        case 11: _t->downloadFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 12: _t->logMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 13: _t->refreshVersions(); break;
        case 14: _t->fetchTypes(); break;
        case 15: _t->fetchArchs(); break;
        case 16: _t->fetchOSes(); break;
        case 17: _t->fetchFiles(); break;
        case 18: { QString _r = _t->fileDownloadUrl((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 19: _t->downloadJavaFile((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 20: _t->downloadJavaFileTo((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[3]))); break;
        case 21: _t->downloadJavaFileTo((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 22: { QString _r = _t->javaInstallDir();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 23: _t->cancelDownload(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (JavaBackend::*)();
            if (_q_method_type _q_method = &JavaBackend::javaVersionsChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (JavaBackend::*)();
            if (_q_method_type _q_method = &JavaBackend::javaTypesChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (JavaBackend::*)();
            if (_q_method_type _q_method = &JavaBackend::javaArchsChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (JavaBackend::*)();
            if (_q_method_type _q_method = &JavaBackend::javaOSesChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (JavaBackend::*)();
            if (_q_method_type _q_method = &JavaBackend::javaFilesChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (JavaBackend::*)();
            if (_q_method_type _q_method = &JavaBackend::fetchingChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (JavaBackend::*)();
            if (_q_method_type _q_method = &JavaBackend::selectedVersionChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _q_method_type = void (JavaBackend::*)();
            if (_q_method_type _q_method = &JavaBackend::selectedTypeChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _q_method_type = void (JavaBackend::*)();
            if (_q_method_type _q_method = &JavaBackend::selectedArchChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 8;
                return;
            }
        }
        {
            using _q_method_type = void (JavaBackend::*)();
            if (_q_method_type _q_method = &JavaBackend::selectedOSChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 9;
                return;
            }
        }
        {
            using _q_method_type = void (JavaBackend::*)(int , qint64 , qint64 , double );
            if (_q_method_type _q_method = &JavaBackend::downloadProgress; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 10;
                return;
            }
        }
        {
            using _q_method_type = void (JavaBackend::*)(bool , const QString & );
            if (_q_method_type _q_method = &JavaBackend::downloadFinished; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 11;
                return;
            }
        }
        {
            using _q_method_type = void (JavaBackend::*)(const QString & );
            if (_q_method_type _q_method = &JavaBackend::logMessage; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 12;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QStringList*>(_v) = _t->javaVersions(); break;
        case 1: *reinterpret_cast< QStringList*>(_v) = _t->javaTypes(); break;
        case 2: *reinterpret_cast< QStringList*>(_v) = _t->javaArchs(); break;
        case 3: *reinterpret_cast< QStringList*>(_v) = _t->javaOSes(); break;
        case 4: *reinterpret_cast< QVariantList*>(_v) = _t->javaFiles(); break;
        case 5: *reinterpret_cast< bool*>(_v) = _t->fetching(); break;
        case 6: *reinterpret_cast< QString*>(_v) = _t->selectedVersion(); break;
        case 7: *reinterpret_cast< QString*>(_v) = _t->selectedType(); break;
        case 8: *reinterpret_cast< QString*>(_v) = _t->selectedArch(); break;
        case 9: *reinterpret_cast< QString*>(_v) = _t->selectedOS(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 6: _t->setSelectedVersion(*reinterpret_cast< QString*>(_v)); break;
        case 7: _t->setSelectedType(*reinterpret_cast< QString*>(_v)); break;
        case 8: _t->setSelectedArch(*reinterpret_cast< QString*>(_v)); break;
        case 9: _t->setSelectedOS(*reinterpret_cast< QString*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *ShadowLauncher::JavaBackend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::JavaBackend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN14ShadowLauncher11JavaBackendE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::JavaBackend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 24)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 24;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 24)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 24;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::JavaBackend::javaVersionsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ShadowLauncher::JavaBackend::javaTypesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ShadowLauncher::JavaBackend::javaArchsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void ShadowLauncher::JavaBackend::javaOSesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void ShadowLauncher::JavaBackend::javaFilesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void ShadowLauncher::JavaBackend::fetchingChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void ShadowLauncher::JavaBackend::selectedVersionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void ShadowLauncher::JavaBackend::selectedTypeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void ShadowLauncher::JavaBackend::selectedArchChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void ShadowLauncher::JavaBackend::selectedOSChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void ShadowLauncher::JavaBackend::downloadProgress(int _t1, qint64 _t2, qint64 _t3, double _t4)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))) };
    QMetaObject::activate(this, &staticMetaObject, 10, _a);
}

// SIGNAL 11
void ShadowLauncher::JavaBackend::downloadFinished(bool _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 11, _a);
}

// SIGNAL 12
void ShadowLauncher::JavaBackend::logMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 12, _a);
}
QT_WARNING_POP
