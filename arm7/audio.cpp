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

// Not good practice, but who cares.
#include "../common/soscodec.cpp"
#include "../common/auduncmp.cpp"

#include "printf.h"

#define IS_CHANNEL_FREE(i) (SCHANNEL_CR(i) & SCHANNEL_ENABLE)

unsigned vblank_count;

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
    MAX_SAMPLE_TRACKERS = 4, // C&C issue where sounds get cut off is because of the small number of trackers.
    STREAM_BUFFER_COUNT = 16,
    BUFFER_CHUNK_SIZE = 256*32, // 256 * 32,
    UNCOMP_BUFFER_SIZE = 2098,
    BUFFER_TOTAL_BYTES = BUFFER_CHUNK_SIZE * 4, // 32 kb
    TIMER_DELAY = 25,
    TIMER_RESOLUTION = 1,
    TIMER_TARGET_RESOLUTION = 10, // 10-millisecond target resolution
    INVALID_AUDIO_HANDLE = -1,
    INVALID_FILE_HANDLE = -1,
    DECOMP_BUFFER_COUNT = 2,
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

class SoundTracker;
static inline int __attribute__((pure)) Get_Channel_Index(SoundTracker*);

int Simple_Copy(void**, int*, void**, int*, void*);
int Sample_Copy(SoundTracker*, void**, int*, void**, int*, void*, int, SCompressType, void*, int16_t*);

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
        return (!IS_CHANNEL_FREE(channel) ||  Active);
    }

    inline void Stop_Sample()
    {
        int channel = Get_Channel_Index(this);
        SCHANNEL_CR(channel) &= ~SCHANNEL_ENABLE;

        //QueueBuffer = NULL;
        //QueueSize = 0;
        MoreSource = false;
        OneShot = false;
        Active = false;
        Decomp_Buffer_Index = 0;
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
        Volume = 255; //volume;
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
        Remainder = raw_header.Size;
        Sample = Add_Long_To_Pointer(sample, sizeof(AUDHeaderType));
        if (false /*Can_Be_Hardware_Decompressed(Compression, Bits)*/) {
            // Yay! just throw this sample to the hardware.
            // Size of sample is the same size reported by the header.
            SampleSize = raw_header.Size;

            return Play(true);
        }

        // Compression is ADPCM so we need to init it's stream info.
        if (Compression == SCOMP_SOS) {
            sosinfo.wChannels = (raw_header.Flags & 1) + 1;
            sosinfo.wBitSize = raw_header.Flags & 2 ? 16 : 8;
            sosinfo.dwCompSize = raw_header.Size;
            sosinfo.dwUnCompSize = raw_header.Size * (sosinfo.wBitSize / 4);
            sosCODECInitStream(&sosinfo);
        }

        for (int i = 0; i < DECOMP_BUFFER_COUNT; i++) {
            int bytes_read = Sample_Copy(this,
                                         &Sample,
                                         &Remainder,
                                         &QueueBuffer,
                                         &QueueSize,
                                         Decomp_Buff[i],
                                         BUFFER_CHUNK_SIZE,
                                         Compression,
                                         nullptr,
                                         nullptr);

            Decomp_Buff_Size[i] = bytes_read;
            if (bytes_read == BUFFER_CHUNK_SIZE) {
                MoreSource = true;
                OneShot = false;
            } else {
                MoreSource = false;
                OneShot = true;
                break;
            }
        }

        Decomp_Buffer_Index = 0;
        Play(false);

        return 0;
    }

    inline void Update()
    {
        int current_channel = Get_Channel_Index(this);

        // Simply return if we have no more audio to uncompress -- we are done.
        if (!Active)
            return;

    //    if (!IS_CHANNEL_FREE(current_channel)) {
    //        return;
    //    }

        if (vblank_count < Frame + 10) {
            return;
        }

        // We are doing a double buffering.  The previous buffer was already
        // decompressed on Play_Sample, or a previous iteration of Update.
        // But the earlier buffer need update to hold the next uncompressed
        // data

        if (MoreSource) {
            char to_update = Decomp_Buffer_Index;
            char to_play = (Decomp_Buffer_Index + 1) % DECOMP_BUFFER_COUNT;
            Decomp_Buffer_Index = to_play;

            // Update stopped buffer with new data.
            int bytes_read = Sample_Copy(this,
                                         &Sample,
                                         &Remainder,
                                         &QueueBuffer,
                                         &QueueSize,
                                         Decomp_Buff[to_update],
                                         BUFFER_CHUNK_SIZE,
                                         Compression,
                                         nullptr,
                                         nullptr);
            Decomp_Buff_Size[to_update] = bytes_read;
            if (bytes_read == 0) {
                // The entire sample have been decompressed, no more precessing is
                // neessary.
                nocashPrintf("Entire sample consumed\n");
                MoreSource = false;
                Active = false;
            }
            nocashPrintf("Decompressed more %d bytes\n", bytes_read);
        }

        for (int j = 0; j < 32; j++)
            nocashPrintf("%x ", Decomp_Buff[Decomp_Buffer_Index][j]);
        nocashPrintf("\n");
        nocashPrintf("About to play remaining buffer %d\n", Decomp_Buffer_Index);
        Play(false);
        //SCHANNEL_CR(current_channel) &= ~SCHANNEL_ENABLE;
        //SCHANNEL_SOURCE(current_channel) = (u32) &Decomp_Buff[Decomp_Buffer_Index][0];
        //SCHANNEL_CR(current_channel) &= SCHANNEL_ENABLE;
        Frame = vblank_count;
    }

    _SOS_COMPRESS_INFO sosinfo;// SOS codec structure.
private:
    unsigned char Decomp_Buff[DECOMP_BUFFER_COUNT][BUFFER_CHUNK_SIZE];
    unsigned char Priority;    // The priority of current sound.
    unsigned char Bits;        // 8 or 16 bits.
    unsigned char Panloc;      // Directional Audio.
    unsigned char Volume;      // Volume of current sound.
    SCompressType Compression; // Compression that this sound data is using.
    int Frequency;             // Frequency of the sample.
    unsigned SoundHandle;      // The Nintendo DS sound handle.
    void* Sample;              // Playable sample. May be compressed or not.
    int SampleSize;            // Size of the Sample.
    int Remainder;             // Number of bytes remaining in the source data
                               // as pointed by the "Source" element.
    void *QueueBuffer;         // Pointer to continued sample data.
    int QueueSize;             // Size of queue buffer attached.
    bool MoreSource;           // Indicate that we have more stuff to decompress.
    bool OneShot;
    bool Active;

    short Decomp_Buff_Size[DECOMP_BUFFER_COUNT];
    char Decomp_Buffer_Index;
    unsigned Frame;
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
        if (st == NULL)
            return -1;

        // Stop sound if currently playing
        st->Stop_Sample();
        return st->Play_Sample(sample, priority, volume, panloc, handle);
    }

    void Update_Trackers()
    {
        for (int i = MAX_SAMPLE_TRACKERS-1; i >= 0; i--) {
            SoundTracker* st = Get_Sample_Tracker(i);
            st->Update();
        }
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
    void* sample;
    SoundFormat format;
    int size;

    unsigned short freq = Frequency;
    unsigned char volume = Volume;
    unsigned char panloc = Panloc;
    int channel = Get_Channel_Index(this);

    if (hwuncompress) {
        sample = Sample;
        format = DS_Sound_Format(Compression, Bits);
        freq = Frequency;
        size = SampleSize;
    }
    else {
        // Decomp_Buffer_Index should have been updated by Update and it should
        // point to the correct buffer.
        sample = Decomp_Buff[Decomp_Buffer_Index];
        format = DS_Sound_Format(SCOMP_NONE, Bits);
        size = Decomp_Buff_Size[Decomp_Buffer_Index];
        freq = Frequency;
    }

    SCHANNEL_SOURCE(channel) = (u32)sample;
    SCHANNEL_REPEAT_POINT(channel) = 0;
    SCHANNEL_LENGTH(channel) = size >> 2;
    SCHANNEL_TIMER(channel) = SOUND_FREQ(freq);
    SCHANNEL_CR(channel) = SCHANNEL_ENABLE | SOUND_VOL(volume) | SOUND_PAN(panloc) | (format << 29) | (SOUND_ONE_SHOT);

    Active = true;
    Frame = vblank_count; 
    return SoundHandle;
}

void Sound_Update()
{
    Trackers.Update_Trackers();
}

// Software audio decompression.  The code is crap as hell...
// ----------------------------------------------------------------------------
int Simple_Copy(void** source, int* ssize, void** alternate, int* altsize, void** dest, int size)
{
    int out = 0;

    if (*ssize == 0) {
        *source = *alternate;
        *ssize = *altsize;
        *alternate = nullptr;
        *altsize = 0;
    }

    if (*source == nullptr || *ssize == 0) {
        return out;
    }

    int s = size;

    if (*ssize < size) {
        s = *ssize;
    }

    memcpy(*dest, *source, s);
    *source = static_cast<char*>(*source) + s;
    *ssize -= s;
    *dest = static_cast<char*>(*dest) + s;
    out = s;

    if ((size - s) == 0) {
        return out;
    }

    *source = *alternate;
    *ssize = *altsize;
    *alternate = nullptr;
    *altsize = 0;

    out = Simple_Copy(source, ssize, alternate, altsize, dest, (size - s)) + s;

    return out;
}

int Sample_Copy(SoundTracker* st,
                void** source,
                int* ssize,
                void** alternate,
                int* altsize,
                void* dest,
                int size,
                SCompressType scomp,
                void* trailer,
                int16_t* trailersize)
{
    unsigned char uncomp_buffer[UNCOMP_BUFFER_SIZE];
    int datasize = 0;

    // There is no compression or it doesn't match any of the supported compressions so we just copy the data over.
    if (scomp == SCOMP_NONE || (scomp != SCOMP_WESTWOOD && scomp != SCOMP_SOS)) {
        return Simple_Copy(source, ssize, alternate, altsize, &dest, size);
    }

    _SOS_COMPRESS_INFO* s = &st->sosinfo;

    while (size > 0) {
        uint16_t fsize;
        uint16_t dsize;
        unsigned magic;

        void* fptr = &fsize;
        void* dptr = &dsize;
        void* mptr = &magic;

        // Verify and seek over the chunk header.
        if (Simple_Copy(source, ssize, alternate, altsize, &fptr, sizeof(fsize)) < sizeof(fsize)) {
            break;
        }

        if (Simple_Copy(source, ssize, alternate, altsize, &dptr, sizeof(dsize)) < sizeof(dsize) || dsize > size) {
            break;
        }

        if (Simple_Copy(source, ssize, alternate, altsize, &mptr, sizeof(magic)) < sizeof(magic)
            || magic != AUD_CHUNK_MAGIC_ID) {
            break;
        }

        if (fsize == dsize) {
            // File size matches size to decompress, so there's nothing to do other than copy the buffer over.
            if (Simple_Copy(source, ssize, alternate, altsize, &dest, fsize) < dsize) {
                return datasize;
            }
        } else {
            // Else we need to decompress it.
            void* uptr = uncomp_buffer; //LockedData.UncompBuffer;
            memset(uncomp_buffer, 0, sizeof(uncomp_buffer));

            if (Simple_Copy(source, ssize, alternate, altsize, &uptr, fsize) < fsize) {
                return datasize;
            }

            if (scomp == SCOMP_WESTWOOD) {
                Audio_Unzap(uncomp_buffer, dest, dsize);
            } else {
                s->lpSource = (char*)uncomp_buffer;
                s->lpDest = (char*)dest;

                sosCODECDecompressData(s, dsize);
            }

            dest = reinterpret_cast<char*>(dest) + dsize;
        }

        datasize += dsize;
        size -= dsize;
    }

    return datasize;
}
// End software decompression code.
//-----------------------------------------------------------------------------

void user01CommandHandler(u32 command, void* userdata)
{

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

    Trackers.Print_Priorities();

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
