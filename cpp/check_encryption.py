"""Verify that encryption/decryption is working correctly."""
import ctypes, struct, os, base64, json, sys
from ctypes import wintypes

# === Read key file ===
key_path = os.path.expandvars(r'%APPDATA%\ShadowTeam\Shadow Launcher\shadow\.token_key')
with open(key_path, 'rb') as f:
    blob = f.read()
print(f'Key file: {len(blob)} bytes at {key_path}')

# === Read session file ===
sess_path = os.path.expandvars(r'%APPDATA%\ShadowTeam\Shadow Launcher\shadow\microsoft_session.json')
with open(sess_path, 'r', encoding='utf-8') as f:
    session = json.load(f)

print(f'encrypted flag: {session.get("encrypted")}')
print(f'username: {session.get("username")}')
print(f'mcToken length: {len(session.get("mcToken",""))}')
print(f'refreshToken length: {len(session.get("refreshToken",""))}')
print(f'tokenObtainedAt: {session.get("tokenObtainedAt")}')
print(f'tokenExpiresIn: {session.get("tokenExpiresIn")}')

# === Try to decrypt via DPAPI using ctypes ===
# Windows DPAPI functions
crypt32 = ctypes.windll.crypt32
kernel32 = ctypes.windll.kernel32

class DATA_BLOB(ctypes.Structure):
    _fields_ = [
        ('cbData', wintypes.DWORD),
        ('pbData', ctypes.POINTER(wintypes.BYTE)),
    ]

CryptUnprotectData = crypt32.CryptUnprotectData
CryptUnprotectData.argtypes = [ctypes.POINTER(DATA_BLOB), wintypes.LPWSTR,
                                ctypes.POINTER(DATA_BLOB), ctypes.c_void_p,
                                ctypes.c_void_p, wintypes.DWORD,
                                ctypes.POINTER(DATA_BLOB)]
CryptUnprotectData.restype = wintypes.BOOL

in_blob = DATA_BLOB()
in_blob.cbData = len(blob)
in_blob.pbData = ctypes.cast(ctypes.create_string_buffer(blob, len(blob)), ctypes.POINTER(wintypes.BYTE))

out_blob = DATA_BLOB()
result = CryptUnprotectData(ctypes.byref(in_blob), None, None, None, None, 0, ctypes.byref(out_blob))

if result:
    key = ctypes.string_at(out_blob.pbData, out_blob.cbData)
    kernel32.LocalFree(out_blob.pbData)
    print(f'Decrypted key: {len(key)} bytes (should be 32)')
    print(f'Key hex: {key.hex()[:32]}...')
    
    # === Now try AES-GCM decrypt of the session fields ===
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    
    aesgcm = AESGCM(key)
    
    def try_decrypt(field_name, field_value):
        if not field_value:
            print(f'  {field_name}: EMPTY')
            return
        try:
            combined = base64.b64decode(field_value)
            if len(combined) < 12 + 16 + 1:
                print(f'  {field_name}: TOO SHORT ({len(combined)} bytes)')
                return
            nonce = combined[:12]
            ciphertext = combined[12:-16]
            tag = combined[-16:]
            plaintext = aesgcm.decrypt(nonce, ciphertext + tag, None)
            print(f'  {field_name}: OK -> {plaintext.decode("utf-8")[:80]}')
        except Exception as e:
            print(f'  {field_name}: DECRYPT FAILED - {e}')
    
    try_decrypt('uuid', session.get('uuid'))
    try_decrypt('mcToken', session.get('mcToken'))
    try_decrypt('refreshToken', session.get('refreshToken'))
    try_decrypt('tokenObtainedAt', str(session.get('tokenObtainedAt','')))
    try_decrypt('tokenExpiresIn', str(session.get('tokenExpiresIn','')))
    
else:
    err = kernel32.GetLastError()
    print(f'CryptUnprotectData FAILED: error {err}')
