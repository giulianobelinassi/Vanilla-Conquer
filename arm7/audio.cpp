/*---------------------------------------------------------------------------------

	Copyright (C) 2008 - 2010
		Dave Murphy  (WinterMute)
		Jason Rogers (Dovoto)

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.
	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.
	3.	This notice may not be removed or altered from any source
		distribution.

---------------------------------------------------------------------------------*/

#include <nds/debug.h>
#include <nds/arm7/audio.h>
#include <nds/system.h>
#include <nds/ipc.h>
#include <nds/fifocommon.h>
#include <nds/system.h>

#include "../common/memflag.h"
#include "../common/audio.h"
#include "../common/audio_fifocommon.h"

#include "printf.h"

#define IS_CHANNEL_FREE(i) (SCHANNEL_CR(i) & SCHANNEL_ENABLE)

class SoundTracker;
static inline int __attribute__((pure)) Get_Channel_Index(SoundTracker*);

typedef enum
{
    SoundFormat_16Bit = 1, /*!<  16-bit PCM */
    SoundFormat_8Bit = 0,  /*!<  8-bit PCM */
    SoundFormat_PSG = 3,   /*!<  PSG (programmable sound generator?) */
    SoundFormat_ADPCM = 2  /*!<  IMA ADPCM compressed audio  */
} SoundFormat;

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

// Tracker class. Represent a track in game.
class SoundTracker
{
public:
    SoundTracker()
    {
        memset(this, 0, sizeof(*this));
        SoundHandle = -1;
    }

    static inline bool Can_Be_Hardware_Decompressed(SCompressType c, unsigned char bits)
    {
        switch (c) {
        case SCOMP_NONE:
        case SCOMP_SOS:
            return true;
            break;

        case SCOMP_WESTWOOD:
            return false;
            break;
        }

        // Should be unreachable, but in any case...
        return false;
    }

    static SoundFormat DS_Sound_Format(SCompressType type, unsigned char bits)
    {
        if (type == SCOMP_SOS) {
            return SoundFormat_ADPCM;
        } else if (bits == 16) {
            return SoundFormat_16Bit;
        }
        return SoundFormat_8Bit;
    }

    inline void* Get_Sample()
    {
        return Sample;
    }

    inline bool Is_Sample_Playing()
    {
        int channel = Get_Channel_Index(this);
        return IS_CHANNEL_FREE(channel);
    }

    inline void Stop_Sample()
    {
        int channel = Get_Channel_Index(this);
        SCHANNEL_CR(channel) &= ~SCHANNEL_ENABLE;
    }

    inline unsigned char Get_Priority()
    {
        return Priority;
    }

    inline unsigned Get_Handle()
    {
        return SoundHandle;
    }

    int Play(bool hwuncompress);

    int
    Play_Sample(const void* sample, unsigned char priority, unsigned char volume, unsigned char panloc, unsigned handle)
    {
        // Set attributes given by call.
        Priority = priority;
        Volume = volume;
        Panloc = panloc;
        SoundHandle = handle;

        // Load the AUD header;
        AUDHeaderType raw_header;
        memcpy(&raw_header, sample, sizeof(raw_header));

        // We don't support anything lower than 20000 hz.
        if (raw_header.Rate < 24000 && raw_header.Rate > 20000) {
            raw_header.Rate = 22050;
        }

        // Get number of bits in sample.
        Bits = (raw_header.Flags & 2) ? 16 : 8;

        // Update Frequency according to header/
        Frequency = raw_header.Rate;

        // Check the Compression.
        Compression = SCompressType(raw_header.Compression);
        if (Can_Be_Hardware_Decompressed(Compression, Bits)) {
            // Yay! just throw this sample to the hardware.

            // Size of sample is the same size reported by the header.
            SampleSize = raw_header.Size;
            Sample = Add_Long_To_Pointer(sample, sizeof(AUDHeaderType));

            return Play(true);
        }
        return 0;
    }

private:
    unsigned char Priority;    // The priority of current sound.
    unsigned char Bits;        // 8 or 16 bits.
    unsigned char Panloc;      // Directional Audio.
    unsigned char Volume;      // Volume of current sound.
    SCompressType Compression; // Compression that this sound data is using.
    int Frequency;             // Frequency of the sample.
    unsigned SoundHandle;      // The Nintendo DS sound handle.
    void* Sample;              // Playable sample. May be compressed or not.
    int SampleSize;            // Size of the Sample.
};

class SoundTrackers
{
public:
    inline SoundTracker* Get_Sample_Tracker(int i)
    {
        return &Trackers[i];
    }

    inline SoundTracker* Get_Tracker_By_Handle(int handle)
    {
        SoundTracker* st;
        for (int i = 0; i < MAX_SAMPLE_TRACKERS; i++) {
            st = Get_Sample_Tracker(i);

            if (st->Is_Sample_Playing() && st->Get_Handle() == handle)
                return st;
        }

        return NULL;
    }

    inline bool Is_Sample_Playing(const void* sample)
    {
        SoundTracker* st;

        for (int i = 0; i < MAX_SAMPLE_TRACKERS; i++) {
            st = Get_Sample_Tracker(i);

            if (st->Get_Sample() == sample && st->Is_Sample_Playing())
                return true;
        }

        return false;
    }

    inline int Get_Free_Sound_Tracker(int priority)
    {
        int i;
        unsigned int min_priority = 255;
        int min_handle = 0;

        // Look in all trackers for a free slot.
        for (i = MAX_SAMPLE_TRACKERS - 1; i >= 0; --i) {
            SoundTracker* st = Get_Sample_Tracker(i);
            unsigned char current_priority = st->Get_Priority();

            if (!st->Is_Sample_Playing() || priority > current_priority) {
                return i;
            }
        }

        // Now that the lowest priority tracker have been found, return it.
        return min_handle;
    }

    int Play_Sample(void const* sample, int priority, int volume, signed short panloc, unsigned handle)
    {
        int free_tracker = Get_Free_Sound_Tracker(priority);
        SoundTracker* st = Get_Sample_Tracker(free_tracker);

        // Stop sound if currently playing
        st->Stop_Sample();
        return st->Play_Sample(sample, priority, volume, panloc, handle);
    }

    void Print_Priorities()
    {
        for (int i = 0; i < MAX_SAMPLE_TRACKERS; i++) {
            SoundTracker* st = Get_Sample_Tracker(i);

            nocashPrintf("%d ", (int)st->Get_Priority());
        }
        nocashPrintf("\n");
    }

private:
    SoundTracker Trackers[MAX_SAMPLE_TRACKERS];
};

static SoundTrackers Trackers;

int Get_Channel_Index(SoundTracker* st)
{
    return ((unsigned long)st - (unsigned long)Trackers.Get_Sample_Tracker(0)) / sizeof(SoundTracker);
}

int SoundTracker::Play(bool hwuncompress)
{
    // Play sound in system.
    void* sample = Sample;
    SoundFormat format = DS_Sound_Format((hwuncompress) ? Compression : SCOMP_NONE, Bits);
    unsigned short freq = Frequency;
    int size = SampleSize;
    unsigned char volume = Volume;
    unsigned char panloc = Panloc;
    int channel = Get_Channel_Index(this);

    SCHANNEL_SOURCE(channel) = (u32)sample;
    SCHANNEL_REPEAT_POINT(channel) = 0;
    SCHANNEL_LENGTH(channel) = size >> 2;
    SCHANNEL_TIMER(channel) = SOUND_FREQ(freq);
    SCHANNEL_CR(channel) = SCHANNEL_ENABLE | SOUND_VOL(volume) | SOUND_PAN(panloc) | (format << 29) | (SOUND_ONE_SHOT);

    return SoundHandle;
}

//---------------------------------------------------------------------------------
void user01CommandHandler(u32 command, void* userdata)
{
    //---------------------------------------------------------------------------------

    int cmd = (command)&0x00F00000;
    int data = command & 0xFFFF;
    int channel = (command >> 16) & 0xF;

    switch (cmd) {

    case USR1::SOUND_MASTER_ENABLE:
        //enableSound();
        break;

    case USR1::SOUND_MASTER_DISABLE:
        //disableSound();
        break;

    case USR1::SOUND_SET_VOLUME:
        SCHANNEL_CR(channel) &= ~0xFF;
        SCHANNEL_CR(channel) |= data;
        break;

    case USR1::SOUND_SET_PAN:
        SCHANNEL_CR(channel) &= ~SOUND_PAN(0xFF);
        SCHANNEL_CR(channel) |= SOUND_PAN(data);
        break;

    case USR1::SOUND_SET_FREQ:
        SCHANNEL_TIMER(channel) = SOUND_FREQ(data);
        break;

    case USR1::SOUND_SET_WAVEDUTY:
        SCHANNEL_CR(channel) &= ~(7 << 24);
        SCHANNEL_CR(channel) |= (data) << 24;
        break;

    case USR1::SOUND_KILL:
        SCHANNEL_CR(channel) &= ~SCHANNEL_ENABLE;
        break;

    case USR1::SOUND_PAUSE:
        SCHANNEL_CR(channel) &= ~SCHANNEL_ENABLE;
        break;

    case USR1::SOUND_RESUME:
        SCHANNEL_CR(channel) |= SCHANNEL_ENABLE;
        break;

    case USR1::MIC_STOP:
        micStopRecording();
        break;

    default:
        break;
    }
}

void user01DataHandler(int bytes, void* user_data)
{
    int channel = -1;

    USR1::FifoMessage msg;

    fifoGetDatamsg(FIFO_USER_01, bytes, (u8*)&msg);

    if (msg.type == USR1::SOUND_PLAY_MESSAGE) {
        const void* sample = msg.SoundPlay.data;
        u32 handle = msg.SoundPlay.handle;
        u8 priority = msg.SoundPlay.priority;
        u8 volume = msg.SoundPlay.volume;
        u8 panloc = msg.SoundPlay.pan;

        channel = Trackers.Play_Sample(sample, priority, volume, panloc, handle);
    }

    // Don't send confirmation -- This engine is asynchronous.
    //fifoSendValue32(FIFO_USER_01, (u32)channel);
}

//---------------------------------------------------------------------------------
void installUser01FIFO(void)
{
    //---------------------------------------------------------------------------------

    fifoSetDatamsgHandler(FIFO_USER_01, user01DataHandler, 0);
    fifoSetValue32Handler(FIFO_USER_01, user01CommandHandler, 0);
}
