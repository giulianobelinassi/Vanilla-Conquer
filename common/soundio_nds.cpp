#include "audio.h"
#include "memflag.h"
#include <cstdio>
#include <limits.h>
#include <nds/arm9/sound.h>
#include <nds/fifocommon.h>
#include "audio_fifocommon.h"

#define CALLED printf("%s called\n", __func__)

void pause(const char*, ...);

enum
{
    AUD_CHUNK_MAGIC_ID = 0x0000DEAF,
    VOLUME_MIN = 0,
    VOLUME_MAX = 255,
    PRIORITY_MIN = 0,
    PRIORITY_MAX = 255,
    MAX_SAMPLE_TRACKERS = 5, // C&C issue where sounds get cut off is because of the small number of trackers.
    STREAM_BUFFER_COUNT = 16,
    BUFFER_CHUNK_SIZE = 8192, // 256 * 32,
    UNCOMP_BUFFER_SIZE = 2098,
    BUFFER_TOTAL_BYTES = BUFFER_CHUNK_SIZE * 4, // 32 kb
    TIMER_DELAY = 25,
    TIMER_RESOLUTION = 1,
    TIMER_TARGET_RESOLUTION = 10, // 10-millisecond target resolution
    INVALID_AUDIO_HANDLE = -1,
    INVALID_FILE_HANDLE = -1,
};

/*
** Define the different type of sound compression avaliable to the westwood
** library.
*/
typedef enum
{
    SCOMP_NONE = 0,     // No compression -- raw data.
    SCOMP_WESTWOOD = 1, // Special sliding window delta compression.
    SCOMP_SOS = 99      // SOS frame compression.
} SCompressType;

// Everything down there is present to conform to the game's API.

void (*Audio_Focus_Loss_Function)(void) = nullptr;
bool StreamLowImpact = false;

SFX_Type SoundType;
Sample_Type SampleType;
static int DigiHandle = INVALID_AUDIO_HANDLE;
static bool AudioInitialized = false;

int File_Stream_Sample(char const* filename, bool real_time_start)
{
    CALLED;
    return 1;
};
int File_Stream_Sample_Vol(char const* filename, int volume, bool real_time_start)
{
    CALLED;
    return 1;
};
void Sound_Callback(void){};
void maintenance_callback(void){};
void* Load_Sample(char const* filename)
{
    CALLED;
    return nullptr;
};
long Load_Sample_Into_Buffer(char const* filename, void* buffer, long size)
{
    CALLED;
    return 0;
}
long Sample_Read(int fh, void* buffer, long size)
{
    CALLED;
    return 0;
};
void Free_Sample(void const* sample){};
bool Audio_Init(int bits_per_sample, bool stereo, int rate, bool reverse_channels)
{
    CALLED;

    // Initialize Nintendo DS sound system.
    soundEnable();

    // Set Global structures required by game's API.
    SoundType = SFX_ALFX;
    SampleType = SAMPLE_SB;
    DigiHandle = 1;
    AudioInitialized = true;

    return true;
};
void Sound_End(void){};
void Stop_Sample(int handle){};
bool Sample_Status(int handle)
{
    CALLED;
    return 0;
};
bool Is_Sample_Playing(void const* sample)
{
    return false;
};
void Stop_Sample_Playing(void const* sample)
{
    CALLED;
};
int Play_Sample(void const* sample, int priority, int volume, signed short panloc)
{
    static int current_handle = 0;
    int handle = current_handle++;
    USR1::FifoMessage msg;

    msg.type = USR1::SOUND_PLAY_MESSAGE;
    msg.SoundPlay.priority = priority;
    msg.SoundPlay.handle = handle;
    msg.SoundPlay.data = sample;
    msg.SoundPlay.volume = volume / 2;
    msg.SoundPlay.pan = ((int)panloc + 32767) / 517;

    // Asynchronous send sound play command
    fifoSendDatamsg(FIFO_USER_01, sizeof(msg), (unsigned char*)&msg);

    return handle;
}

int Play_Sample_Handle(void const* sample, int priority, int volume, signed short panloc, int id)
{
    CALLED;
    return -1;
};
int Set_Sound_Vol(int volume)
{
    CALLED;
    return 127;
};
int Set_Score_Vol(int volume)
{
    CALLED;
    return 127;
};
void Fade_Sample(int handle, int ticks){};
int Get_Free_Sample_Handle(int priority)
{
    CALLED;
    return 1;
};
int Get_Digi_Handle(void)
{
    CALLED;
    return DigiHandle;
}
long Sample_Length(void const* sample)
{
    CALLED;
    return 0;
};
void Restore_Sound_Buffers(void){};
bool Set_Primary_Buffer_Format(void)
{
    CALLED;
    return 0;
};
bool Start_Primary_Sound_Buffer(bool forced)
{
    CALLED;
    return 0;
};
void Stop_Primary_Sound_Buffer(void)
{
    CALLED;
}
