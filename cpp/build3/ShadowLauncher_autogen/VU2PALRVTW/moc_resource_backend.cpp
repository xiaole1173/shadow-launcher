/****************************************************************************
** Meta object code from reading C++ file 'resource_backend.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/backend/resource_backend.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'resource_backend.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSShadowLauncherSCOPEResourceBackendENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSShadowLauncherSCOPEResourceBackendENDCLASS = QtMocHelpers::stringData(
    "ShadowLauncher::ResourceBackend",
    "downloadProgressChanged",
    "",
    "completed",
    "total",
    "fileName",
    "downloadStateChanged",
    "downloadFinished",
    "slug",
    "success",
    "filePath",
    "searchResultsReady",
    "results",
    "logMessage",
    "msg",
    "onSearchCompleted",
    "totalHits",
    "onDownloadProgress",
    "name",
    "received",
    "onDownloadFinished",
    "getPopularMods",
    "loader",
    "getShaderList",
    "searchMods",
    "query",
    "downloadMod",
    "downloadShader",
    "cancelDownload",
    "downloading",
    "dlProgress",
    "dlTotal",
    "dlFile"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSShadowLauncherSCOPEResourceBackendENDCLASS_t {
    uint offsetsAndSizes[66];
    char stringdata0[32];
    char stringdata1[24];
    char stringdata2[1];
    char stringdata3[10];
    char stringdata4[6];
    char stringdata5[9];
    char stringdata6[21];
    char stringdata7[17];
    char stringdata8[5];
    char stringdata9[8];
    char stringdata10[9];
    char stringdata11[19];
    char stringdata12[8];
    char stringdata13[11];
    char stringdata14[4];
    char stringdata15[18];
    char stringdata16[10];
    char stringdata17[19];
    char stringdata18[5];
    char stringdata19[9];
    char stringdata20[19];
    char stringdata21[15];
    char stringdata22[7];
    char stringdata23[14];
    char stringdata24[11];
    char stringdata25[6];
    char stringdata26[12];
    char stringdata27[15];
    char stringdata28[15];
    char stringdata29[12];
    char stringdata30[11];
    char stringdata31[8];
    char stringdata32[7];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSShadowLauncherSCOPEResourceBackendENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSShadowLauncherSCOPEResourceBackendENDCLASS_t qt_meta_stringdata_CLASSShadowLauncherSCOPEResourceBackendENDCLASS = {
    {
        QT_MOC_LITERAL(0, 31),  // "ShadowLauncher::ResourceBackend"
        QT_MOC_LITERAL(32, 23),  // "downloadProgressChanged"
        QT_MOC_LITERAL(56, 0),  // ""
        QT_MOC_LITERAL(57, 9),  // "completed"
        QT_MOC_LITERAL(67, 5),  // "total"
        QT_MOC_LITERAL(73, 8),  // "fileName"
        QT_MOC_LITERAL(82, 20),  // "downloadStateChanged"
        QT_MOC_LITERAL(103, 16),  // "downloadFinished"
        QT_MOC_LITERAL(120, 4),  // "slug"
        QT_MOC_LITERAL(125, 7),  // "success"
        QT_MOC_LITERAL(133, 8),  // "filePath"
        QT_MOC_LITERAL(142, 18),  // "searchResultsReady"
        QT_MOC_LITERAL(161, 7),  // "results"
        QT_MOC_LITERAL(169, 10),  // "logMessage"
        QT_MOC_LITERAL(180, 3),  // "msg"
        QT_MOC_LITERAL(184, 17),  // "onSearchCompleted"
        QT_MOC_LITERAL(202, 9),  // "totalHits"
        QT_MOC_LITERAL(212, 18),  // "onDownloadProgress"
        QT_MOC_LITERAL(231, 4),  // "name"
        QT_MOC_LITERAL(236, 8),  // "received"
        QT_MOC_LITERAL(245, 18),  // "onDownloadFinished"
        QT_MOC_LITERAL(264, 14),  // "getPopularMods"
        QT_MOC_LITERAL(279, 6),  // "loader"
        QT_MOC_LITERAL(286, 13),  // "getShaderList"
        QT_MOC_LITERAL(300, 10),  // "searchMods"
        QT_MOC_LITERAL(311, 5),  // "query"
        QT_MOC_LITERAL(317, 11),  // "downloadMod"
        QT_MOC_LITERAL(329, 14),  // "downloadShader"
        QT_MOC_LITERAL(344, 14),  // "cancelDownload"
        QT_MOC_LITERAL(359, 11),  // "downloading"
        QT_MOC_LITERAL(371, 10),  // "dlProgress"
        QT_MOC_LITERAL(382, 7),  // "dlTotal"
        QT_MOC_LITERAL(390, 6)   // "dlFile"
    },
    "ShadowLauncher::ResourceBackend",
    "downloadProgressChanged",
    "",
    "completed",
    "total",
    "fileName",
    "downloadStateChanged",
    "downloadFinished",
    "slug",
    "success",
    "filePath",
    "searchResultsReady",
    "results",
    "logMessage",
    "msg",
    "onSearchCompleted",
    "totalHits",
    "onDownloadProgress",
    "name",
    "received",
    "onDownloadFinished",
    "getPopularMods",
    "loader",
    "getShaderList",
    "searchMods",
    "query",
    "downloadMod",
    "downloadShader",
    "cancelDownload",
    "downloading",
    "dlProgress",
    "dlTotal",
    "dlFile"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSShadowLauncherSCOPEResourceBackendENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
      15,   14, // methods
       4,  165, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       5,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    3,  104,    2, 0x06,    5 /* Public */,
       6,    0,  111,    2, 0x06,    9 /* Public */,
       7,    3,  112,    2, 0x06,   10 /* Public */,
      11,    1,  119,    2, 0x06,   14 /* Public */,
      13,    1,  122,    2, 0x06,   16 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      15,    2,  125,    2, 0x08,   18 /* Private */,
      17,    3,  130,    2, 0x08,   21 /* Private */,
      20,    3,  137,    2, 0x08,   25 /* Private */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      21,    1,  144,    2, 0x02,   29 /* Public */,
      23,    0,  147,    2, 0x02,   31 /* Public */,
      24,    2,  148,    2, 0x02,   32 /* Public */,
      24,    1,  153,    2, 0x22,   35 /* Public | MethodCloned */,
      26,    2,  156,    2, 0x02,   37 /* Public */,
      27,    1,  161,    2, 0x02,   40 /* Public */,
      28,    0,  164,    2, 0x02,   42 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::QString,    3,    4,    5,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::Bool, QMetaType::QString,    8,    9,   10,
    QMetaType::Void, QMetaType::QVariantList,   12,
    QMetaType::Void, QMetaType::QString,   14,

 // slots: parameters
    QMetaType::Void, QMetaType::QJsonArray, QMetaType::Int,   12,   16,
    QMetaType::Void, QMetaType::QString, QMetaType::LongLong, QMetaType::LongLong,   18,   19,    4,
    QMetaType::Void, QMetaType::QString, QMetaType::Bool, QMetaType::QString,    8,    9,   10,

 // methods: parameters
    QMetaType::QVariantList, QMetaType::QString,   22,
    QMetaType::QVariantList,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   25,   22,
    QMetaType::Void, QMetaType::QString,   25,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,    8,   22,
    QMetaType::Void, QMetaType::QString,    8,
    QMetaType::Void,

 // properties: name, type, flags
      29, QMetaType::Bool, 0x00015001, uint(1), 0,
      30, QMetaType::Int, 0x00015001, uint(0), 0,
      31, QMetaType::Int, 0x00015001, uint(0), 0,
      32, QMetaType::QString, 0x00015001, uint(0), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::ResourceBackend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSShadowLauncherSCOPEResourceBackendENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSShadowLauncherSCOPEResourceBackendENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSShadowLauncherSCOPEResourceBackendENDCLASS_t,
        // property 'downloading'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'dlProgress'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'dlTotal'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'dlFile'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<ResourceBackend, std::true_type>,
        // method 'downloadProgressChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'downloadStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'downloadFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'searchResultsReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantList &, std::false_type>,
        // method 'logMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onSearchCompleted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QJsonArray &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onDownloadProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        // method 'onDownloadFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'getPopularMods'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'getShaderList'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        // method 'searchMods'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'searchMods'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'downloadMod'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'downloadShader'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cancelDownload'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::ResourceBackend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ResourceBackend *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->downloadProgressChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 1: _t->downloadStateChanged(); break;
        case 2: _t->downloadFinished((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 3: _t->searchResultsReady((*reinterpret_cast< std::add_pointer_t<QVariantList>>(_a[1]))); break;
        case 4: _t->logMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->onSearchCompleted((*reinterpret_cast< std::add_pointer_t<QJsonArray>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 6: _t->onDownloadProgress((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[3]))); break;
        case 7: _t->onDownloadFinished((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 8: { QVariantList _r = _t->getPopularMods((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 9: { QVariantList _r = _t->getShaderList();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 10: _t->searchMods((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 11: _t->searchMods((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 12: _t->downloadMod((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 13: _t->downloadShader((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 14: _t->cancelDownload(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ResourceBackend::*)(int , int , const QString & );
            if (_t _q_method = &ResourceBackend::downloadProgressChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (ResourceBackend::*)();
            if (_t _q_method = &ResourceBackend::downloadStateChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (ResourceBackend::*)(const QString & , bool , const QString & );
            if (_t _q_method = &ResourceBackend::downloadFinished; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (ResourceBackend::*)(const QVariantList & );
            if (_t _q_method = &ResourceBackend::searchResultsReady; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (ResourceBackend::*)(const QString & );
            if (_t _q_method = &ResourceBackend::logMessage; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
    }else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<ResourceBackend *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->isDownloading(); break;
        case 1: *reinterpret_cast< int*>(_v) = _t->dlProgress(); break;
        case 2: *reinterpret_cast< int*>(_v) = _t->dlTotal(); break;
        case 3: *reinterpret_cast< QString*>(_v) = _t->dlFile(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
    } else if (_c == QMetaObject::ResetProperty) {
    } else if (_c == QMetaObject::BindableProperty) {
    }
}

const QMetaObject *ShadowLauncher::ResourceBackend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::ResourceBackend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSShadowLauncherSCOPEResourceBackendENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::ResourceBackend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 15)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 15)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 15;
    }else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::ResourceBackend::downloadProgressChanged(int _t1, int _t2, const QString & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void ShadowLauncher::ResourceBackend::downloadStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ShadowLauncher::ResourceBackend::downloadFinished(const QString & _t1, bool _t2, const QString & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void ShadowLauncher::ResourceBackend::searchResultsReady(const QVariantList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void ShadowLauncher::ResourceBackend::logMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}
QT_WARNING_POP
