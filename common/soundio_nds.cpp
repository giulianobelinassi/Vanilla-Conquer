#include "audio.h"
#include "memflag.h"
#include "file.h"
#include <cstdio>
#include <limits.h>
#include <cstdlib>
#include <nds/arm9/sound.h>
#include <nds/fifocommon.h>
#include <nds/arm9/cache.h>
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

inline u16 Get_Next_Handle()
{
    static u16 handle = 0;
    return ++handle;
}

// Everything down there is present to conform to the game's API.

void (*Audio_Focus_Loss_Function)(void) = nullptr;
bool StreamLowImpact = false;

SFX_Type SoundType;
Sample_Type SampleType;
static int DigiHandle = INVALID_AUDIO_HANDLE;
static bool AudioInitialized = false;

class MusicBuffer
{
public:
    MusicBuffer()
    {
        memset(this, 0, sizeof(*this));
        Handle = -1;
        Volume = 200;
    }

    inline int Set_Volume(int volume)
    {
        int oldvol = Volume;
        Volume = (u8)volume;
        return oldvol;
    }

    inline bool Is_Music_Handle(u16 handle)
    {
        if (handle == Handle)
            return true;

        return false;
    }

    inline bool Sample_Status()
    {
        return IsPlaying;
    }

    inline int Set_File_Stream(const char* filename, unsigned char volume)
    {
        int bytes;

        // If volume is too low then disable music to save resources
        if (Volume < 2) {
            printf("!! Volume too low!\n");
            return INVALID_AUDIO_HANDLE;
        }

        FileHandle = Open_File(filename, 1);
        if (FileHandle == INVALID_FILE_HANDLE) {
            return INVALID_AUDIO_HANDLE;
        }

        if (Buffer == NULL) {
            Buffer = (char*)malloc(2 * MUSIC_CHUNK_SIZE);
        }

        if (Buffer == NULL) {
            printf("Failure allocating music buffer\n");
            while (1)
                ;
        }

        //memset(Buffer, 0, 2 * MUSIC_CHUNK_SIZE);

        bytes = Read_File(FileHandle, Buffer, 2 * MUSIC_CHUNK_SIZE);
        if (bytes == 0) {
            IsPlaying = false;
            return INVALID_AUDIO_HANDLE;
        }

        ToUpdate = 0;
        IsPlaying = true;
        HasMoreSource = true;
        Handle = Get_Next_Handle();
        Send_Chunk();
        return Handle;
    }

    inline int Stop_File_Stream()
    {
        ShouldBeUpdated = 0;
        IsPlaying = false;
        HasMoreSource = false;
        if (FileHandle >= 0)
            Close_File(FileHandle);
        FileHandle = -1;
        return 0;
    }

    inline void Mark_For_Update()
    {
        ShouldBeUpdated++;
    }

    inline void Update_File_Stream()
    {
        if (ShouldBeUpdated == 0)
            return;

        ShouldBeUpdated--;

        if (!Buffer || !IsPlaying)
            return;

        unsigned char* to_update = (unsigned char*)Buffer + ToUpdate * MUSIC_CHUNK_SIZE;
        if (HasMoreSource) {
            int bytes = Read_File(FileHandle, to_update, MUSIC_CHUNK_SIZE);
            if (bytes == 0) {
                HasMoreSource = false;
                Close_File(FileHandle);
                FileHandle = -1;
            }
        } else {
            IsPlaying = false;
        }

        ToUpdate = (ToUpdate + 1) % 2;
    }

    inline void Send_Chunk()
    {
        if (!Buffer)
            return;

        USR1::FifoMessage msg;

        msg.type = USR1::SOUND_PLAY_MESSAGE;
        msg.SoundPlay.priority = 255;
        msg.SoundPlay.handle = Handle;
        msg.SoundPlay.data = Buffer;
        msg.SoundPlay.volume = Volume;
        msg.SoundPlay.pan = 64;
        msg.SoundPlay.hwuncompress = 0;
        msg.SoundPlay.is_music = true;

        // Asynchronous send sound play command
        fifoSendDatamsg(FIFO_USER_01, sizeof(msg), (u8*)&msg);
    }

private:
    char* Buffer;
    int FileHandle;
    bool IsPlaying;
    int ToUpdate;
    int ShouldBeUpdated;
    u16 Handle;
    unsigned char Volume;
    bool HasMoreSource;
};

static MusicBuffer MBuffer;

void user02CommandHandler(u32 command, void* userdata)
{
    int cmd = (command)&0x00F00000;
    int data = command & 0xFFFF;
    int channel = (command >> 16) & 0xF;

    switch (cmd) {

    case USR2::MUSIC_REQUEST_CHUNK:
        MBuffer.Mark_For_Update();
        break;

    default:
        break;
    }
}

int File_Stream_Sample(char const* filename, bool real_time_start)
{
    CALLED;
    return File_Stream_Sample_Vol(filename, 255, real_time_start);
};
int File_Stream_Sample_Vol(char const* filename, int volume, bool real_time_start)
{
    CALLED;
    return MBuffer.Set_File_Stream(filename, volume);
};
void Sound_Callback(void)
{
    MBuffer.Update_File_Stream();
}

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

void Free_Sample(void const* sample)
{
    CALLED;
};

bool Audio_Init(int bits_per_sample, bool stereo, int rate, bool reverse_channels)
{
    CALLED;

    // Initialize Nintendo DS sound system.
    soundEnable();

    // Install ARM7 to ARM9 Queue, used to request music data.
    fifoSetValue32Handler(FIFO_USER_02, user02CommandHandler, 0);

    // Set Global structures required by game's API.
    SoundType = SFX_ALFX;
    SampleType = SAMPLE_SB;
    DigiHandle = 1;
    AudioInitialized = true;

    return true;
};
void Sound_End(void)
{
    CALLED;
}
void Stop_Sample(int handle)
{
    if (MBuffer.Is_Music_Handle(handle)) {
        MBuffer.Stop_File_Stream();
    }

    unsigned command = USR1::STOP_SAMPLE_HANDLE | ((u32)handle & 0xFFFF);
    fifoSendValue32(FIFO_USER_01, command);
}
bool Sample_Status(int handle)
{
    if (MBuffer.Is_Music_Handle(handle))
        return MBuffer.Sample_Status();

    return false;
};
bool Is_Sample_Playing(void const* sample)
{
    /* Don't implement that.  It is called constantly and may flood the ARM7
       with messages more than we already is.  */
    return false;
};
void Stop_Sample_Playing(void const* sample)
{
    unsigned command = USR1::STOP_SAMPLE | ((u32)sample & 0xFFFF);
    fifoSendValue32(FIFO_USER_01, command);
};
int Play_Sample(void const* sample, int priority, int volume, signed short panloc, bool hwuncompress)
{
    u16 handle = Get_Next_Handle();
    USR1::FifoMessage msg;

    msg.type = USR1::SOUND_PLAY_MESSAGE;
    msg.SoundPlay.priority = priority;
    msg.SoundPlay.handle = handle;
    msg.SoundPlay.data = sample;
    msg.SoundPlay.volume = volume / 2;
    msg.SoundPlay.pan = ((int)panloc + 32767) / 517;
    msg.SoundPlay.hwuncompress = (char)(hwuncompress) ? 1 : 0;
    msg.SoundPlay.is_music = false;

    // Asynchronous send sound play command
    fifoSendDatamsg(FIFO_USER_01, sizeof(msg), (u8*)&msg);

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

void Fade_Sample(int handle, int ticks)
{
    CALLED;
    Stop_Sample(handle);
}

int Get_Free_Sample_Handle(int priority)
{
    CALLED;
    return 1;
}

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
void Restore_Sound_Buffers(void)
{
    CALLED;
}

bool Set_Primary_Buffer_Format(void)
{
    CALLED;
    return 0;
}

bool Start_Primary_Sound_Buffer(bool forced)
{
    CALLED;
    return 0;
}

void Stop_Primary_Sound_Buffer(void)
{
    CALLED;
}
