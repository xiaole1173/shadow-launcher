/* Types.h — 7z types (minimal, from LZMA SDK) */

#ifndef LZMA_TYPES_H
#define LZMA_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef EXTERN_C_BEGIN
#ifdef __cplusplus
#define EXTERN_C_BEGIN extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C_BEGIN
#define EXTERN_C_END
#endif
#endif

EXTERN_C_BEGIN

typedef uint8_t Byte;
typedef int32_t Int32;
typedef uint16_t UInt16;
typedef uint32_t UInt32;
typedef uint64_t UInt64;
typedef int BoolInt;
typedef int32_t SRes;
typedef size_t SizeT;

#define SZ_OK 0
#define SZ_ERROR_DATA 1
#define SZ_ERROR_MEM 2
#define SZ_ERROR_UNSUPPORTED 4
#define SZ_ERROR_PARAM 5
#define SZ_ERROR_INPUT_EOF 6
#define SZ_ERROR_OUTPUT_EOF 7
#define SZ_ERROR_FAIL 11

/* MSVC x64: __fastcall is default, so Z7_FASTCALL is a no-op */
#ifndef Z7_FASTCALL
#define Z7_FASTCALL
#endif

/* Detect x86/x64 for LZMA SDK assembly-optimized probability layout */
#if defined(_M_AMD64) || defined(__amd64__) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
#ifndef MY_CPU_X86_OR_AMD64
#define MY_CPU_X86_OR_AMD64
#endif
#endif

#ifndef True
#define True 1
#endif
#ifndef False
#define False 0
#endif

#ifndef RINOK
#define RINOK(x) { const int _result_ = (x); if (_result_ != 0) return _result_; }
#endif

struct ISzAlloc
{
    void *(*Alloc)(const struct ISzAlloc *p, size_t size);
    void (*Free)(const struct ISzAlloc *p, void *address); /* address can be 0 */
};
typedef struct ISzAlloc ISzAlloc;
typedef const ISzAlloc * ISzAllocPtr;

static void *_SzAlloc_impl(const struct ISzAlloc *p, size_t size) { if (p) {} return malloc(size); }
static void _SzFree_impl(const struct ISzAlloc *p, void *address) { if (p) {} free(address); }

/* Wrappers expected by 7z SDK */
static inline void *ISzAlloc_Alloc(ISzAllocPtr p, size_t size) { return p->Alloc(p, size); }
static inline void ISzAlloc_Free(ISzAllocPtr p, void *address) { p->Free(p, address); }

EXTERN_C_END

/* Allocator instance — placed outside extern "C" so C++ code can use &g_Alloc as ISzAlloc* */
static ISzAlloc g_Alloc = { _SzAlloc_impl, _SzFree_impl };

#endif
