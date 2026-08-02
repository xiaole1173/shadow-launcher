// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// zip_archive.h — 基于 miniz 的 ZIP 读取封装（文件工具层）。
//
// 背景：旧实现用 PowerShell 调 System.IO.Compression 解压，速度慢、依赖
// 系统环境、失败无日志。此处改用 miniz（public domain 单文件库，官方
// amalgamated 发布版，MIT/Unlicense），流式解压，支持取消与进度回调，
// 含路径穿越防护（主流启动器 对 Modrinth 路径同样做越界校验）。
//
// 线程约定：所有方法仅供单个工作线程串行调用（miniz 内部持 FILE* 状态）。

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <functional>

#include <atomic>

namespace ShadowLauncher {

// pimpl：避免在头文件暴露 miniz 实现细节（miniz 结构体为匿名 typedef，无法前置声明）
struct ZipArchiveData;

class ZipArchive {
public:
    ZipArchive() = default;
    ~ZipArchive();

    ZipArchive(const ZipArchive&) = delete;
    ZipArchive& operator=(const ZipArchive&) = delete;

    // 打开 zip（mz_zip_reader_init_file）。失败返回 false 并记录 error。
    bool open(const QString& zipPath);
    void close();
    bool isOpen() const { return m_open; }

    QString error() const { return m_error; }

    int entryCount() const;
    bool hasEntry(const QString& name) const;

    // 读取单个条目到内存（maxBytes 限制防止恶意超大条目打爆内存）。
    // 找不到返回空 QByteArray（与空文件区分：isEmpty 均可，由调用方 hasEntry 预判）。
    QByteArray readEntry(const QString& name, qint64 maxBytes = 64 * 1024 * 1024) const;

    // 把 zip 中 prefix 前缀下的所有文件解压到 destDir，落盘路径剥离 prefix。
    //   prefix 为空字符串 => 解压整包（主流启动器 CF 包 overrides="." 场景）。
    //   目录条目自动创建，文件时间戳按 zip 记录恢复。
    // onBeforeWrite(destPath, existed)：写入前回调（任务层用于覆盖备份/回滚追踪）。
    // 返回成功解压的文件数；发生致命错误返回 -1；被取消返回 -2。
    // cancelFlag 非空时每解压一个文件检查一次。
    // progress(filesDone, filesTotal) 可选进度回调。
    int extractPrefixTo(const QString& prefix, const QString& destDir,
                        const std::atomic<bool>* cancelFlag = nullptr,
                        const std::function<void(int, int)>& progress = nullptr,
                        const std::function<void(const QString& destPath, bool existed)>& onBeforeWrite = nullptr);

    // 列出 prefix 前缀下的文件条目（不含目录条目），供解析层判断 overrides 是否存在。

private:
    // 路径安全由 modpack_common 的共享工具 sanitizeRelPath 承担（解压层/解析层同一套规则）
    ZipArchiveData* m_data = nullptr;  // 惰性分配
    bool m_open = false;
    QString m_path;
    QString m_error;
};

} // namespace ShadowLauncher
