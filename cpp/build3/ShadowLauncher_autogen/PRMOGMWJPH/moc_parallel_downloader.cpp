/****************************************************************************
** Meta object code from reading C++ file 'parallel_downloader.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/core/parallel_downloader.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'parallel_downloader.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSShadowLauncherSCOPEParallelDownloaderENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSShadowLauncherSCOPEParallelDownloaderENDCLASS = QtMocHelpers::stringData(
    "ShadowLauncher::ParallelDownloader",
    "progressChanged",
    "",
    "completedFiles",
    "totalFiles",
    "downloadedBytes",
    "totalBytes",
    "fileProgress",
    "fileName",
    "received",
    "total",
    "fileCompleted",
    "success",
    "allFinished",
    "failedCount",
    "failedFiles",
    "logMessage",
    "msg",
    "stateChanged",
    "state"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSShadowLauncherSCOPEParallelDownloaderENDCLASS_t {
    uint offsetsAndSizes[40];
    char stringdata0[35];
    char stringdata1[16];
    char stringdata2[1];
    char stringdata3[15];
    char stringdata4[11];
    char stringdata5[16];
    char stringdata6[11];
    char stringdata7[13];
    char stringdata8[9];
    char stringdata9[9];
    char stringdata10[6];
    char stringdata11[14];
    char stringdata12[8];
    char stringdata13[12];
    char stringdata14[12];
    char stringdata15[12];
    char stringdata16[11];
    char stringdata17[4];
    char stringdata18[13];
    char stringdata19[6];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSShadowLauncherSCOPEParallelDownloaderENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSShadowLauncherSCOPEParallelDownloaderENDCLASS_t qt_meta_stringdata_CLASSShadowLauncherSCOPEParallelDownloaderENDCLASS = {
    {
        QT_MOC_LITERAL(0, 34),  // "ShadowLauncher::ParallelDownl..."
        QT_MOC_LITERAL(35, 15),  // "progressChanged"
        QT_MOC_LITERAL(51, 0),  // ""
        QT_MOC_LITERAL(52, 14),  // "completedFiles"
        QT_MOC_LITERAL(67, 10),  // "totalFiles"
        QT_MOC_LITERAL(78, 15),  // "downloadedBytes"
        QT_MOC_LITERAL(94, 10),  // "totalBytes"
        QT_MOC_LITERAL(105, 12),  // "fileProgress"
        QT_MOC_LITERAL(118, 8),  // "fileName"
        QT_MOC_LITERAL(127, 8),  // "received"
        QT_MOC_LITERAL(136, 5),  // "total"
        QT_MOC_LITERAL(142, 13),  // "fileCompleted"
        QT_MOC_LITERAL(156, 7),  // "success"
        QT_MOC_LITERAL(164, 11),  // "allFinished"
        QT_MOC_LITERAL(176, 11),  // "failedCount"
        QT_MOC_LITERAL(188, 11),  // "failedFiles"
        QT_MOC_LITERAL(200, 10),  // "logMessage"
        QT_MOC_LITERAL(211, 3),  // "msg"
        QT_MOC_LITERAL(215, 12),  // "stateChanged"
        QT_MOC_LITERAL(228, 5)   // "state"
    },
    "ShadowLauncher::ParallelDownloader",
    "progressChanged",
    "",
    "completedFiles",
    "totalFiles",
    "downloadedBytes",
    "totalBytes",
    "fileProgress",
    "fileName",
    "received",
    "total",
    "fileCompleted",
    "success",
    "allFinished",
    "failedCount",
    "failedFiles",
    "logMessage",
    "msg",
    "stateChanged",
    "state"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSShadowLauncherSCOPEParallelDownloaderENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       5,   82, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       6,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    4,   50,    2, 0x06,    6 /* Public */,
       7,    3,   59,    2, 0x06,   11 /* Public */,
      11,    2,   66,    2, 0x06,   15 /* Public */,
      13,    3,   71,    2, 0x06,   18 /* Public */,
      16,    1,   78,    2, 0x06,   22 /* Public */,
      18,    0,   81,    2, 0x06,   24 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::LongLong, QMetaType::LongLong,    3,    4,    5,    6,
    QMetaType::Void, QMetaType::QString, QMetaType::LongLong, QMetaType::LongLong,    8,    9,   10,
    QMetaType::Void, QMetaType::QString, QMetaType::Bool,    8,   12,
    QMetaType::Void, QMetaType::Bool, QMetaType::Int, QMetaType::QStringList,   12,   14,   15,
    QMetaType::Void, QMetaType::QString,   17,
    QMetaType::Void,

 // properties: name, type, flags
       4, QMetaType::Int, 0x00015001, uint(0), 0,
       3, QMetaType::Int, 0x00015001, uint(0), 0,
       6, QMetaType::LongLong, 0x00015001, uint(0), 0,
       5, QMetaType::LongLong, 0x00015001, uint(0), 0,
      19, QMetaType::QString, 0x00015001, uint(5), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::ParallelDownloader::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSShadowLauncherSCOPEParallelDownloaderENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSShadowLauncherSCOPEParallelDownloaderENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSShadowLauncherSCOPEParallelDownloaderENDCLASS_t,
        // property 'totalFiles'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'completedFiles'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'totalBytes'
        QtPrivate::TypeAndForceComplete<qint64, std::true_type>,
        // property 'downloadedBytes'
        QtPrivate::TypeAndForceComplete<qint64, std::true_type>,
        // property 'state'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<ParallelDownloader, std::true_type>,
        // method 'progressChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        // method 'fileProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        // method 'fileCompleted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'allFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        // method 'logMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'stateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::ParallelDownloader::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ParallelDownloader *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->progressChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[4]))); break;
        case 1: _t->fileProgress((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[3]))); break;
        case 2: _t->fileCompleted((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 3: _t->allFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[3]))); break;
        case 4: _t->logMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->stateChanged(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ParallelDownloader::*)(int , int , qint64 , qint64 );
            if (_t _q_method = &ParallelDownloader::progressChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (ParallelDownloader::*)(const QString & , qint64 , qint64 );
            if (_t _q_method = &ParallelDownloader::fileProgress; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (ParallelDownloader::*)(const QString & , bool );
            if (_t _q_method = &ParallelDownloader::fileCompleted; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (ParallelDownloader::*)(bool , int , const QStringList & );
            if (_t _q_method = &ParallelDownloader::allFinished; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (ParallelDownloader::*)(const QString & );
            if (_t _q_method = &ParallelDownloader::logMessage; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (ParallelDownloader::*)();
            if (_t _q_method = &ParallelDownloader::stateChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
    }else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<ParallelDownloader *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< int*>(_v) = _t->totalFiles(); break;
        case 1: *reinterpret_cast< int*>(_v) = _t->completedFiles(); break;
        case 2: *reinterpret_cast< qint64*>(_v) = _t->totalBytes(); break;
        case 3: *reinterpret_cast< qint64*>(_v) = _t->downloadedBytes(); break;
        case 4: *reinterpret_cast< QString*>(_v) = _t->stateStr(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
    } else if (_c == QMetaObject::ResetProperty) {
    } else if (_c == QMetaObject::BindableProperty) {
    }
}

const QMetaObject *ShadowLauncher::ParallelDownloader::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::ParallelDownloader::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSShadowLauncherSCOPEParallelDownloaderENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::ParallelDownloader::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
    }else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::ParallelDownloader::progressChanged(int _t1, int _t2, qint64 _t3, qint64 _t4)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void ShadowLauncher::ParallelDownloader::fileProgress(const QString & _t1, qint64 _t2, qint64 _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void ShadowLauncher::ParallelDownloader::fileCompleted(const QString & _t1, bool _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void ShadowLauncher::ParallelDownloader::allFinished(bool _t1, int _t2, const QStringList & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void ShadowLauncher::ParallelDownloader::logMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void ShadowLauncher::ParallelDownloader::stateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}
QT_WARNING_POP
