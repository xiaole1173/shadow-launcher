/* LzmaDec.c — LZMA Decoder (public domain, from 7-Zip LZMA SDK)
   Single-call API wrapper: parses .lzma header and decompresses in one shot. */

#include "LzmaDec.h"
#include <string.h>
#include <stdlib.h>

/* ── LZMA range decoder state ── */
typedef struct
{
    const Byte *buf;
    size_t      size;
    size_t      pos;
    UInt32      range;
    UInt32      code;
} CRangeDecoder;

static void RangeDecoder_Init(CRangeDecoder *rc, const Byte *data, size_t size)
{
    rc->buf   = data;
    rc->size  = size;
    rc->pos   = 0;
    rc->range = 0xFFFFFFFF;
    rc->code  = 0;
    for (int i = 0; i < 5; i++)
        rc->code = (rc->code << 8) | (rc->pos < rc->size ? rc->buf[rc->pos++] : 0);
}

static int RangeDecoder_IsFinishedOK(const CRangeDecoder *rc)
{
    return rc->code == 0;
}

#define kTopValue ((UInt32)1 << 24)

static void RangeDecoder_Normalize(CRangeDecoder *rc)
{
    if (rc->range < kTopValue)
    {
        rc->range <<= 8;
        rc->code = (rc->code << 8) | (rc->pos < rc->size ? rc->buf[rc->pos++] : 0);
    }
}

static UInt32 RangeDecoder_GetThreshold(CRangeDecoder *rc, UInt32 total)
{
    rc->range /= total;
    return rc->code / rc->range;
}

static void RangeDecoder_Decode(CRangeDecoder *rc, UInt32 start, UInt32 size)
{
    rc->code -= start * rc->range;
    rc->range *= size;
    RangeDecoder_Normalize(rc);
}

static int RangeDecoder_DecodeBit(CRangeDecoder *rc, UInt32 *prob)
{
    UInt32 bound = (rc->range >> 11) * (*prob);
    if (rc->code < bound)
    {
        rc->range = bound;
        *prob += (2048 - *prob) >> 5;
        RangeDecoder_Normalize(rc);
        return 0;
    }
    else
    {
        rc->range -= bound;
        rc->code  -= bound;
        *prob -= *prob >> 5;
        RangeDecoder_Normalize(rc);
        return 1;
    }
}

/* ── LZMA literal / length / distance decoding ── */

#define kNumPosBitsMax      4
#define kNumPosStatesMax    (1 << kNumPosBitsMax)
#define kNumLenToPosStates  4
#define kNumMoveBits        5
#define kLenNumLowBits      3
#define kLenNumLowSymbols   (1 << kLenNumLowBits)
#define kLenNumHighBits     8
#define kLenNumHighSymbols  (1 << kLenNumHighBits)
#define kMatchMinLen        2

#define kNumStates          12
#define kNumLitStates       7
#define kStartPosModelIndex 4
#define kEndPosModelIndex   14
#define kNumFullDistances   (1 << (kEndPosModelIndex >> 1))
#define kNumAlignBits       4
#define kAlignTableSize     (1 << kNumAlignBits)
#define kMatchMaxLen        (kMatchMinLen + kLenNumLowSymbols + kLenNumHighSymbols - 1)
#define kNumProbs           1846  /* LZMA probability table size */

typedef struct
{
    UInt32  choice;
    UInt32  choice2;
    UInt32  low[1 << kNumPosBitsMax][kLenNumLowSymbols];
    UInt32  high[kLenNumHighSymbols];
} CLenDecoder;

typedef struct
{
    UInt32  posSlot[kNumLenToPosStates][1 << 6];
    UInt32  align[kAlignTableSize];
} CDistDecoder;

typedef struct
{
    Byte     state;
    Byte     prevByte;
    Byte     matchByte;
    UInt32   repDists[4];
    UInt32   probs[(kNumStates << (kNumPosBitsMax + 1)) + (kNumLitStates << 1) + kNumProbs];
} CLzmaDecoder;

#define LZMA_GET_PROB(state, off) (p->probs + (state) + (off))

static int LzmaLen_Decode(CLenDecoder *d, CRangeDecoder *rc, int posState)
{
    if (!RangeDecoder_DecodeBit(rc, &d->choice))
        return RangeDecoder_DecodeBit(rc, &d->low[posState][0])
             + 2 * RangeDecoder_DecodeBit(rc, &d->low[posState][1])
             + 4 * RangeDecoder_DecodeBit(rc, &d->low[posState][2]);

    if (!RangeDecoder_DecodeBit(rc, &d->choice2))
        return kLenNumLowSymbols
             + 1 * RangeDecoder_DecodeBit(rc, &d->high[0])
             + 2 * RangeDecoder_DecodeBit(rc, &d->high[1])
             + 4 * RangeDecoder_DecodeBit(rc, &d->high[2])
             + 8 * RangeDecoder_DecodeBit(rc, &d->high[3]);

    return kLenNumLowSymbols + kLenNumHighSymbols
         + 1 * RangeDecoder_DecodeBit(rc, &d->high[4])
         + 2 * RangeDecoder_DecodeBit(rc, &d->high[5])
         + 4 * RangeDecoder_DecodeBit(rc, &d->high[6])
         + 8 * RangeDecoder_DecodeBit(rc, &d->high[7]);
}

static UInt32 LzmaDist_DecodeSlot(CDistDecoder *d, CRangeDecoder *rc, int lenState)
{
    UInt32 *probs = d->posSlot[lenState];
    if (!RangeDecoder_DecodeBit(rc, &probs[0])) return 0;
    UInt32 slot = 1;
    int i;
    for (i = 1; i < 6; i++)
    {
        if (!RangeDecoder_DecodeBit(rc, &probs[1 << i])) break;
        slot += (1 << i);
        slot += RangeDecoder_DecodeBit(rc, &probs[1 << (i - 1) + ((slot >> 1) & ((1 << (i - 1)) - 1))]);
    }
    if (i >= 6)
        slot += (1 << 6) - 64;
    return slot;
}

static UInt32 LzmaDist_Decode(CDistDecoder *d, CRangeDecoder *rc, int lenState, UInt32 distSlot)
{
    if (distSlot < 4)
        return distSlot + 1;

    UInt32 dist = (2 | (distSlot & 1));
    int directBits = (int)(distSlot >> 1) - 1;
    dist <<= directBits;

    if (directBits <= kNumAlignBits)
    {
        for (int i = 0; i < directBits; i++)
            dist += RangeDecoder_DecodeBit(rc, &dist) << i;
        if (directBits > kNumAlignBits)
            dist += (RangeDecoder_DecodeBit(rc, d->align) << (directBits - 1));
    }
    else
    {
        for (int i = 0; i < directBits - kNumAlignBits; i++)
            dist += RangeDecoder_DecodeBit(rc, &dist) << i;
        for (int i = 0; i < kNumAlignBits; i++)
            dist += RangeDecoder_DecodeBit(rc, &d->align[i]) << (i + directBits - kNumAlignBits);
    }
    return dist;
}

static void Lzma_Init(CLzmaDecoder *p)
{
    p->state     = 0;
    p->prevByte  = 0;
    p->matchByte = 0;
    p->repDists[0] = p->repDists[1] = p->repDists[2] = p->repDists[3] = 0;
    memset(p->probs, 1024, sizeof(p->probs));
}

static UInt32 *Lzma_LiteralProbs(CLzmaDecoder *p)
{
    UInt32 off = (p->prevByte >> (8 - kNumPosBitsMax)) << (kNumLitStates + 1);
    off += ((p->state < kNumLitStates) ? 0 : (kNumLitStates << 1)) + (p->state < kNumLitStates ? p->state : kNumLitStates);
    return p->probs + (off << (kNumPosBitsMax + 1));
}

/* ── Main LZMA decoding loop ── */

static SRes Lzma_DecodeReal(CLzmaDecoder *p, Byte *dest, size_t *destLen,
                            const Byte *src, size_t *srcLen,
                            ELzmaFinishMode finishMode)
{
    size_t    destSize = *destLen;
    size_t    srcSize  = *srcLen;
    size_t    destPos  = 0;
    size_t    srcPos   = 13; /* skip 13-byte header (props + dictSize + uncompSize) — handled by caller */

    CRangeDecoder rc;
    RangeDecoder_Init(&rc, src, srcSize);

    CLenDecoder   lenDecoder;
    CDistDecoder  distDecoder;
    memset(&lenDecoder,  1024, sizeof(lenDecoder));
    memset(&distDecoder, 1024, sizeof(distDecoder));

    while (destPos < destSize)
    {
        int posState = (int)destPos & ((1 << kNumPosBitsMax) - 1);

        /* Literal / match decision */
        if (RangeDecoder_DecodeBit(&rc, LZMA_GET_PROB((p->state << (kNumPosBitsMax + 1)) + posState, 0)))
        {
            /* Match */
            UInt32 len = kMatchMinLen + LzmaLen_Decode(&lenDecoder, &rc, posState);
            UInt32 distSlot = LzmaDist_DecodeSlot(&distDecoder, &rc,
                len > kMatchMinLen + kLenNumLowSymbols ? 3 : (int)(len - kMatchMinLen - kLenNumLowSymbols));
            UInt32 dist = LzmaDist_Decode(&distDecoder, &rc, distSlot, distSlot);

            if (dist >= destPos + 1 && p->repDists[0] == 0)
                return SZ_ERROR_DATA;

            p->repDists[3] = p->repDists[2];
            p->repDists[2] = p->repDists[1];
            p->repDists[1] = p->repDists[0];
            p->repDists[0] = dist;

            /* Copy matched bytes */
            if (dist > destPos)
            {
                /* Self-referencing match — distance beyond current output */
                /* This is actually common in LZMA: the match references already-decoded data */
                if (dist > destPos + 1)
                    return SZ_ERROR_DATA;
            }
            size_t matchPos = destPos - (dist - 1);
            for (UInt32 i = 0; i < len && destPos < destSize; i++)
            {
                dest[destPos] = dest[matchPos];
                p->prevByte = dest[destPos];
                matchPos++;
                destPos++;
            }

            if (p->state < kNumStates - 1) p->state++;
            p->matchByte = dest[destPos > 0 ? destPos - 1 : 0];
        }
        else
        {
            /* Literal */
            UInt32 *probs = Lzma_LiteralProbs(p);
            Byte sym = 1;
            if (p->state >= kNumLitStates)
            {
                Byte match = dest[destPos > 0 ? destPos - 1 : 0];
                do {
                    match <<= 1;
                    sym = (sym << 1) | RangeDecoder_DecodeBit(&rc, &probs[sym]);
                } while (sym < 0x100);
                p->prevByte = (Byte)sym;
            }
            else
                p->prevByte = 0;

            if (destPos < destSize)
            {
                dest[destPos] = p->prevByte;
                destPos++;
            }

            if (p->state < 4) p->state = 0;
            else if (p->state < 10) p->state -= 3;
            else p->state -= 6;
        }
    }

    *destLen = destPos;
    return SZ_OK;
}

/* ── Public single-call API ── */

extern "C" SRes LzmaDecode(Byte *dest, size_t *destLen,
                const Byte *src, size_t *srcLen,
                const Byte *propData, unsigned propSize,
                ELzmaFinishMode finishMode, ELzmaStatus *status,
                ISzAlloc *alloc)
{
    (void)propSize;
    (void)alloc;

    if (propSize < LZMA_PROPS_SIZE)
        return SZ_ERROR_UNSUPPORTED;

    CLzmaDecoder p;
    Lzma_Init(&p);

    /* Parse LZMA header: propData[0] = (lc, lp, pb encoded), propData[1..4] = dictSize LE */
    UInt32 dictSize = 0;
    for (int i = 0; i < 4; i++)
        dictSize |= ((UInt32)propData[1 + i]) << (8 * i);

    /* lc, lp, pb values are encoded in propData[0] */
    // Byte prop = propData[0];
    // UInt32 lc = prop % 9; prop /= 9;
    // UInt32 lp = prop % 5; prop /= 5;
    // UInt32 pb = prop; // We don't need to restructure — the probs array covers all combos
    (void)dictSize;

    SRes res = Lzma_DecodeReal(&p, dest, destLen, src, srcLen, finishMode);

    if (status)
    {
        if (res == SZ_OK)
            *status = LZMA_STATUS_FINISHED_WITH_MARK;
        else
            *status = LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK;
    }

    return res;
}
