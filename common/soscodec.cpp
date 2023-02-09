#include "soscomp.h"
#include <string.h>
#include <assert.h>

// index table for stepping into step table.
static const short wCODECIndexTab[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

static const short wCODECStepTab[89] = {
    7,    8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,    25,   28,
    31,   34,    37,    41,    45,    50,    55,    60,    66,    73,    80,    88,    97,    107,  118,
    130,  143,   157,   173,   190,   209,   230,   253,   279,   307,   337,   371,   408,   449,  494,
    544,  598,   658,   724,   796,   876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878, 2066,
    2272, 2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845, 8630,
    9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

#ifndef clamp
#define clamp(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#endif

void sosCODECInitStream(_SOS_COMPRESS_INFO* stream)
{
    stream->wCode = 0;
    stream->wCodeBuf = 0;
    stream->wIndex = 0;
    stream->wStep = wCODECStepTab[stream->wIndex];
    stream->dwPredicted = 0;
    stream->dwSampleIndex = 0;

    stream->wCode2 = 0;
    stream->wCodeBuf2 = 0;
    stream->wIndex2 = 0;
    stream->wStep2 = wCODECStepTab[stream->wIndex2];
    stream->dwPredicted2 = 0;
    stream->dwSampleIndex2 = 0;
}

/* Number of possible wIndex.  Comes from the fact that:
 *
 *   next_index = clamp(next_index, 0, 88);
 *
 * which means 0 <= index <= 88, hence 89 indexes.
 */
#define NUM_INDEXES 89

/* Number of possible nybbles.  Comes from the fact that:
 *
 *   next_nybble = wCodeBuf & 0xF
 *
 * which means 0 <= next_nybble <= 15, hence 16 possibilites.
 */
#define NUM_NYBBLES 16

/* Define a dynamic programming table mapping all possible indexes and nybbles
 * into their next value.  Pack things together into a struct so a cache miss
 * will retrieve both next index and diff value.
 *
 * This table should consume ~12kb, which is quite small.
 *
 */
static struct
{
    int diff;
    short index;
} SosDecompTable[NUM_INDEXES][NUM_NYBBLES];

/* Flag if above table was initialized.  */
static bool SosDecompTableGenerated = false;

/* Generate decompression table for 16-bit mono samples.  Precompute every
 * possible value of dwDifference and wIndex based on every possible
 * combination of index and nybble values.  */
void sosCODECGenerateDecompressTable(void)
{
    short index, nybble;
    int diff;

    for (index = 0; index < NUM_INDEXES; index++) {
        short step = wCODECStepTab[index];
        for (nybble = 0; nybble < NUM_NYBBLES; nybble++) {
            diff = step >> 3;

            if ((nybble & 4) != 0) {
                diff += step;
            }

            if ((nybble & 2) != 0) {
                diff += step >> 1;
            }

            if ((nybble & 1) != 0) {
                diff += step >> 2;
            }

            if ((nybble & 8) != 0) {
                diff = -diff;
            }

            short next_index = index + wCODECIndexTab[nybble & 0x7];
            next_index = clamp(next_index, 0, 88);

            SosDecompTable[index][nybble].diff = diff;
            SosDecompTable[index][nybble].index = next_index;
        }
    }
}

/* Template version of sosCODECDecompressData which generates a single version
   for each possible case.  This means:
    (STEREO, 16Bits),
    (STEREO, 8Bits),
    (MONO, 16Bits),
    (MONO, 8Bits).  */
template <bool STEREO, bool BITS_8>
static unsigned sosCODECDecompressDataTemplate(_SOS_COMPRESS_INFO* stream, unsigned bytes)
{

    /* This macro encapsulates the loop that should decompress the samples.  It is
     invoked in multiple locations so it is nice to have it factored out
     somewhere.  */
#define SOS_DECOMP_LOOP(src, dst, bytes, index, sample)                                                                \
    do {                                                                                                               \
        const int STREAM_BYTES = STEREO ? 2 : 1;                                                                       \
        for (int _i = 0; _i < bytes; _i++) {                                                                           \
            unsigned char codebuf = *src;                                                                              \
            src += STREAM_BYTES;                                                                                       \
                                                                                                                       \
            /* First step: case dwSampleIndex is even (unrolled).  */                                                  \
            char current_nybble = codebuf & 0xF;                                                                       \
                                                                                                                       \
            sample += SosDecompTable[index][current_nybble].diff;                                                      \
            sample = clamp(sample, -32768, 32767);                                                                     \
                                                                                                                       \
            if (BITS_8) {                                                                                              \
                *dst = ((sample & 0xFF00) >> 8) ^ 0x80;                                                                \
                dst = (short*)((char*)(dst) + STREAM_BYTES);                                                           \
            } else {                                                                                                   \
                *dst = sample;                                                                                         \
                dst += STREAM_BYTES;                                                                                   \
            }                                                                                                          \
                                                                                                                       \
            index = SosDecompTable[index][current_nybble].index;                                                       \
                                                                                                                       \
            /* Second step: case dwSampleIndex is odd (unrolled).  */                                                  \
            current_nybble = codebuf >> 4;                                                                             \
            sample += SosDecompTable[index][current_nybble].diff;                                                      \
            sample = clamp(sample, -32768, 32767);                                                                     \
                                                                                                                       \
            if (BITS_8) {                                                                                              \
                *dst = ((sample & 0xFF00) >> 8) ^ 0x80;                                                                \
                dst = (short*)((char*)(dst) + STREAM_BYTES);                                                           \
            } else {                                                                                                   \
                *dst = sample;                                                                                         \
                dst += STREAM_BYTES;                                                                                   \
            }                                                                                                          \
                                                                                                                       \
            index = SosDecompTable[index][current_nybble].index;                                                       \
        }                                                                                                              \
    } while (0);

    unsigned full_length = bytes;
    bytes = BITS_8 ? (bytes / 2) : (bytes / 4);

    /* Quickly return if we are not going to write anything.  */
    if (bytes == 0) {
        return full_length;
    }

    unsigned char* src = (unsigned char*)stream->lpSource;
    short* dst = (short*)(stream->lpDest);

    short index = stream->wIndex;
    int sample = stream->dwPredicted;

    SOS_DECOMP_LOOP(src, dst, bytes, index, sample);

    /* Write back the important stuff from the loop back to the struct.  */
    stream->dwPredicted = sample;
    stream->wIndex = index;

    /* In the stereo case we have to decompress the other channel.  */
    if (STEREO) {
        /* Load important stuff from the second part.  */
        index = stream->wIndex2;
        sample = stream->dwPredicted2;

        src = (unsigned char*)stream->lpSource + 1;

        if (BITS_8) {
            dst = (short*)(stream->lpDest + 1);
        } else {
            dst = (short*)(stream->lpDest) + 1;
        }

        SOS_DECOMP_LOOP(src, dst, bytes, index, sample);

        /* Write back the important stuff from the loop back to the struct.  */
        stream->dwPredicted2 = sample;
        stream->wIndex2 = index;
    }

    return full_length;
#undef SOS_DECOMP_LOOP
}

//
// decompress data from a 4:1 ADPCM compressed file.  the number of
// bytes decompressed is returned.
//
//
unsigned sosCODECDecompressData(_SOS_COMPRESS_INFO* stream, unsigned bytes)
{
    if (SosDecompTableGenerated == false) {
        sosCODECGenerateDecompressTable();
        SosDecompTableGenerated = true;
    }

    if (stream->wBitSize == 16 && stream->wChannels == 1) {
        return sosCODECDecompressDataTemplate<false, false>(stream, bytes);
    } else if (stream->wBitSize == 16 && stream->wChannels == 2) {
        return sosCODECDecompressDataTemplate<true, false>(stream, bytes);
    }
#if 0 // No video or audio sample with this option?
    else if (stream->wBitSize == 8 && stream->wChannels == 1) {
        return sosCODECDecompressDataTemplate<false, true>(stream, bytes);
    } else if (stream->wBitSize == 8 && stream->wChannels == 2) {
        return sosCODECDecompressDataTemplate<true, true>(stream, bytes);
    }
#endif
    assert(0 && "Unreachable");
}

//
// Compresses a data stream into 4:1 ADPCM.  16 bit data is compressed 4:1
// 8 bit data is compressed 2:1.
//
unsigned int sosCODECCompressData(_SOS_COMPRESS_INFO* stream, unsigned int bytes)
{
    int delta;
    int tmp_step;
    short code;
    unsigned tmp;
    unsigned step;
    short current_samp;

    int samples = stream->wBitSize == 16 ? bytes >> 1 : bytes;
    stream->dwSampleIndex = 0;
    stream->dwSampleIndex2 = 0;
    short* src = (short*)(stream->lpSource);
    char* dst = stream->lpDest;

    if (stream->wChannels == 2) {
        // Compress a stereo data stream.
        for (int i = samples; i > 0; i -= 2) {
            if (stream->wBitSize == 16) {
                current_samp = *src;
                src += 2;
            } else {
                current_samp = *src;
                ++src;
                current_samp = (current_samp & 0xFF00) ^ 0x8000;
            }

            delta = current_samp - (stream->dwPredicted);
            code = 0;

            if (delta < 0) {
                delta = -delta;
                code = 8;
            }

            stream->wCode = code;
            tmp_step = stream->wStep;
            tmp = 4;

            for (int i = 3; i > 0; --i) {
                if (delta >= tmp_step) {
                    stream->wCode |= tmp;
                    delta -= tmp_step;
                }

                tmp_step = tmp_step >> 1;
                tmp >>= 1;
            };

            stream->dwDifference = delta;

            if (stream->dwSampleIndex & 1) {
                *dst = stream->wCodeBuf | (stream->wCode << 4);
                dst += 2;
            } else {
                stream->wCodeBuf = stream->wCode & 0xF;
            }

            step = stream->wStep;
            code = stream->wCode;
            stream->dwDifference = step >> 3;

            if (code & 4) {
                stream->dwDifference += step;
            }

            if (code & 2) {
                stream->dwDifference += step >> 1;
            }

            if (code & 1) {
                stream->dwDifference += step >> 2;
            }

            if (code & 8) {
                stream->dwDifference = -stream->dwDifference;
            }

            stream->dwPredicted = clamp(stream->dwDifference + stream->dwPredicted, -32768, 32767);
            stream->wIndex += wCODECIndexTab[stream->wCode];
            stream->wIndex = clamp(stream->wIndex, 0, 88);
            ++stream->dwSampleIndex;
            stream->wStep = wCODECStepTab[stream->wIndex];
        }

        src = (short*)(stream->lpSource + 1);
        dst = stream->lpDest + 1;

        if (stream->wBitSize == 16) {
            src = (short*)(stream->lpSource) + 1;
        }

        for (int i = samples; i > 0; i -= 2) {
            if (stream->wBitSize == 16) {
                current_samp = *src;
                src += 2;
            } else {
                current_samp = *src;
                ++src;
                current_samp = (current_samp & 0xFF00) ^ 0x8000;
            }

            delta = current_samp - (stream->dwPredicted2);
            code = 0;

            if (delta < 0) {
                delta = -delta;
                code = 8;
            }

            stream->wCode2 = code;
            tmp_step = stream->wStep2;
            tmp = 4;

            for (int i = 3; i > 0; --i) {
                if (delta >= tmp_step) {
                    stream->wCode2 |= tmp;
                    delta -= tmp_step;
                }

                tmp_step >>= 1;
                tmp >>= 1;
            };

            stream->dwDifference2 = delta;

            if (stream->dwSampleIndex2 & 1) {
                *dst = stream->wCodeBuf2 | (stream->wCode2 << 4);
                dst += 2;
            } else {
                stream->wCodeBuf2 = stream->wCode2 & 0xF;
            }

            step = stream->wStep2;
            code = stream->wCode2;
            stream->dwDifference2 = step >> 3;

            if (code & 4) {
                stream->dwDifference2 += step;
            }

            if (code & 2) {
                stream->dwDifference2 += step >> 1;
            }

            if (code & 1) {
                stream->dwDifference2 += step >> 2;
            }

            if (code & 8) {
                stream->dwDifference2 = -stream->dwDifference2;
            }

            stream->dwPredicted2 = clamp(stream->dwDifference2 + stream->dwPredicted2, -32768, 32767);
            stream->wIndex2 += wCODECIndexTab[stream->wCode2];
            stream->wIndex2 = clamp(stream->wIndex2, 0, 88);
            ++stream->dwSampleIndex2;
            stream->wStep2 = wCODECStepTab[stream->wIndex2];
        }
    } else {
        // Compress a mono data stream.
        for (int i = samples; i > 0; --i) {
            if (stream->wBitSize == 16) {
                current_samp = *src;
                ++src;
            } else {
                current_samp = *src;
                src = (short*)((char*)(src) + 1);
                current_samp = (current_samp & 0xFF00) ^ 0x8000;
            }

            delta = current_samp - (stream->dwPredicted);
            code = 0;

            if (delta < 0) {
                delta = -delta;
                code = 8;
            }

            stream->wCode = code;
            tmp_step = stream->wStep;
            tmp = 4;

            for (int i = 3; i > 0; --i) {
                if (delta >= tmp_step) {
                    stream->wCode |= tmp;
                    delta -= tmp_step;
                }

                tmp_step >>= 1;
                tmp >>= 1;
            }

            stream->dwDifference = delta;

            if (stream->dwSampleIndex & 1) {
                *dst++ = stream->wCodeBuf | (stream->wCode << 4);
            } else {
                stream->wCodeBuf = stream->wCode & 0xF;
            }

            step = stream->wStep;
            code = stream->wCode;
            stream->dwDifference = step >> 3;

            if (code & 4) {
                stream->dwDifference += step;
            }

            if (code & 2) {
                stream->dwDifference += step >> 1;
            }

            if (code & 1) {
                stream->dwDifference += step >> 2;
            }

            if (code & 8) {
                stream->dwDifference = -stream->dwDifference;
            }

            stream->dwPredicted = clamp(stream->dwDifference + stream->dwPredicted, -32768, 32767);
            stream->wIndex += wCODECIndexTab[stream->wCode];
            stream->wIndex = clamp(stream->wIndex, 0, 88);
            ++stream->dwSampleIndex;
            stream->wStep = wCODECStepTab[stream->wIndex];
        }
    }

    return stream->wBitSize == 16 ? bytes >> 2 : bytes >> 1;
}
