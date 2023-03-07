#include "auduncmp.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

static const signed char ZapTabTwo[4] = {-2, -1, 0, 1};
static const signed char ZapTabFour[16] = {-9, -8, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 8};

#ifndef clamp
static int clamp(int x, int low, int high)
{
    return ((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x));
}
#endif

void Dump_Source(void *src, int size)
{
    unsigned char *bsrc = (unsigned char *) src;
    FILE *f = fopen("/tmp/dump.bin", "a");
    assert(f);

    while (size-- > 0) {
      fputc(*bsrc++, f);
    }
    fclose(f);
}

void Dump_Dest(void *src, int size)
{
    unsigned char *bsrc = (unsigned char *) src;
    FILE *f = fopen("/tmp/dump_out.bin", "wb");
    assert(f);

    while (size-- > 0) {
      fputc(*bsrc++, f);
    }
    fclose(f);
}

void* Read_Dump(const char *filename, int *size)
{
  FILE *f = fopen(filename, "r");
  assert(f);

  fseek(f, 0L, SEEK_END);
  *size = ftell(f);
  fseek(f, 0L, SEEK_SET);

  void *buff = malloc(*size);
  fread(buff, *size, 1, f);

  fclose(f);

  return buff;
}

short Audio_Unzap(void* source, void* dest, short size)
{
    short sample;
    unsigned char code;
    signed char count;
    unsigned short shifted;

    sample = 0x80; //-128
    unsigned char* src = (unsigned char*)(source);
    unsigned char* dst = (unsigned char*)(dest);
    unsigned short remaining = size;

    while (remaining > 0) { // expecting more output
        shifted = *src++;
        shifted <<= 2;
        code = (shifted & 0xFF00) >> 8;
        count = (shifted & 0x00FF) >> 2;

        switch (code) {
        case 2: // no compression...
            if (count & 0x20) {
                count <<= 3;          // here it's significant that (count) is signed:
                sample += count >> 3; // the sign bit will be copied by these shifts!
                *dst++ = clamp(sample, 0, 255);
                remaining--; // one byte added to output
            } else {
                for (++count; count > 0; --count) {
                    --remaining;
                    *dst++ = *src++;
                }

                sample = *(src - 1); // set (sample) to the last byte sent to output
            }
            break;

        case 1:                                 // ADPCM 8-bit -> 4-bit
            for (++count; count > 0; --count) { // decode (count+1) bytes
                code = *src++;
                sample += ZapTabFour[(code & 0x0F)]; // lower nibble
                *dst++ = clamp(sample, 0, 255);
                sample += ZapTabFour[(code >> 4)]; // higher nibble
                *dst++ = clamp(sample, 0, 255);
                remaining -= 2; // two bytes added to output
            }
            break;

        case 0:                                 // ADPCM 8-bit -> 2-bit
            for (++count; count > 0; --count) { // decode (count+1) bytes
                code = *src++;
                sample += ZapTabTwo[(code & 0x03)]; // lower 2 bits
                *dst++ = clamp(sample, 0, 255);
                sample += ZapTabTwo[((code >> 2) & 0x03)]; // lower middle 2 bits
                *dst++ = clamp(sample, 0, 255);
                sample += ZapTabTwo[((code >> 4) & 0x03)]; // higher middle 2 bits
                *dst++ = clamp(sample, 0, 255);
                sample += ZapTabTwo[((code >> 6) & 0x03)]; // higher 2 bits
                *dst++ = clamp(sample, 0, 255);
                remaining -= 4; // 4 bytes sent to output
            }
            break;

        default: // just copy (sample) (count+1) times to output
            memset(dst, clamp(sample, 0, 255), ++count);
            remaining -= count;
            dst += count;
            break;
        }
    }

    return size - remaining;
}

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define ABS(x) ((x) < 0) ? -(x) : (x);

enum COMP_TYPE {
  NO_COMP = 0,
  COMP_4BITS = 1,
  COMP_2BITS = 2,
  COMP_COUNT = 3,
};

signed char Find_Nearest_Value(const signed char *table, int n, int x)
{
  short key, value = UINT16_MAX, i;

  for (i = 0; i < n; i++) {
    short a = ABS(table[i] - x);
    if (a < value) {
      value = a;
      key = i;
    }
  }

  return key;
}

#define FIND_NEAREST_VALUE(v, x) Find_Nearest_Value((v), (sizeof(v)/sizeof(*v)), (x))

short Predict_Compressed_Value(int prev_sample, int curr_sample, COMP_TYPE comp)
{
  short key;
  int f;

  switch (comp) {
    case NO_COMP:
      return clamp(curr_sample, 0, 255);
      break;

    case COMP_4BITS:
      f = curr_sample - prev_sample;
      key = FIND_NEAREST_VALUE(ZapTabFour, f);
      return clamp(prev_sample + ZapTabFour[key], 0, 255);

      break;

    case COMP_2BITS:
      f = curr_sample - prev_sample;
      key = FIND_NEAREST_VALUE(ZapTabTwo, f);
      return clamp(prev_sample + ZapTabTwo[key], 0, 255);

      break;

    default:
      assert(0);
      break;
  }
}


static void Decide_Compression(unsigned char *src, short count)
{

                        /*previous    current*/
  short pred_table[count][COMP_COUNT][COMP_COUNT];

  for (short previous = 0; previous < COMP_COUNT; previous++) {
    for (short current = 0; current < COMP_COUNT; current++) {
      short pred = Predict_Compressed_Value(0x80, src[0], (COMP_TYPE)current);
      pred_table[0][previous][current] = pred;
    }
  }

  for (short i = 1; i < count; i++) {
    for (short previous = 0; previous < COMP_COUNT; previous++) {
      for (short current = 0; current < COMP_COUNT; current++) {
        pred_table[i][previous][current] = Predict_Compressed_Value(pred_table[i-1][previous][current], src[i], (COMP_TYPE) current);
      }
    }
  }
}

short Audio_Zap(void *source, void *dest, short size)
{
    unsigned char* src = (unsigned char*)(source);
    unsigned char* dst = (unsigned char*)(dest);
    unsigned short remaining = size;

    unsigned short code;
    short count;

    short written = 0;

    for (short i = 0; i < size; i += count) {
        code = 2; //no compression.
        count = MIN(32, size - i);

        // count-1 comes from the fact that the decoder always add 1 before the
        // copy loop starts.
        unsigned char header = ((code << 8) | (((count - 1) << 2) & 0xFF)) >> 2;
        *dst++ = header;

        for (short j = 0; j < count; j++) {
            *dst++ = *src++;
        }

        written += count + 1;
    }

    return written;
}

int main()
{
  const char *dump_in = "dump.bin";
  const char *dump_out = "dump_out.bin";

  int ssize;
  void *source = Read_Dump(dump_in, &ssize);

  char dest[16000];
  int dsize = Audio_Unzap(source, (void*) dest, 8192);

  Decide_Compression((unsigned char *)dest, 8192);

  char dest2[16000];
  int ddsize = Audio_Zap((void *)dest, (void *)dest2, dsize);

  dsize = Audio_Unzap((void*) dest2, (void *)dest, 8192);

  Dump_Dest((void*) dest, dsize);

  free(source);
}
