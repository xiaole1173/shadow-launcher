/* LzmaDec.h — LZMA Decoder interface (public domain, from LZMA SDK) */

#ifndef LZMA_DEC_H
#define LZMA_DEC_H

#include "Types.h"

EXTERN_C_BEGIN

/* LZMA properties (5 bytes), stored at offset 0 of .lzma files:
   Byte 0:    encoded lc/lp/pb
   Bytes 1-4: dictionary size (little-endian UInt32)
   Bytes 5-12: uncompressed size (little-endian UInt64), -1 = unknown
*/

typedef enum
{
    LZMA_FINISH_ANY,   /* finish at any point */
    LZMA_FINISH_END    /* block must be finished at the end */
} ELzmaFinishMode;

typedef enum
{
    LZMA_STATUS_NOT_SPECIFIED,
    LZMA_STATUS_FINISHED_WITH_MARK,
    LZMA_STATUS_NOT_FINISHED,
    LZMA_STATUS_NEEDS_MORE_INPUT,
    LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK
} ELzmaStatus;

#define LZMA_PROPS_SIZE 5

/*
   Single-call LZMA decode: dest = decompress(src, propData)

   propData: 5 bytes from .lzma header (properties + dictSize encoded)
   src:      compressed data (starting after the 13-byte .lzma header)
   srcLen:   size of compressed data
   dest:     output buffer
   destLen:  in = output buffer size, out = actual decoded size
   alloc:    memory allocator (use &g_Alloc)
   finishMode: LZMA_FINISH_END for complete stream, LZMA_FINISH_ANY for partial
   status:   output status
*/
SRes LzmaDecode(Byte *dest, size_t *destLen,
                const Byte *src, size_t *srcLen,
                const Byte *propData, unsigned propSize,
                ELzmaFinishMode finishMode, ELzmaStatus *status,
                ISzAlloc *alloc);

EXTERN_C_END

#endif
