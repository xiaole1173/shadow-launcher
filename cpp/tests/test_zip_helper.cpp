// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// test_zip_helper.cpp — 自测专用：用 miniz 写 ZIP（构造测试整合包）。
// 仅参与 ModpackSelfTest 目标，不进入主程序。

#include <QByteArray>
#include <QFile>
#include <QList>
#include <QPair>
#include <QString>

#include "miniz.h"

#ifdef _WIN32
#include <windows.h>
#endif

// 构造 zip：indexEntryName 条目 + 若干 extra 文件（键为条目路径）。
// 成功返回 0；indexEntryName 为空则只写 extra 文件。
int buildTestZip(const QString& zipPath, const QString& indexEntryName,
                 const QByteArray& indexContent,
                 const QList<QPair<QString, QByteArray>>& extraFiles)
{
    FILE* f = nullptr;
#ifdef _WIN32
    f = _wfopen(zipPath.toStdWString().c_str(), L"wb");
#else
    f = fopen(QFile::encodeName(zipPath).constData(), "wb");
#endif
    if (!f) return -1;

    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));

    if (!mz_zip_writer_init_cfile(&zip, f, 0)) {
        fclose(f);
        return -1;
    }

    bool ok = true;
    if (!indexEntryName.isEmpty()) {
        ok = mz_zip_writer_add_mem(&zip, indexEntryName.toUtf8().constData(),
                                   indexContent.constData(), indexContent.size(),
                                   MZ_DEFAULT_COMPRESSION) != 0;
    }
    for (const auto& e : extraFiles) {
        if (!ok) break;
        ok = mz_zip_writer_add_mem(&zip, e.first.toUtf8().constData(),
                                   e.second.constData(), e.second.size(),
                                   MZ_DEFAULT_COMPRESSION) != 0;
    }

    mz_zip_writer_finalize_archive(&zip);
    mz_zip_writer_end(&zip);
    fclose(f);
    return ok ? 0 : -1;
}
