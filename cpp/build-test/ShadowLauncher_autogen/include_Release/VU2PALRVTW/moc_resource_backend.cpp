/****************************************************************************
** Meta object code from reading C++ file 'resource_backend.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/backend/resource_backend.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'resource_backend.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14ShadowLauncher15ResourceBackendE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN14ShadowLauncher15ResourceBackendE = QtMocHelpers::stringData(
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
    "modFileDownloadStarted",
    "downloadId",
    "fileSize",
    "displayName",
    "modFileDownloadProgress",
    "received",
    "speed",
    "modFileDownloadFinished",
    "modFileDownloadFailed",
    "errorDetail",
    "searchResultsReady",
    "QVariantList",
    "results",
    "modSearchResultsReady",
    "shaderSearchResultsReady",
    "resourcepackSearchCompleted",
    "totalHits",
    "resourcepackSearchFailed",
    "error",
    "resourcepackDownloadFinished",
    "resourcepackVersionsLoaded",
    "QVariantMap",
    "slugToVersions",
    "resourcepackVersionsPartial",
    "versions",
    "details",
    "resourcepackVersionsProgress",
    "done",
    "modVersionsLoaded",
    "modVersionsPartial",
    "modVersionsProgress",
    "shaderVersionsLoaded",
    "shaderVersionsPartial",
    "shaderVersionsProgress",
    "logMessage",
    "msg",
    "onSearchCompleted",
    "onResourcepackSearchCompleted",
    "onResourcepackDownloadFinished",
    "onResourcepackVersionsLoaded",
    "onModVersionsLoaded",
    "onShaderVersionsLoaded",
    "onDownloadProgress",
    "name",
    "onDownloadFinished",
    "onModFileDownloadStarted",
    "onModFileDownloadProgress",
    "onModFileDownloadFinished",
    "onModFileDownloadFailed",
    "getPopularMods",
    "loader",
    "getShaderList",
    "searchMods",
    "query",
    "searchModsEx",
    "category",
    "gameVersions",
    "environment",
    "license",
    "offset",
    "limit",
    "getModCategories",
    "searchShadersEx",
    "categories",
    "performance",
    "downloadMod",
    "gameVersion",
    "minecraftDir",
    "downloadShader",
    "searchResourcepacks",
    "downloadResourcepack",
    "fetchResourcepackVersions",
    "slugs",
    "fetchModVersions",
    "fetchShaderVersions",
    "cancelDownload",
    "downloadModFile",
    "url",
    "savePath",
    "expectedSize",
    "sha1",
    "receivedOffset",
    "resumeId",
    "cancelModFileDownload",
    "pauseModFileDownload",
    "resumeModFileDownload",
    "retryModFileDownload",
    "downloading",
    "dlProgress",
    "dlTotal",
    "dlSpeed",
    "dlFile"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN14ShadowLauncher15ResourceBackendE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      63,   14, // methods
       5,  753, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      23,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    3,  392,    2, 0x06,    6 /* Public */,
       6,    0,  399,    2, 0x06,   10 /* Public */,
       7,    3,  400,    2, 0x06,   11 /* Public */,
      11,    4,  407,    2, 0x06,   15 /* Public */,
      15,    4,  416,    2, 0x06,   20 /* Public */,
      18,    4,  425,    2, 0x06,   25 /* Public */,
      19,    3,  434,    2, 0x06,   30 /* Public */,
      21,    1,  441,    2, 0x06,   34 /* Public */,
      24,    1,  444,    2, 0x06,   36 /* Public */,
      25,    1,  447,    2, 0x06,   38 /* Public */,
      26,    2,  450,    2, 0x06,   40 /* Public */,
      28,    1,  455,    2, 0x06,   43 /* Public */,
      30,    3,  458,    2, 0x06,   45 /* Public */,
      31,    1,  465,    2, 0x06,   49 /* Public */,
      34,    3,  468,    2, 0x06,   51 /* Public */,
      37,    2,  475,    2, 0x06,   55 /* Public */,
      39,    1,  480,    2, 0x06,   58 /* Public */,
      40,    3,  483,    2, 0x06,   60 /* Public */,
      41,    2,  490,    2, 0x06,   64 /* Public */,
      42,    1,  495,    2, 0x06,   67 /* Public */,
      43,    3,  498,    2, 0x06,   69 /* Public */,
      44,    2,  505,    2, 0x06,   73 /* Public */,
      45,    1,  510,    2, 0x06,   76 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      47,    2,  513,    2, 0x08,   78 /* Private */,
      48,    2,  518,    2, 0x08,   81 /* Private */,
      49,    3,  523,    2, 0x08,   84 /* Private */,
      50,    1,  530,    2, 0x08,   88 /* Private */,
      51,    1,  533,    2, 0x08,   90 /* Private */,
      52,    1,  536,    2, 0x08,   92 /* Private */,
      53,    3,  539,    2, 0x08,   94 /* Private */,
      55,    3,  546,    2, 0x08,   98 /* Private */,
      56,    4,  553,    2, 0x08,  102 /* Private */,
      57,    4,  562,    2, 0x08,  107 /* Private */,
      58,    4,  571,    2, 0x08,  112 /* Private */,
      59,    3,  580,    2, 0x08,  117 /* Private */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      60,    1,  587,    2, 0x02,  121 /* Public */,
      62,    0,  590,    2, 0x02,  123 /* Public */,
      63,    2,  591,    2, 0x02,  124 /* Public */,
      63,    1,  596,    2, 0x22,  127 /* Public | MethodCloned */,
      65,    8,  599,    2, 0x02,  129 /* Public */,
      72,    0,  616,    2, 0x02,  138 /* Public */,
      73,    7,  617,    2, 0x02,  139 /* Public */,
      76,    3,  632,    2, 0x02,  147 /* Public */,
      76,    2,  639,    2, 0x22,  151 /* Public | MethodCloned */,
      79,    3,  644,    2, 0x02,  154 /* Public */,
      79,    2,  651,    2, 0x22,  158 /* Public | MethodCloned */,
      80,    4,  656,    2, 0x02,  161 /* Public */,
      80,    3,  665,    2, 0x22,  166 /* Public | MethodCloned */,
      80,    2,  672,    2, 0x22,  170 /* Public | MethodCloned */,
      80,    1,  677,    2, 0x22,  173 /* Public | MethodCloned */,
      81,    3,  680,    2, 0x02,  175 /* Public */,
      81,    2,  687,    2, 0x22,  179 /* Public | MethodCloned */,
      82,    1,  692,    2, 0x02,  182 /* Public */,
      84,    1,  695,    2, 0x02,  184 /* Public */,
      85,    1,  698,    2, 0x02,  186 /* Public */,
      86,    0,  701,    2, 0x02,  188 /* Public */,
      87,    7,  702,    2, 0x02,  189 /* Public */,
      87,    6,  717,    2, 0x22,  197 /* Public | MethodCloned */,
      87,    5,  730,    2, 0x22,  204 /* Public | MethodCloned */,
      94,    1,  741,    2, 0x02,  210 /* Public */,
      95,    1,  744,    2, 0x02,  212 /* Public */,
      96,    1,  747,    2, 0x02,  214 /* Public */,
      97,    1,  750,    2, 0x02,  216 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::QString,    3,    4,    5,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::Bool, QMetaType::QString,    8,    9,   10,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::LongLong, QMetaType::QString,   12,    5,   13,   14,
    QMetaType::Void, QMetaType::Int, QMetaType::LongLong, QMetaType::LongLong, QMetaType::LongLong,   12,   16,    4,   17,
    QMetaType::Void, QMetaType::Int, QMetaType::Bool, QMetaType::QString, QMetaType::QString,   12,    9,   10,   14,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::QString,   12,   20,   14,
    QMetaType::Void, 0x80000000 | 22,   23,
    QMetaType::Void, 0x80000000 | 22,   23,
    QMetaType::Void, 0x80000000 | 22,   23,
    QMetaType::Void, 0x80000000 | 22, QMetaType::Int,   23,   27,
    QMetaType::Void, QMetaType::QString,   29,
    QMetaType::Void, QMetaType::QString, QMetaType::Bool, QMetaType::QString,    8,    9,   10,
    QMetaType::Void, 0x80000000 | 32,   33,
    QMetaType::Void, QMetaType::QString, QMetaType::QStringList, 0x80000000 | 32,    8,   35,   36,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   38,    4,
    QMetaType::Void, 0x80000000 | 32,   33,
    QMetaType::Void, QMetaType::QString, QMetaType::QStringList, 0x80000000 | 32,    8,   35,   36,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   38,    4,
    QMetaType::Void, 0x80000000 | 32,   33,
    QMetaType::Void, QMetaType::QString, QMetaType::QStringList, 0x80000000 | 32,    8,   35,   36,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   38,    4,
    QMetaType::Void, QMetaType::QString,   46,

 // slots: parameters
    QMetaType::Void, QMetaType::QJsonArray, QMetaType::Int,   23,   27,
    QMetaType::Void, QMetaType::QJsonArray, QMetaType::Int,   23,   27,
    QMetaType::Void, QMetaType::QString, QMetaType::Bool, QMetaType::QString,    8,    9,   10,
    QMetaType::Void, 0x80000000 | 32,   33,
    QMetaType::Void, 0x80000000 | 32,   33,
    QMetaType::Void, 0x80000000 | 32,   33,
    QMetaType::Void, QMetaType::QString, QMetaType::LongLong, QMetaType::LongLong,   54,   16,    4,
    QMetaType::Void, QMetaType::QString, QMetaType::Bool, QMetaType::QString,    8,    9,   10,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::LongLong, QMetaType::QString,   12,    5,   13,   14,
    QMetaType::Void, QMetaType::Int, QMetaType::LongLong, QMetaType::LongLong, QMetaType::LongLong,   12,   16,    4,   17,
    QMetaType::Void, QMetaType::Int, QMetaType::Bool, QMetaType::QString, QMetaType::QString,   12,    9,   10,   14,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::QString,   12,   20,   14,

 // methods: parameters
    0x80000000 | 22, QMetaType::QString,   61,
    0x80000000 | 22,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   64,   61,
    QMetaType::Void, QMetaType::QString,   64,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QStringList, QMetaType::QString, QMetaType::QString, QMetaType::Int, QMetaType::Int,   64,   61,   66,   67,   68,   69,   70,   71,
    0x80000000 | 32,
    QMetaType::Void, QMetaType::QString, QMetaType::QStringList, QMetaType::QStringList, QMetaType::QStringList, QMetaType::QStringList, QMetaType::Int, QMetaType::Int,   64,   67,   74,   75,   61,   70,   71,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString,    8,   77,   78,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,    8,   77,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString,    8,   77,   78,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,    8,   77,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::Int, QMetaType::QStringList,   64,   77,   70,   74,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::Int,   64,   77,   70,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   64,   77,
    QMetaType::Void, QMetaType::QString,   64,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString,    8,   77,   78,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,    8,   77,
    QMetaType::Void, QMetaType::QStringList,   83,
    QMetaType::Void, QMetaType::QStringList,   83,
    QMetaType::Void, QMetaType::QStringList,   83,
    QMetaType::Void,
    QMetaType::Int, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::LongLong, QMetaType::QString, QMetaType::LongLong, QMetaType::Int,   88,   89,   14,   90,   91,   92,   93,
    QMetaType::Int, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::LongLong, QMetaType::QString, QMetaType::LongLong,   88,   89,   14,   90,   91,   92,
    QMetaType::Int, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::LongLong, QMetaType::QString,   88,   89,   14,   90,   91,
    QMetaType::Void, QMetaType::Int,   12,
    QMetaType::Void, QMetaType::Int,   12,
    QMetaType::Void, QMetaType::Int,   12,
    QMetaType::Void, QMetaType::Int,   12,

 // properties: name, type, flags, notifyId, revision
      98, QMetaType::Bool, 0x00015001, uint(1), 0,
      99, QMetaType::Int, 0x00015001, uint(0), 0,
     100, QMetaType::Int, 0x00015001, uint(0), 0,
     101, QMetaType::Int, 0x00015001, uint(0), 0,
     102, QMetaType::QString, 0x00015001, uint(0), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ShadowLauncher::ResourceBackend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN14ShadowLauncher15ResourceBackendE.offsetsAndSizes,
    qt_meta_data_ZN14ShadowLauncher15ResourceBackendE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN14ShadowLauncher15ResourceBackendE_t,
        // property 'downloading'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'dlProgress'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'dlTotal'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'dlSpeed'
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
        // method 'modFileDownloadStarted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'modFileDownloadProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        // method 'modFileDownloadFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'modFileDownloadFailed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'searchResultsReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantList &, std::false_type>,
        // method 'modSearchResultsReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantList &, std::false_type>,
        // method 'shaderSearchResultsReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantList &, std::false_type>,
        // method 'resourcepackSearchCompleted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'resourcepackSearchFailed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'resourcepackDownloadFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'resourcepackVersionsLoaded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'resourcepackVersionsPartial'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'resourcepackVersionsProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'modVersionsLoaded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'modVersionsPartial'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'modVersionsProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'shaderVersionsLoaded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'shaderVersionsPartial'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'shaderVersionsProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'logMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onSearchCompleted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QJsonArray &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onResourcepackSearchCompleted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QJsonArray &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onResourcepackDownloadFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onResourcepackVersionsLoaded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'onModVersionsLoaded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'onShaderVersionsLoaded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
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
        // method 'onModFileDownloadStarted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onModFileDownloadProgress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        // method 'onModFileDownloadFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onModFileDownloadFailed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
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
        // method 'searchModsEx'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'getModCategories'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::false_type>,
        // method 'searchShadersEx'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'downloadMod'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'downloadMod'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'downloadShader'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'downloadShader'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'searchResourcepacks'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        // method 'searchResourcepacks'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'searchResourcepacks'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'searchResourcepacks'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'downloadResourcepack'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'downloadResourcepack'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'fetchResourcepackVersions'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        // method 'fetchModVersions'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        // method 'fetchShaderVersions'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        // method 'cancelDownload'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'downloadModFile'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'downloadModFile'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        // method 'downloadModFile'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cancelModFileDownload'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'pauseModFileDownload'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'resumeModFileDownload'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'retryModFileDownload'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>
    >,
    nullptr
} };

void ShadowLauncher::ResourceBackend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ResourceBackend *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->downloadProgressChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 1: _t->downloadStateChanged(); break;
        case 2: _t->downloadFinished((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 3: _t->modFileDownloadStarted((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4]))); break;
        case 4: _t->modFileDownloadProgress((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[4]))); break;
        case 5: _t->modFileDownloadFinished((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4]))); break;
        case 6: _t->modFileDownloadFailed((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 7: _t->searchResultsReady((*reinterpret_cast< std::add_pointer_t<QVariantList>>(_a[1]))); break;
        case 8: _t->modSearchResultsReady((*reinterpret_cast< std::add_pointer_t<QVariantList>>(_a[1]))); break;
        case 9: _t->shaderSearchResultsReady((*reinterpret_cast< std::add_pointer_t<QVariantList>>(_a[1]))); break;
        case 10: _t->resourcepackSearchCompleted((*reinterpret_cast< std::add_pointer_t<QVariantList>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 11: _t->resourcepackSearchFailed((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 12: _t->resourcepackDownloadFinished((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 13: _t->resourcepackVersionsLoaded((*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 14: _t->resourcepackVersionsPartial((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[3]))); break;
        case 15: _t->resourcepackVersionsProgress((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 16: _t->modVersionsLoaded((*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 17: _t->modVersionsPartial((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[3]))); break;
        case 18: _t->modVersionsProgress((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 19: _t->shaderVersionsLoaded((*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 20: _t->shaderVersionsPartial((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[3]))); break;
        case 21: _t->shaderVersionsProgress((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 22: _t->logMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 23: _t->onSearchCompleted((*reinterpret_cast< std::add_pointer_t<QJsonArray>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 24: _t->onResourcepackSearchCompleted((*reinterpret_cast< std::add_pointer_t<QJsonArray>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 25: _t->onResourcepackDownloadFinished((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 26: _t->onResourcepackVersionsLoaded((*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 27: _t->onModVersionsLoaded((*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 28: _t->onShaderVersionsLoaded((*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 29: _t->onDownloadProgress((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[3]))); break;
        case 30: _t->onDownloadFinished((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 31: _t->onModFileDownloadStarted((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4]))); break;
        case 32: _t->onModFileDownloadProgress((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[4]))); break;
        case 33: _t->onModFileDownloadFinished((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4]))); break;
        case 34: _t->onModFileDownloadFailed((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 35: { QVariantList _r = _t->getPopularMods((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 36: { QVariantList _r = _t->getShaderList();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 37: _t->searchMods((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 38: _t->searchMods((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 39: _t->searchModsEx((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[6])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[7])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[8]))); break;
        case 40: { QVariantMap _r = _t->getModCategories();
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 41: _t->searchShadersEx((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[6])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[7]))); break;
        case 42: _t->downloadMod((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 43: _t->downloadMod((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 44: _t->downloadShader((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 45: _t->downloadShader((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 46: _t->searchResourcepacks((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[4]))); break;
        case 47: _t->searchResourcepacks((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 48: _t->searchResourcepacks((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 49: _t->searchResourcepacks((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 50: _t->downloadResourcepack((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 51: _t->downloadResourcepack((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 52: _t->fetchResourcepackVersions((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 53: _t->fetchModVersions((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 54: _t->fetchShaderVersions((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 55: _t->cancelDownload(); break;
        case 56: { int _r = _t->downloadModFile((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[6])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[7])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 57: { int _r = _t->downloadModFile((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[6])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 58: { int _r = _t->downloadModFile((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<qint64>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 59: _t->cancelModFileDownload((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 60: _t->pauseModFileDownload((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 61: _t->resumeModFileDownload((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 62: _t->retryModFileDownload((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (ResourceBackend::*)(int , int , const QString & );
            if (_q_method_type _q_method = &ResourceBackend::downloadProgressChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)();
            if (_q_method_type _q_method = &ResourceBackend::downloadStateChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(const QString & , bool , const QString & );
            if (_q_method_type _q_method = &ResourceBackend::downloadFinished; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(int , const QString & , qint64 , const QString & );
            if (_q_method_type _q_method = &ResourceBackend::modFileDownloadStarted; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(int , qint64 , qint64 , qint64 );
            if (_q_method_type _q_method = &ResourceBackend::modFileDownloadProgress; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(int , bool , const QString & , const QString & );
            if (_q_method_type _q_method = &ResourceBackend::modFileDownloadFinished; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(int , const QString & , const QString & );
            if (_q_method_type _q_method = &ResourceBackend::modFileDownloadFailed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(const QVariantList & );
            if (_q_method_type _q_method = &ResourceBackend::searchResultsReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(const QVariantList & );
            if (_q_method_type _q_method = &ResourceBackend::modSearchResultsReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 8;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(const QVariantList & );
            if (_q_method_type _q_method = &ResourceBackend::shaderSearchResultsReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 9;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(const QVariantList & , int );
            if (_q_method_type _q_method = &ResourceBackend::resourcepackSearchCompleted; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 10;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(const QString & );
            if (_q_method_type _q_method = &ResourceBackend::resourcepackSearchFailed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 11;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(const QString & , bool , const QString & );
            if (_q_method_type _q_method = &ResourceBackend::resourcepackDownloadFinished; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 12;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(const QVariantMap & );
            if (_q_method_type _q_method = &ResourceBackend::resourcepackVersionsLoaded; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 13;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(const QString & , const QStringList & , const QVariantMap & );
            if (_q_method_type _q_method = &ResourceBackend::resourcepackVersionsPartial; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 14;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(int , int );
            if (_q_method_type _q_method = &ResourceBackend::resourcepackVersionsProgress; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 15;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(const QVariantMap & );
            if (_q_method_type _q_method = &ResourceBackend::modVersionsLoaded; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 16;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(const QString & , const QStringList & , const QVariantMap & );
            if (_q_method_type _q_method = &ResourceBackend::modVersionsPartial; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 17;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(int , int );
            if (_q_method_type _q_method = &ResourceBackend::modVersionsProgress; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 18;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(const QVariantMap & );
            if (_q_method_type _q_method = &ResourceBackend::shaderVersionsLoaded; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 19;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(const QString & , const QStringList & , const QVariantMap & );
            if (_q_method_type _q_method = &ResourceBackend::shaderVersionsPartial; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 20;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(int , int );
            if (_q_method_type _q_method = &ResourceBackend::shaderVersionsProgress; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 21;
                return;
            }
        }
        {
            using _q_method_type = void (ResourceBackend::*)(const QString & );
            if (_q_method_type _q_method = &ResourceBackend::logMessage; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 22;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->isDownloading(); break;
        case 1: *reinterpret_cast< int*>(_v) = _t->dlProgress(); break;
        case 2: *reinterpret_cast< int*>(_v) = _t->dlTotal(); break;
        case 3: *reinterpret_cast< int*>(_v) = _t->dlSpeed(); break;
        case 4: *reinterpret_cast< QString*>(_v) = _t->dlFile(); break;
        default: break;
        }
    }
}

const QMetaObject *ShadowLauncher::ResourceBackend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShadowLauncher::ResourceBackend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN14ShadowLauncher15ResourceBackendE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShadowLauncher::ResourceBackend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 63)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 63;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 63)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 63;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
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
void ShadowLauncher::ResourceBackend::modFileDownloadStarted(int _t1, const QString & _t2, qint64 _t3, const QString & _t4)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void ShadowLauncher::ResourceBackend::modFileDownloadProgress(int _t1, qint64 _t2, qint64 _t3, qint64 _t4)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void ShadowLauncher::ResourceBackend::modFileDownloadFinished(int _t1, bool _t2, const QString & _t3, const QString & _t4)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void ShadowLauncher::ResourceBackend::modFileDownloadFailed(int _t1, const QString & _t2, const QString & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void ShadowLauncher::ResourceBackend::searchResultsReady(const QVariantList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 8
void ShadowLauncher::ResourceBackend::modSearchResultsReady(const QVariantList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 8, _a);
}

// SIGNAL 9
void ShadowLauncher::ResourceBackend::shaderSearchResultsReady(const QVariantList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 9, _a);
}

// SIGNAL 10
void ShadowLauncher::ResourceBackend::resourcepackSearchCompleted(const QVariantList & _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 10, _a);
}

// SIGNAL 11
void ShadowLauncher::ResourceBackend::resourcepackSearchFailed(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 11, _a);
}

// SIGNAL 12
void ShadowLauncher::ResourceBackend::resourcepackDownloadFinished(const QString & _t1, bool _t2, const QString & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 12, _a);
}

// SIGNAL 13
void ShadowLauncher::ResourceBackend::resourcepackVersionsLoaded(const QVariantMap & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 13, _a);
}

// SIGNAL 14
void ShadowLauncher::ResourceBackend::resourcepackVersionsPartial(const QString & _t1, const QStringList & _t2, const QVariantMap & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 14, _a);
}

// SIGNAL 15
void ShadowLauncher::ResourceBackend::resourcepackVersionsProgress(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 15, _a);
}

// SIGNAL 16
void ShadowLauncher::ResourceBackend::modVersionsLoaded(const QVariantMap & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 16, _a);
}

// SIGNAL 17
void ShadowLauncher::ResourceBackend::modVersionsPartial(const QString & _t1, const QStringList & _t2, const QVariantMap & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 17, _a);
}

// SIGNAL 18
void ShadowLauncher::ResourceBackend::modVersionsProgress(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 18, _a);
}

// SIGNAL 19
void ShadowLauncher::ResourceBackend::shaderVersionsLoaded(const QVariantMap & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 19, _a);
}

// SIGNAL 20
void ShadowLauncher::ResourceBackend::shaderVersionsPartial(const QString & _t1, const QStringList & _t2, const QVariantMap & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 20, _a);
}

// SIGNAL 21
void ShadowLauncher::ResourceBackend::shaderVersionsProgress(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 21, _a);
}

// SIGNAL 22
void ShadowLauncher::ResourceBackend::logMessage(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 22, _a);
}
QT_WARNING_POP
