import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tools'))
from encrypt_cf_key import _aes_expand_key, _aes_encrypt_block, _ghash, _gcm_encrypt, _gfmul

# 固定测试：全零 key/iv，单块明文 —— 与 NIST Case3 相同的结构
key = bytes(32)
iv = bytes(12)
pt = bytes(16)
ct, tag = _gcm_encrypt(key, iv, pt)
print('KEY =', key.hex())
print('IV  =', iv.hex())
print('PT  =', pt.hex())
print('CT  =', ct.hex())
print('TAG =', tag.hex())

# 中间值：H 和 GHASH(S)
rk = _aes_expand_key(key)
h = _aes_encrypt_block(bytes(16), rk)
print('H   =', h.hex())
# S = GHASH(CT || len-block)
ct_plus_len = ct + (0).to_bytes(8, 'big') + (128).to_bytes(8, 'big')
s = _ghash(h, ct_plus_len)
print('S   =', s.hex())
