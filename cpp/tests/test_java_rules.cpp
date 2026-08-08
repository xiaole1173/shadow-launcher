// 验证 Java 需求判定规则（与 src/backend/shadow_backend.cpp L2333 inferJavaByMcVersion 同步）：
// 1.16.5- → 8；1.17 → 16；1.18-1.20.4 → 17；1.20.5-1.21.x → 21；22+ → 25
// 注意：此文件是规则快照，改动真实规则时必须同步更新两处
#include <QCoreApplication>
#include <cstdio>
#include "utils/logger.h"

using namespace ShadowLauncher;

static int inferJava(const QString& mcVersion)
{
    if (mcVersion.isEmpty()) return 8;
    QStringList parts = mcVersion.split(QStringLiteral("."));
    if (parts.size() < 2) return 8;
    int major = parts[0].toInt();
    int minor = parts[1].toInt();
    int rev = (parts.size() >= 3) ? parts[2].split(QStringLiteral("-"))[0].toInt() : 0;
    if (major >= 22) return 25;
    if (major == 1) {
        if (minor >= 22) return 25;
        if (minor >= 21) return 21;
        if (minor >= 20) return (rev >= 5) ? 21 : 17;
        if (minor == 18 || minor == 19) return 17;
        if (minor == 17) return 16;
        return 8;
    }
    return 8;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ShadowLauncher::installFileLogger(QCoreApplication::applicationDirPath());
    struct { const char* ver; int expect; } cases[] = {
        {"1.16.5", 8}, {"1.16.1", 8}, {"1.12.2", 8}, {"1.7.10", 8}, {"1.4.7", 8},
        {"1.17", 16}, {"1.17.1", 16},
        {"1.18", 17}, {"1.19.4", 17}, {"1.20.4", 17},
        {"1.20.5", 21}, {"1.20.6", 21}, {"1.21", 21}, {"1.21.4", 21}, {"1.21.11", 21},
        {"22.1", 25}, {"25.1", 25}, {"26.2", 25}, {"26.2-forge-65.1.0", 25},
    };
    int fail = 0;
    for (const auto& c : cases) {
        const int got = inferJava(QString::fromUtf8(c.ver));
        const bool ok = (got == c.expect);
        fprintf(stderr, "[rule] %-22s → Java %d (expect %d) %s\n", c.ver, got, c.expect, ok ? "OK" : "FAIL");
        if (!ok) fail++;
    }
    fprintf(stderr, "=== %s\n", fail ? "FAIL" : "PASS");
    return fail ? 1 : 0;
}
