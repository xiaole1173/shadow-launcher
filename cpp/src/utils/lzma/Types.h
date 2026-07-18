/* Types.h — 7z ANSI-C types (public domain, from LZMA SDK) */

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
typedef uint32_t UInt32;
typedef uint64_t UInt64;
typedef int32_t SRes;

#define SZ_OK 0
#define SZ_ERROR_DATA 1
#define SZ_ERROR_MEM 2
#define SZ_ERROR_UNSUPPORTED 4

typedef struct
{
    void *(*Alloc)(void *p, size_t size);
    void (*Free)(void *p, void *address);
} ISzAlloc;

static void *SzAlloc(void *p, size_t size) { (void)p; return malloc(size); }
static void SzFree(void *p, void *address) { (void)p; free(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

EXTERN_C_END

#endif
