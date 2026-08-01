// CF API Key 加密链路互操作测试：
//   1. 使用 Python 工具链生成的固定测试向量（tests/cf_test_vectors.h）验证 C++ 解密
//   2. 使用真实 cf_api_key_local.h（本地私有，git 忽略）验证端到端解密
// 构建：cmake -S . -B build-test -DSHADOW_BUILD_MODPACK_SELFTEST=ON
// 运行：build-test/Release/CfCryptoInteropTest.exe
#include <QCoreApplication>
#include <QString>
#include <cstdio>
#include <cstring>

#include "cf_test_vectors.h"
#include "core/cf_key_crypto.h"

#if defined(__has_include)
#  if __has_include("core/cf_api_key_local.h")
#    include "core/cf_api_key_local.h"
#    define HAVE_CF_LOCAL 1
#  endif
#endif
#ifndef HAVE_CF_LOCAL
#  define HAVE_CF_LOCAL 0
#endif

static QByteArray fromHex(const char* s)
{
    QByteArray out(static_cast<int>(strlen(s)) / 2, '\0');
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (int i = 0; i + 1 < static_cast<int>(strlen(s)); i += 2) {
        const int hi = nib(s[i]), lo = nib(s[i+1]);
        if (hi < 0 || lo < 0) return {};
        out[i/2] = static_cast<char>((hi << 4) | lo);
    }
    return out;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    using namespace ShadowLauncher;

    int failures = 0;

    // 1) Python 固定向量（明文 "ShadowTest-CF-Key-互操作验证"）
    {
        const QString key = CfKeyCrypto::decryptEmbeddedCfKey(
            TEST_IKM_HEX, TEST_SALT_HEX, TEST_NONCE_HEX, TEST_CIPHER_HEX, TEST_TAG_HEX);
        const bool ok = (key == QString::fromUtf8(TEST_EXPECT));
        std::printf("[%s] Python 固定向量解密: %s\n", ok ? "PASS" : "FAIL",
                    key.toUtf8().constData());
        if (!ok) ++failures;
    }

    // 2) 真实 cf_api_key_local.h（仅本地存在时）
    if (HAVE_CF_LOCAL) {
        const QString key = CfKeyCrypto::decryptEmbeddedCfKey(
            SHADOW_CF_ENC_IKM_HEX, SHADOW_CF_ENC_SALT_HEX,
            SHADOW_CF_ENC_NONCE_HEX, SHADOW_CF_ENC_CIPHER_HEX, SHADOW_CF_ENC_TAG_HEX);
        const bool ok = !key.isEmpty();
        std::printf("[%s] 真实本地头文件解密: 长度=%d%s\n", ok ? "PASS" : "FAIL",
                    key.size(), ok ? "" : " (解密失败)");
        if (!ok) ++failures;
    } else {
        std::printf("[SKIP] 真实本地头文件不存在（cf_api_key_local.h 未生成）\n");
    }

    std::printf(failures == 0 ? "总结果: PASS\n" : "总结果: FAIL (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
