// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace ShadowLauncher {

/// 一键安装所需 Java 运行时（设置-关于页入口）。
///
/// 从 Tuna Adoptium 镜像下载 ZIP 并解压到 java_cache/{major}/（便携式，
/// 不写注册表，与 主流启动器/主流启动器 一致）。装完后被 ModLoaderInstaller::findJavaPath
/// 自动发现（其第 2 步扫描 java_cache/{major}/bin/java.exe）。
///
/// 版本策略（2026-08-04 实测确认）：
///   - Java 8  → JRE（老版本游戏运行够用，体积小）
///   - Java 17 → JDK
///   - Java 25 → JDK
/// 架构策略（Windows）：
///   - x64 → x64；x86 → x32（Tuna 目录名）；ARM64 → x64（Win11 Prism 模拟运行，
///     实测 Temurin 17/25 无 Windows ARM64 构建，Adoptium API 0 结果确认）
///   - arm32 → 不支持
class JavaRuntimeInstaller : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString cpuArch READ cpuArch CONSTANT)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int currentStep READ currentStep NOTIFY progressChanged)
    Q_PROPERTY(int totalSteps READ totalSteps CONSTANT)
    Q_PROPERTY(QString statusText READ statusText NOTIFY progressChanged)

public:
    explicit JavaRuntimeInstaller(QObject* parent = nullptr);

    /// 检测当前 CPU 架构（映射到 Tuna 镜像目录名）: x64 / x32 / aarch64 / arm / unknown
    static QString detectCpuArch();
    QString cpuArch() const { return m_cpuArch; }

    bool running() const { return m_running; }
    int currentStep() const { return m_currentStep; }
    int totalSteps() const { return 3; }
    QString statusText() const { return m_statusText; }

    /// 一键安装 Java 8 (JRE) + 17 (JDK) + 25 (JDK)，跳过已安装
    Q_INVOKABLE void installRequiredJavas();
    /// 取消（当前版本下载完成后停止后续）
    Q_INVOKABLE void cancelInstall();

    /// 安装单个版本。返回 java.exe 路径（成功）或空串（失败/已取消）。
    /// type: "jdk" 或 "jre"
    QString installJava(int majorVersion, const QString& type);

    /// 从 Tuna 镜像按架构/类型/版本下载 ZIP 并解压到 java_cache/{major}/
    /// （与 ModLoaderInstaller::downloadAndExtractJava 同源，支持架构/类型参数化）
    QString downloadAndExtract(int majorVersion, const QString& type, const QString& arch);

signals:
    void progressChanged();
    void runningChanged();
    /// 单个版本安装完成（label: "Java 8 (JRE)" 等；path: java.exe 路径；skipped: 已存在跳过）
    void javaInstalled(const QString& label, const QString& path, bool skipped);
    /// 全部完成
    void finished(bool ok, const QString& error);
    void logMessage(const QString& msg);

private:
    /// 校验 java.exe 真实主版本号（java -version 解析）
    static int verifyJavaMajor(const QString& javaExe);
    /// 递归查找 bin/java.exe（非标准 ZIP 布局兜底）
    static QString findJavaExeRecursive(const QString& dir);

    QString m_cpuArch;
    bool m_running = false;
    bool m_cancelled = false;
    int m_currentStep = 0;
    QString m_statusText;
};

} // namespace ShadowLauncher
