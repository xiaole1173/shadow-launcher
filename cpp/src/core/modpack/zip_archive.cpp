// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173

#include "zip_archive.h"

#include "modpack_common.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>

#include "miniz.h"
#include "../../utils/logger.h"

#include <cstring>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace ShadowLauncher {

// miniz 结构体（匿名 typedef，只能在 cpp 内定义）
struct ZipArchiveData {
    mz_zip_archive zip;
    FILE* file = nullptr;
};

namespace {

// Windows 下用宽字符 fopen 打开 zip，避免中文路径在 ANSI 代码页下打不开
// （旧实现用 PowerShell 也是这个原因绕不过去，导致解压失效）。
bool openZipFileHandle(const QString& path, FILE** out)
{
#ifdef Q_OS_WIN
    const std::wstring wpath = path.toStdWString();
    *out = _wfopen(wpath.c_str(), L"rb");
    return *out != nullptr;
#else
    *out = fopen(QFile::encodeName(path).constData(), "rb");
    return *out != nullptr;
#endif
}

} // namespace

ZipArchive::~ZipArchive()
{
    close();
}

// ── 打开 / 关闭 ──

bool ZipArchive::open(const QString& zipPath)
{
    close();
    m_error.clear();

    FILE* f = nullptr;
    if (!openZipFileHandle(zipPath, &f)) {
        m_error = QStringLiteral("无法打开压缩包文件: %1").arg(zipPath);
        qCWarning(logMod) << "[zip]" << m_error;
        return false;
    }

    auto* data = new ZipArchiveData;
    std::memset(&data->zip, 0, sizeof(data->zip));
    data->file = f;

    if (!mz_zip_reader_init_cfile(&data->zip, f, 0, 0)) {
        m_error = QStringLiteral("压缩包解析失败（文件损坏或非 ZIP 格式）: %1").arg(zipPath);
        qCWarning(logMod) << "[zip]" << m_error;
        fclose(f);
        delete data;
        return false;
    }

    m_data = data;
    m_open = true;
    m_path = zipPath;
    return true;
}

void ZipArchive::close()
{
    if (m_data) {
        mz_zip_reader_end(&m_data->zip);
        if (m_data->file)
            fclose(m_data->file);
        delete m_data;
        m_data = nullptr;
    }
    m_open = false;
    m_path.clear();
}

int ZipArchive::entryCount() const
{
    if (!m_open || !m_data) return 0;
    return static_cast<int>(mz_zip_reader_get_num_files(&m_data->zip));
}

bool ZipArchive::hasEntry(const QString& name) const
{
    if (!m_open || !m_data) return false;
    return mz_zip_reader_locate_file(&m_data->zip, name.toUtf8().constData(), nullptr, 0) >= 0;
}

QByteArray ZipArchive::readEntry(const QString& name, qint64 maxBytes) const
{
    if (!m_open || !m_data) return {};
    const int idx = mz_zip_reader_locate_file(&m_data->zip, name.toUtf8().constData(), nullptr, 0);
    if (idx < 0) return {};

    mz_zip_archive_file_stat st;
    if (!mz_zip_reader_file_stat(&m_data->zip, static_cast<mz_uint>(idx), &st)) return {};
    if (st.m_is_directory) return {};

    if (static_cast<qint64>(st.m_uncomp_size) > maxBytes) {
        qCWarning(logMod) << "[zip] 条目过大，拒绝读取:" << name
                          << static_cast<qint64>(st.m_uncomp_size) << "bytes >" << maxBytes;
        return {};
    }

    QByteArray out;
    out.resize(static_cast<int>(st.m_uncomp_size));
    mz_zip_reader_extract_iter_state* iter =
        mz_zip_reader_extract_iter_new(&m_data->zip, static_cast<mz_uint>(idx), 0);
    if (!iter) return {};

    size_t got = mz_zip_reader_extract_iter_read(iter, out.data(), out.size());
    mz_zip_reader_extract_iter_free(iter);
    if (got != out.size()) {
        qCWarning(logMod) << "[zip] 条目读取不完整:" << name << got << "/" << out.size();
        return {};
    }
    return out;
}

// ── 解压 ──

int ZipArchive::extractPrefixTo(const QString& prefix, const QString& destDir,
                                const std::atomic<bool>* cancelFlag,
                                const std::function<void(int, int)>& progress,
                                const std::function<void(const QString&, bool)>& onBeforeWrite)
{
    if (!m_open || !m_data) {
        qCWarning(logMod) << "[zip] extractPrefixTo: 未打开压缩包";
        return -1;
    }

    QString p = prefix;
    p.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (p.endsWith(QLatin1Char('/')) && p.size() > 1)
        p.chop(1);
    const bool extractAll = p.isEmpty();

    // 预扫描目标条目数（central directory，开销可忽略）
    struct Target { mz_uint index; QString rel; };
    QList<Target> targets;
    const mz_uint count = mz_zip_reader_get_num_files(&m_data->zip);
    for (mz_uint i = 0; i < count; ++i) {
        if (cancelFlag && cancelFlag->load()) return -2;
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&m_data->zip, i, &st)) continue;

        QString name = QString::fromUtf8(st.m_filename);
        name.replace(QLatin1Char('\\'), QLatin1Char('/'));

        if (st.m_is_directory) {
            const QString safeDir = sanitizeRelPath(name);
            if (!safeDir.isEmpty())
                targets.append({i, safeDir + QLatin1Char('/')});
            continue;
        }

        QString rel;
        if (extractAll) {
            rel = name;
        } else {
            if (!name.startsWith(p + QLatin1Char('/')))
                continue;
            rel = name.mid(p.size() + 1);
        }
        rel = sanitizeRelPath(rel);
        if (rel.isEmpty()) {
            qCWarning(logMod) << "[zip] 跳过非法路径条目:" << name;
            continue;
        }
        targets.append({i, rel});
    }

    const int total = targets.size();
    int done = 0;
    if (progress) progress(0, total);
    qCInfo(logMod) << "[zip] 解压开始:" << m_path << "prefix=" << (extractAll ? QStringLiteral("<all>") : p)
                   << "条目数=" << total << "→" << destDir;

    for (const Target& t : targets) {
        if (cancelFlag && cancelFlag->load()) return -2;

        const QString destPath = destDir + QLatin1Char('/') + t.rel;
        if (t.rel.endsWith(QLatin1Char('/'))) {
            QDir().mkpath(destPath);
            ++done;
            if (progress) progress(done, total);
            continue;
        }

        QFileInfo fi(destPath);
        QDir().mkpath(fi.absolutePath());

        // 写前回调：任务层在此备份被覆盖的旧文件并登记回滚信息
        if (onBeforeWrite)
            onBeforeWrite(destPath, QFileInfo::exists(destPath));

        mz_zip_reader_extract_iter_state* iter =
            mz_zip_reader_extract_iter_new(&m_data->zip, t.index, 0);
        if (!iter) {
            qCWarning(logMod) << "[zip] 解压失败（无法创建迭代器）:" << t.rel;
            return -1;
        }

        QFile outFile(destPath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qCWarning(logMod) << "[zip] 无法写入目标文件:" << destPath;
            mz_zip_reader_extract_iter_free(iter);
            return -1;
        }

        bool ok = true;
        char buf[256 * 1024];
        qint64 chunkCounter = 0;
        for (;;) {
            if (cancelFlag && cancelFlag->load()) {
                ok = false;
                break;
            }
            const size_t got = mz_zip_reader_extract_iter_read(iter, buf, sizeof(buf));
            if (got == 0) break;
            if (outFile.write(buf, static_cast<qint64>(got)) != static_cast<qint64>(got)) {
                qCWarning(logMod) << "[zip] 写入失败（磁盘空间不足？）:" << destPath;
                mz_zip_reader_extract_iter_free(iter);
                outFile.close();
                outFile.remove();
                return -3;  // 致命写盘错误（区别于取消 -2），任务层必须中止导入
            }
            chunkCounter += static_cast<qint64>(got);
            if (chunkCounter >= 4 * 1024 * 1024) {
                chunkCounter = 0;
                if (cancelFlag && cancelFlag->load()) {
                    ok = false;
                    break;
                }
            }
        }
        mz_zip_reader_extract_iter_free(iter);
        outFile.close();

        if (!ok) {
            outFile.remove();
            return -2;  // 取消
        }

        // 恢复 zip 内记录的时间戳（主流启动器 同样保留压缩包时间）
        mz_zip_archive_file_stat st;
        if (mz_zip_reader_file_stat(&m_data->zip, t.index, &st) && st.m_time) {
            const QDateTime dt = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(st.m_time), Qt::UTC);
            QFile f(destPath);
            if (f.open(QIODevice::ReadOnly))
                f.setFileTime(dt, QFileDevice::FileModificationTime);
        }

        ++done;
        if (progress) progress(done, total);
    }

    qCInfo(logMod) << "[zip] 解压完成:" << done << "/" << total << "→" << destDir;
    return done;
}

} // namespace ShadowLauncher
