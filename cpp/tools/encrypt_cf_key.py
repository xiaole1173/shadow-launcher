#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ShadowLauncher CF API Key 加密内嵌工具
=====================================
用途：把作者的 CurseForge API Key 加密后写入本地私有头文件 cf_api_key_local.h
      （已被 .gitignore 忽略，不会进入远程仓库）。exe 内只含密文，不含明文。

算法：HKDF-SHA256(salt, IKM) 派生 32 字节 AES-256 密钥
      → AES-256-GCM 加密明文 Key（随机 12 字节 nonce，输出 16 字节 tag）

用法：
    # 交互输入（推荐，key 不留在 shell 历史）
    python tools/encrypt_cf_key.py

    # 或从环境变量读取（CI/脚本场景）
    set SHADOW_CF_KEY_TO_ENCRYPT=your-key
    python tools/encrypt_cf_key.py --from-env

    # 输出到指定文件（默认 src/core/cf_api_key_local.h）
    python tools/encrypt_cf_key.py -o src/core/cf_api_key_local.h

生成后重新编译 Release 即可；如需更换 Key，重跑本工具并重编译。
"""
import argparse
import hashlib
import hmac
import os
import secrets
import sys

# ── 常量（与 C++ 端 cf_key_crypto.h 保持一致）──
IKM_BYTES = 32          # HKDF 输入密钥材料（随机主密钥）
SALT_BYTES = 16         # HKDF 盐
NONCE_BYTES = 12        # GCM nonce（标准 96-bit）
TAG_BYTES = 16          # GCM tag
HKDF_INFO = b"ShadowLauncher-CF-Key-v1"
DEFAULT_OUT = os.path.join("src", "core", "cf_api_key_local.h")


# ══════════════════════════════════════════════════════════════
# AES-256 加密（仅需要加密方向；GCM 解密也只用 AES-encrypt）
# ══════════════════════════════════════════════════════════════

SBOX = [
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16,
]

def _xtime(b):
    return ((b << 1) ^ (0x1b if b & 0x80 else 0)) & 0xFF

def _gfmul(x, y):
    """GF(2^128) 乘法（GHASH 用）—— NIST SP 800-38D 附录 B 算法 1：
    从 Y 的 MSB 遍历，V 每步右移，最低位溢出时异或 R = 0xE1<<120。"""
    r = 0
    v = x
    red = 0xE1 << 120
    for i in range(128):
        if (y >> (127 - i)) & 1:
            r ^= v
        if v & 1:
            v = (v >> 1) ^ red
        else:
            v >>= 1
    return r

def _aes_expand_key(key):
    """AES-256 密钥扩展：返回 15 轮密钥 (240 字节)"""
    nk, nr = 8, 14
    w = [int.from_bytes(key[i:i+4], 'big') for i in range(0, 32, 4)]
    rcon = 1
    for i in range(nk, 4 * (nr + 1)):
        temp = w[i - 1]
        if i % nk == 0:
            temp = ((SBOX[(temp >> 16) & 0xFF] << 24) |
                    (SBOX[(temp >> 8) & 0xFF] << 16) |
                    (SBOX[temp & 0xFF] << 8) |
                    SBOX[(temp >> 24) & 0xFF])
            temp ^= (rcon << 24)
            rcon = _xtime(rcon)
        elif nk > 6 and i % nk == 4:
            temp = ((SBOX[(temp >> 24) & 0xFF] << 24) |
                    (SBOX[(temp >> 16) & 0xFF] << 16) |
                    (SBOX[(temp >> 8) & 0xFF] << 8) |
                    SBOX[temp & 0xFF])
        w.append(w[i - nk] ^ temp)
    return b''.join(x.to_bytes(4, 'big') for x in w)

def _aes_encrypt_block(block, rk):
    """加密单个 16 字节块"""
    state = list(block)
    def add_round_key(round_key):
        for i in range(16):
            state[i] ^= round_key[i]
    def sub_bytes():
        for i in range(16):
            state[i] = SBOX[state[i]]
    def shift_rows():
        state[1], state[5], state[9], state[13] = state[5], state[9], state[13], state[1]
        state[2], state[6], state[10], state[14] = state[10], state[14], state[2], state[6]
        state[3], state[7], state[11], state[15] = state[15], state[3], state[7], state[11]
    def mix_columns():
        for c in range(4):
            i0 = c * 4
            a0, a1, a2, a3 = state[i0], state[i0+1], state[i0+2], state[i0+3]
            state[i0]   = _xtime(a0) ^ (_xtime(a1) ^ a1) ^ a2 ^ a3
            state[i0+1] = a0 ^ _xtime(a1) ^ (_xtime(a2) ^ a2) ^ a3
            state[i0+2] = a0 ^ a1 ^ _xtime(a2) ^ (_xtime(a3) ^ a3)
            state[i0+3] = (_xtime(a0) ^ a0) ^ a1 ^ a2 ^ _xtime(a3)
    add_round_key(rk[0:16])
    for rnd in range(1, 14):
        sub_bytes(); shift_rows(); mix_columns(); add_round_key(rk[rnd*16:(rnd+1)*16])
    sub_bytes(); shift_rows(); add_round_key(rk[224:240])
    return bytes(state)

def _ghash(h, data):
    """GHASH：GF(2^128) 上的多项式哈希，H 为 128 位密钥"""
    h_int = int.from_bytes(h, 'big')
    y = 0
    for i in range(0, len(data), 16):
        block = data[i:i+16] + b'\x00' * (16 - len(data[i:i+16]))
        y ^= int.from_bytes(block, 'big')
        y = _gfmul(y, h_int)
    return y.to_bytes(16, 'big')

def _gcm_encrypt(key, nonce, plaintext, aad=b''):
    """AES-256-GCM 加密，返回 (ciphertext, tag)"""
    rk = _aes_expand_key(key)
    h = _aes_encrypt_block(b'\x00' * 16, rk)
    # J0 = IV || 0^31 || 1
    j0 = nonce + b'\x00\x00\x00\x01'
    keystream = b''
    # GCM 数据块计数器从 inc32(J0) 开始（第一个块 = IV||0x2），tag 才用 J0 本身
    counter = int.from_bytes(j0, 'big') + 1
    for _ in range((len(plaintext) + 15) // 16):
        ks = _aes_encrypt_block(counter.to_bytes(16, 'big'), rk)
        keystream += ks
        counter = (counter + 1) & ((1 << 128) - 1)
    ciphertext = bytes(p ^ k for p, k in zip(plaintext, keystream))
    # S = GHASH(AAD || pad || CT || pad || len(AAD)||len(CT))
    aad_len = (len(aad) + 15) // 16 * 16
    ct_len = (len(ciphertext) + 15) // 16 * 16
    s = _ghash(h, aad + b'\x00' * (aad_len - len(aad))
                  + ciphertext + b'\x00' * (ct_len - len(ciphertext))
                  + (len(aad) * 8).to_bytes(8, 'big') + (len(ciphertext) * 8).to_bytes(8, 'big'))
    tag = bytes(x ^ y for x, y in zip(_aes_encrypt_block(j0, rk), s))
    return ciphertext, tag


# ══════════════════════════════════════════════════════════════
# HKDF-SHA256 (RFC 5869)
# ══════════════════════════════════════════════════════════════

def hkdf_sha256(ikm, salt, info, length=32):
    prk = hmac.new(salt, ikm, hashlib.sha256).digest()
    out = b''
    t = b''
    counter = 1
    while len(out) < length:
        t = hmac.new(prk, t + info + bytes([counter]), hashlib.sha256).digest()
        out += t
        counter += 1
    return out[:length]


# ══════════════════════════════════════════════════════════════
# 主流程
# ══════════════════════════════════════════════════════════════

def build_header(plain_key: str) -> str:
    ikm = secrets.token_bytes(IKM_BYTES)
    salt = secrets.token_bytes(SALT_BYTES)
    nonce = secrets.token_bytes(NONCE_BYTES)
    key = hkdf_sha256(ikm, salt, HKDF_INFO, 32)
    ct, tag = _gcm_encrypt(key, nonce, plain_key.encode('utf-8'))
    # 自检：用同一套逻辑解密验证
    assert _gcm_decrypt_self(key, nonce, ct, tag) == plain_key.encode('utf-8'), "自检失败"
    return f"""// SPDX-License-Identifier: AGPL-3.0-or-later
// ⚠️ 本文件由 tools/encrypt_cf_key.py 自动生成，已被 .gitignore 忽略 —— 严禁提交到远程仓库！
// 内容为加密后的 CurseForge API Key（应用标识），不含明文。
// 算法：HKDF-SHA256(salt, IKM) → AES-256-GCM，详见 src/core/cf_key_crypto.h。
// 更换 Key：重跑 python tools/encrypt_cf_key.py 后重新编译 Release。

#pragma once

#define SHADOW_CF_ENC_IKM_HEX    "{ikm.hex()}"
#define SHADOW_CF_ENC_SALT_HEX   "{salt.hex()}"
#define SHADOW_CF_ENC_NONCE_HEX  "{nonce.hex()}"
#define SHADOW_CF_ENC_CIPHER_HEX "{ct.hex()}"
#define SHADOW_CF_ENC_TAG_HEX    "{tag.hex()}"
"""

def _gcm_decrypt_self(key, nonce, ciphertext, tag, aad=b''):
    """仅用于工具自检的 GCM 解密（与 _gcm_encrypt 同源逻辑）"""
    rk = _aes_expand_key(key)
    h = _aes_encrypt_block(b'\x00' * 16, rk)
    j0 = nonce + b'\x00\x00\x00\x01'
    keystream = b''
    # 解密与加密同源：数据块计数器同样从 inc32(J0) 开始
    counter = int.from_bytes(j0, 'big') + 1
    for _ in range((len(ciphertext) + 15) // 16):
        ks = _aes_encrypt_block(counter.to_bytes(16, 'big'), rk)
        keystream += ks
        counter = (counter + 1) & ((1 << 128) - 1)
    plaintext = bytes(c ^ k for c, k in zip(ciphertext, keystream))
    aad_len = (len(aad) + 15) // 16 * 16
    ct_len = (len(ciphertext) + 15) // 16 * 16
    s = _ghash(h, aad + b'\x00' * (aad_len - len(aad))
                  + ciphertext + b'\x00' * (ct_len - len(ciphertext))
                  + (len(aad) * 8).to_bytes(8, 'big') + (len(ciphertext) * 8).to_bytes(8, 'big'))
    expect_tag = bytes(x ^ y for x, y in zip(_aes_encrypt_block(j0, rk), s))
    if not hmac.compare_digest(expect_tag, tag):
        raise ValueError("GCM tag 校验失败（自检）")
    return plaintext


def main():
    ap = argparse.ArgumentParser(description="ShadowLauncher CF API Key 加密内嵌工具")
    ap.add_argument("-o", "--output", default=DEFAULT_OUT, help="输出头文件路径")
    ap.add_argument("--from-env", action="store_true",
                    help="从环境变量 SHADOW_CF_KEY_TO_ENCRYPT 读取明文 Key")
    args = ap.parse_args()

    plain = None
    if args.from_env:
        plain = os.environ.get("SHADOW_CF_KEY_TO_ENCRYPT", "")
    if not plain:
        import getpass
        plain = getpass.getpass("请输入 CurseForge API Key（输入不回显）: ").strip()
    if not plain:
        print("[FAIL] Key 为空", file=sys.stderr)
        return 1

    header = build_header(plain)
    out = os.path.abspath(args.output)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        f.write(header)
    print(f"[OK] 已生成加密头文件: {out}")
    print(f"     密文 {len(plain)} 字节明文 → {len(plain.encode()):d} B, IKM/SALT/NONCE 随机生成")
    print(f"     请重新编译 Release；本文件已被 git 忽略，不会进入远程仓库")
    return 0


if __name__ == "__main__":
    sys.exit(main())
