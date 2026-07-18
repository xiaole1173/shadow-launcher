// Relayed multiplayer config — encrypted with tools/encrypt_addr.py
// SEE: tools/encrypt_addr.py — run it locally to encrypt your own relay address
// The encrypted blobs below are PLACEHOLDERS. Replace them with your own.
#pragma once
#include <cstdint>

namespace Relay {

// Salt XOR'd with raw key before decryption (all zeros = no key configured)
constexpr uint8_t  kSalt[8]       = { 0 };

// Raw AES-256 key — XOR with salt to get real key (all zeros = not configured)
constexpr uint8_t  kRawKey[32]    = { 0 };

// ── Relay endpoint: tcp://host:port ──
constexpr uint8_t  kRelayNonce[12]= { 0 };
constexpr uint8_t  kRelayCT[]     = { 0 };
constexpr uint32_t kRelayCTLen    = 0;
constexpr uint8_t  kRelayTag[16]  = { 0 };

// ── IP prefix filter ──
constexpr uint8_t  kPrefixNonce[12]= { 0 };
constexpr uint8_t  kPrefixCT[]    = { 0 };
constexpr uint32_t kPrefixCTLen   = 0;
constexpr uint8_t  kPrefixTag[16] = { 0 };

} // namespace Relay
