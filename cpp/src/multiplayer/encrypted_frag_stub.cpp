// encrypted_frag_stub.cpp — Placeholder for open source / CI builds.
// When the real encrypted fragment .cpp files are present (gitignored, local),
// they override this stub. The build system conditionally selects sources.
//
// These zero-value definitions produce a zero-filled assembled blob,
// which fails AES-GCM authentication → Worker & Relay features disabled.

#include "encrypted_frag_1.h"
#include "encrypted_frag_2.h"
#include "encrypted_frag_3.h"
#include "encrypted_frag_4.h"
#include "encrypted_frag_5.h"

namespace Frag1 { const uint8_t data[49] = {0}; const uint8_t perm[49] = {0}; }
namespace Frag2 { const uint8_t data[49] = {0}; const uint8_t perm[49] = {0}; }
namespace Frag3 { const uint8_t data[49] = {0}; const uint8_t perm[49] = {0}; }
namespace Frag4 { const uint8_t data[48] = {0}; const uint8_t perm[48] = {0}; }
namespace Frag5 { const uint8_t data[48] = {0}; const uint8_t perm[48] = {0}; }
