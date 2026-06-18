/****************************************************************************
** Meta object code from reading C++ file 'settings_backend.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/backend/settings_backend.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'settings_backend.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSShadowLauncherSCOPESettingsBackendENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSShadowLauncherSCOPESettingsBackendENDCLASS = QtMocHelpers::stringData(
    "ShadowLauncher::SettingsBackend",
    "javaPathChanged",
    "",
    "javaReadyChanged",
    "memorySettingsChanged",
    "generalSettingsChanged",
    "isolationChanged",
    "logMessage",
    "msg",
    "scanJavaInstallations",
    "autoSelectJava",
    "detectJava",
    "getMemoryStatus",
    "setMinMemory",
    "mb",
    "setMaxMemory",
    "availableJavaList",
    "selectJavaByIndex",
    "index",
    "openJavaFileDialog",
    "browseJava",
    "setIsolationEnabled",
    "enabled",
    "migrateVersionToIsolated",
    "versionId",
    "getVersionGameDir",
    "isolationEnabled",
    "openGameDir",
    "openVersionDir",
    "deleteVersion",
    "openPath",
    "path",
    "setCloseAfterLaunch",
    "javaPath",
    "javaVersion",
    "javaMajor",
    "minMemoryMB",
    "maxMemoryMB",
    "closeAfterLaunch",
    "javaReady"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSShadowLauncherSCOPESettingsBackendENDCLASS_t {
    uint offsetsAndSizes[80];
    char stringdata0[32];
    char stringdata1[16];
    char stringdata2[1];
    char stringdata3[17];
    char stringdata4[22];
    char stringdata5[23];
    char stringdata6[17];
    char stringdata7[11];
    char stringdata8[4];
    char stringdata9[22];
    char stringdata10[15];
    char stringdata11[11];
    char stringdata12[16];
    char stringdata13[13];
    char stringdata14[3];
    char stringdata15[13];
    char stringdata16[18];
    char stringdata17[18];
    char stringdata18[6];
    char stringdata19[19];
    char stringdata20[11];
    char stringdata21[20];
    char stringdata22[8];
    char stringdata23[25];
    char stringdata24[10];
    char stringdata25[18];
    char stringdata26[17];
    char stringdata27[12];
    char stringdata28[15];
    char stringdata29[14];
    char stringdata30[9];
    char stringdata31[5];
    char stringdata32[20];
    char stringdata33[9];
    char stringdata34[12];
    char stringdata35[10];
    char stringdata36[12];
    char stringdata37[12];
    char stringdata38[17];
    char stringdata39[10];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSShadowLauncherSCOPESettingsBackendENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSShadowLauncherSCOPESettingsBackendENDCLASS_t qt_meta_stringdata_CLASSShadowLauncherSCOPESettingsBackendENDCLASS = {
    {
        QT_MOC_LITERAL(0, 31),  // "ShadowLauncher::SettingsBackend"
        QT_MOC_LITERAL(32, 15),  // "javaPathChanged"
        QT_MOC_LITERAL(48, 0),  // ""
        QT_MOC_LITERAL(49, 16),  // "javaReadyChanged"
        QT_MOC_LITERAL(66, 21),  // "memorySettingsChanged"
        QT_MOC_LITERAL(88, 22),  // "generalSettingsChanged"
        QT_MOC_LITERAL(111, 16),  // "isolationChanged"
        QT_MOC_LITERAL(128, 10),  // "logMessage"
        QT_MOC_LITERAL(139, 3),  // "msg"
        QT_MOC_LITERAL(143, 21),  // "scanJavaInstallations"
        QT_MOC_LITERAL(165, 14),  // "autoSelectJava"
        QT_MOC_LITERAL(180, 10),  // "detectJava"
        QT_MOC_LITERAL(191, 15),  // "getMemoryStatus"
        QT_MOC_LITERAL(207, 12),  // "setMinMemory"
        QT_MOC_LITERAL(220, 2),  // "mb"
        QT_MOC_LITERAL(223, 12),  // "setMaxMemory"
        QT_MOC_LITERAL(236, 17),  // "availableJavaList"
        QT_MOC_LITERAL(254, 17),  // "selectJavaByIndex"
        QT_MOC_LITERAL(272, 5),  // "index"
        QT_MOC_LITERAL(278, 18),  // "openJavaFileDialog"
        QT_MOC_LITERAL(297, 10),  // "browseJava"
        QT_MOC_LITERAL(308, 19),  // "setIsolationEnabled"
        QT_MOC_LITERAL(328, 7),  // "enabled"
        QT_MOC_LITERAL(336, 24),  // "migrateVersionToIsolated"
        QT_MOC_LITERAL(361, 9),  // "versionId"
        QT_MOC_LITERAL(371, 17),  // "getVersionGameDir"
        QT_MOC_LITERAL(389, 16),  // "isolationEnabled"
        QT_MOC_LITERAL(406, 11),  // "openGameDir"
        QT_MOC_LITERAL(418, 14),  // "openVersionDir"
        QT_MOC_LITERAL(433, 13),  // "deleteVersion"
        QT_MOC_LITERAL(447, 8),  // "openPath"
        QT_MOC_LITERAL(456, 4),  // "path"
        QT_MOC_LITERAL(461, 19),  // "setCloseAfterLaunch"
        QT_MOC_LITERAL(481, 8),  // "javaPath"
        QT_MOC_LITERAL(490, 11),  // "javaVersion"
        QT_MOC_LITERAL(502, 9),  // "javaMajor"
        QT_MOC_LITERAL(512, 11),  // "minMemoryMB"
        QT_MOC_LITERAL(524, 11),  // "maxMemoryMB"
        QT_MOC_LITERAL(536, 16),  // "closeAfterLaunch"
        QT_MOC_LITERAL(553, 9)   // "javaReady"
    },
    "ShadowLauncher::SettingsBackend",
    "javaPathChanged",
    "",
    "javaReadyChanged",
    "memorySettingsChanged",
    "generalSettingsChanged",
    "isolationChanged",
    "logMessage",
    "msg",
    "scanJavaInstallations",
    "autoSelectJava",
    "detectJava",
    "getMemoryStatus",
    "setMinMemory",
    "mb",
    "setMaxMemory",
    "availableJavaList",
    "selectJavaByIndex",
    "index",
    "openJavaFileDialog",
    "browseJava",
    "setIsolationEnabled",
    "enabled",
    "migrateVersionToIsolated",
    "versionId",
    "getVersionGameDir",
    "isolationEnabled",
    "openGameDir",
    "openVersionDir",
    "deleteVersion",
    "openPath",
    "path",
    "setCloseAfterLaunch",
    "javaPath",
    "javaVersion",
    "javaMajor",
    "minMemoryMB",
    "maxMemoryMB",
    "closeAfterLaunch",
    "javaReady"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSShadowLauncherSCOPESettingsBackendENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
      25,   14, // methods
       8,  211, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       6,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  164,    2, 0x06,    9 /* Public */,
       3,    0,  165,    2, 0x06,   10 /* Public */,
       4,    0,  166,    2, 0x06,   11 /* Public */,
       5,    0,  167,    2, 0x06,   12 /* Public */,
       6,    0,  168,    2, 0x06,   13 /* Public */,
       7,    1,  169,    2, 0x06,   14 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
       9,    0,  172,    2, 0x02,   16 /* Public */,
      10,    0,  173,    2, 0x02,   17 /* Public */,
      11,    0,  174,    2, 0x02,   18 /* Public */,
      12,    0,  175,    2, 0x02,   19 /* Public */,
      13,    1,  176,    2, 0x02,   20 /* Public */,
      15,    1,  179,    2, 0x02,   22 /* Public */,
      16,    0,  182,    2, 0x02,   24 /* Public */,
      17,    1,  183,    2, 0x02,   25 /* Public */,
      19,    0,  186,    2, 0x02,   27 /* Public */,
      20,    0,  187,    2, 0x02,   28 /* Public */,
      21,    1,  188,    2, 0x02,   29 /* Public */,
      23,    1,  191,    2, 0x02,   31 /* Public */,
      25,    1,  194,    2, 0x102,   33 /* Public | MethodIsConst  */,
      26,    0,  197,    2, 0x102,   35 /* Public | MethodIsConst  */,
      27,    0,  198,    2, 0x02,   36 /* Public */,
      28,    1,  199,    2, 0x02,   37 /* Public */,
      29,    1,  202,    2, 0x02,   39 /* Public */,
      30,    1,  205,    2, 0x02,   41 /* Public */,
      32,    1,  208,    2, 0x02,   43 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    8,

 // methods: parameters
    QMetaType::QVariantList,
    QMetaType::QString,
    QMetaType::QString,
    QMetaType::QVariantMap,
    QMetaType::Void, QMetaType::Int,   14,
    QMetaType::Void, QMetaType::Int,   14,
    QMetaType::QVariantList,
    QMetaType::Void, QMetaType::Int,   18,
    QMetaType::QString,
    QMetaType::QString,
    QMetaType::Void, QMetaType::Bool,   22,
    QMetaType::Void, QMetaType::QString,   24,
    QMetaType::QString, QMetaType::QString,   24,
    QMetaType::Bool,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   24,
    QMetaType::Void, QMetaType::QString,   24,
    QMetaType::Void, QMetaType::QString,   31,
    QMetaType::Void, QMetaType::Bool,   22,

 // properties: name, type, flags
      33, QMetaType::QString, 0x00015103, uint(0), 0,
      34, QMetaType::QString, 0x00015001, uint(0), 0,
      35, QMetaType::Int, 0x00015001, uint(0), 0,
      36, QMetaType::Int, 0x00015001, uint(2), 0,
      37, QMetaType::Int, 0x00015001, uint(2), 0,
      38, QMetaType::Bool, 0x00015001, uint(3), 0,
      39, QMetaType::Bool, 0x00015001, uint(0), 0,
      26, QMetaType::Bool, 0x00015001, uint(4), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::SettingsBackend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSShadowLauncherSCOPESettingsBackendENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSShadowLauncherSCOPESettingsBackendENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSShadowLauncherSCOPESettingsBackendENDCLASS_t,
        // property 'javaPath'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'javaVersion'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'javaMajor'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'minMemoryMB'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'maxMemoryMB'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'closeAfterLaunch'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'javaReady'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'isolationEnabled'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<SettingsBackend, std::true_type>,
        // method 'javaPathChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'javaReadyChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'memorySettingsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'generalSettingsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'isolationChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'logMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'scanJavaInstallations'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        // method 'autoSelectJava'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'detectJava'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'getMemoryStatus'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::false_type>,
        // method 'setMinMemory'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setMaxMemory'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'availableJavaList'
        QtPrivate::TypeAndForceComplete<QVariantList, std::false_type>,
        // method 'selectJavaByIndex'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'openJavaFileDialog'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'browseJava'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'setIsolationEnabled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'migrateVersionToIsolated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'getVersionGameDir'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'isolationEnabled'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'openGameDir'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openVersionDir'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'deleteVersion'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'openPath'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setCloseAfterLaunch'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::SettingsBackend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<SettingsBackend *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->javaPathChanged(); break;
        case 1: _t->javaReadyChanged(); break;
        case 2: _t->memorySettingsChanged(); break;
        case 3: _t->generalSettingsChanged(); break;
        case 4: _t->isolationChanged(); break;
        case 5: _t->logMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: { QVariantList _r = _t->scanJavaInstallations();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 7: { QString _r = _t->autoSelectJava();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 8: { QString _r = _t->detectJava();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 9: { QVariantMap _r = _t->getMemoryStatus();
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 10: _t->setMinMemory((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 11: _t->setMaxMemory((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 12: { QVariantList _r = _t->availableJavaList();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 13: _t->selectJavaByIndex((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 14: { QString _r = _t->openJavaFileDialog();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 15: { QString _r = _t->browseJava();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 16: _t->setIsolationEnabled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 17: _t->migrateVersionToIsolated((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 18: { QString _r = _t->getVersionGameDir((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 19: { bool _r = _t->isolationEnabled();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 20: _t->openGameDir(); break;
        case 21: _t->openVersionDir((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 22: _t->deleteVersion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 23: _t->openPath((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 24: _t->setCloseAfterLaunch((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (SettingsBackend::*)();
            if (_t _q_method = &SettingsBackend::javaPathChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (SettingsBackend::*)();
            if (_t _q_method = &SettingsBackend::javaReadyChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (SettingsBackend::*)();
            if (_t _q_method = &SettingsBackend::memorySettingsChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (SettingsBackend::*)();
            if (_t _q_method = &SettingsBackend::generalSettingsChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (SettingsBackend::*)();
            if (_t _q_method = &SettingsBackend::isolationChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (SettingsBackend::*)(const QString & );
            if (_t _q_method = &SettingsBackend::logMessage; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
    }else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<SettingsBackend *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->javaPath(); break;
        case 1: *reinterpret_cast< QString*>(_v) = _t->javaVersion(); break;
        case 2: *reinterpret_cast< int*>(_v) = _t->javaMajor(); break;
        case 3: *reinterpret_cast< int*>(_v) = _t->minMemoryMB(); break;
        case 4: *reinterpret_cast< int*>(_v) = _t->maxMemoryMB(); break;
        case 5: *reinterpret_cast< bool*>(_v) = _t->closeAfterLaunch(); break;
        case 6: *reinterpret_cast< bool*>(_v) = _t->isJavaReady(); break;
        case 7: *reinterpret_cast< bool*>(_v) = _t->isolationEnabled(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
        auto *_t = static_cast<SettingsBackend *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setJavaPath(*reinterpret_cast< QString*>(_v)); break;
        default: break;
        }
    } else if (_c == QMetaObject::ResetProperty) {
    } else if (_c == QMetaObject::BindableProperty) {
    }
}

const QMetaObject *ShadowLauncher::SettingsBackend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::SettingsBackend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSShadowLauncherSCOPESettingsBackendENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::SettingsBackend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 25)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 25;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 25)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 25;
    }else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void ShadowLauncher::SettingsBackend::javaPathChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ShadowLauncher::SettingsBackend::javaReadyChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ShadowLauncher::SettingsBackend::memorySettingsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void ShadowLauncher::SettingsBackend::generalSettingsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void ShadowLauncher::SettingsBackend::isolationChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void ShadowLauncher::SettingsBackend::logMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}
QT_WARNING_POP
