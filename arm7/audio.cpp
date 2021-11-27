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

#define ARM7

// Not good practice, but who cares.
#include "../common/soscodec.cpp"
#include "../common/auduncmp.cpp"

#define IS_CHANNEL_FREE(i) (!(SCHANNEL_CR(i) & SCHANNEL_ENABLE))
#define VQA_CHANNEL        7

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
    BUFFER_CHUNK_SIZE = 4096, // 256 * 32,
    UNCOMP_BUFFER_SIZE = 2098,
    BUFFER_TOTAL_BYTES = BUFFER_CHUNK_SIZE * 4, // 32 kb
    TIMER_DELAY = 25,
    TIMER_RESOLUTION = 1,
    TIMER_TARGET_RESOLUTION = 10, // 10-millisecond target resolution
    INVALID_AUDIO_HANDLE = -1,
    INVALID_FILE_HANDLE = -1,
    DECOMP_BUFFER_COUNT = 2,
};

class SoundTracker;

static inline int __attribute__((pure)) Get_Channel_Index(SoundTracker*);

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

int Simple_Copy(void**, int*, void**, int*, void*);
int Sample_Copy(SoundTracker*, void**, int*, void**, int*, void*, int, SCompressType, void*, int16_t*);

// Tracker class. Represent a track in game.
class SoundTracker
{
public:
    SoundTracker()
    {
        memset(this, 0, sizeof(*this));
        SoundHandle = 0;
    }

    inline int Get_Channel_Index();

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
        int channel = Get_Channel_Index();
        // If sample is hwuncompressed then it is queued directly to the
        // hardware and there is no need to touch the active variable
        // because we can directly querry the hardware.
        return Active || !IS_CHANNEL_FREE(channel);
    }

    inline void Stop_Sample()
    {
        int channel = Get_Channel_Index();
        SCHANNEL_CR(channel) &= ~SCHANNEL_ENABLE;
        Active = false;
        MoreSource = false;
        OneShot = false;
        QueueBuffer = NULL;
        QueueSize = false;
        Remainder = 0;
        Decomp_Buffer_Index = 0;
        Decomp_Buff_Size[0] = 0;
        Decomp_Buff_Size[1] = 0;
        Sample = NULL;
        OriginalSample = NULL;
        SampleSize = 0;
        IsMusic = false;
        MusicStreamIndex = 0;
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

    int Play_Sample(const void* sample,
                    unsigned char priority,
                    unsigned char volume,
                    unsigned char panloc,
                    u16 handle,
                    bool hwuncompress,
                    bool is_music)
    {
        // Set attributes given by call.
        Priority = priority;
        Volume = volume;
        Panloc = panloc;
        SoundHandle = handle;
        IsMusic = is_music;

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
        OriginalSample = (void*)sample;
        Sample = Add_Long_To_Pointer(sample, sizeof(AUDHeaderType));

        if (IsMusic) {
            QueueBuffer = Add_Long_To_Pointer(sample, MUSIC_CHUNK_SIZE);
            QueueSize = MUSIC_CHUNK_SIZE;
            Remainder = MUSIC_CHUNK_SIZE - sizeof(AUDHeaderType);
        } else {
            Remainder = raw_header.Size;
        }

        if (hwuncompress && Can_Be_Hardware_Decompressed(Compression, Bits)) {
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
        } else if (Compression == SCOMP_WESTWOOD) {
            Volume = Volume / 4; // SCOMP_WESTWOOD seems incorrectly pitched,
                                 // decrease its volume to avoid ear rape.
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
                Remainder = 0;
                QueueBuffer = 0;
                break;
            }
        }

        Decomp_Buffer_Index = 0;
        return Play(false);
    }

    inline u16 Get_Sample_16()
    {
        return (u16)((u32)OriginalSample & 0xFFFF);
    }

    void Request_Music_Data()
    {
        fifoSendValue32(FIFO_USER_02, USR2::MUSIC_REQUEST_CHUNK);
        QueueBuffer = (char*)OriginalSample + MusicStreamIndex * MUSIC_CHUNK_SIZE;
        MusicStreamIndex = (MusicStreamIndex + 1) % 2;
        QueueSize = MUSIC_CHUNK_SIZE;
        //while(!fifoCheckValue32(FIFO_USER_01));
    }

    inline void Update()
    {
        int current_channel = Get_Channel_Index();

        if (!IS_CHANNEL_FREE(current_channel))
            return;

        // If OneShot is enabled, we need to set the tracker as inative else it
        // get stuck because this variable won't get updated again.

        if (OneShot)
            Active = false;

        if (!Active)
            return;

        // We are doing a double buffering.  The previous buffer was already
        // decompressed on Play_Sample, or a previous iteration of Update.
        // But the earlier buffer need update to hold the next uncompressed
        // data

        char to_update = Decomp_Buffer_Index;
        char to_play = (Decomp_Buffer_Index + 1) % DECOMP_BUFFER_COUNT;
        Decomp_Buffer_Index = to_play;

        // Play next buffer, that should've been decompressed already.
        Play(false);

        if (MoreSource) {
            // Update stopped buffer with new data
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

            if (IsMusic && !QueueBuffer) {
                Request_Music_Data();
            }

            if (bytes_read == 0) {
                // The entire sample have been decompressed, no more precessing is
                // neessary.
                MoreSource = false;
            }
        }
    }

    void Print_IsMusic()
    {
        if (IsMusic)
            nocashPrintf("1 ");
        else
            nocashPrintf("0 ");
    }

private:
    unsigned char Decomp_Buff[DECOMP_BUFFER_COUNT][BUFFER_CHUNK_SIZE];

    unsigned char Priority;    // The priority of current sound.
    unsigned char Bits;        // 8 or 16 bits.
    unsigned char Panloc;      // Directional Audio.
    unsigned char Volume;      // Volume of current sound.
    SCompressType Compression; // Compression that this sound data is using.
    int Frequency;             // Frequency of the sample.
    u16 SoundHandle;           // The Nintendo DS sound handle.
    void* Sample;              // Playable sample. May be compressed or not.
    void* OriginalSample;
    int SampleSize; // Size of the Sample.

    int Remainder;     // Number of bytes remaining in the source data
                       // as pointed by the "Source" element.
    void* QueueBuffer; // Pointer to continued sample data.
    int QueueSize;     // Size of queue buffer attached.
    bool MoreSource;   // Indicate that we have more stuff to decompress.
    bool OneShot;

    short Decomp_Buff_Size[DECOMP_BUFFER_COUNT];
    char Decomp_Buffer_Index;

    bool Active;
    bool IsMusic;

    char MusicStreamIndex;

public:
    _SOS_COMPRESS_INFO sosinfo;
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

    inline void Stop_Sample_Handle(u16 handle)
    {
        SoundTracker* st;

        for (int i = 0; i < MAX_SAMPLE_TRACKERS; i++) {
            st = Get_Sample_Tracker(i);

            if (st->Get_Handle() == handle && st->Is_Sample_Playing())
                st->Stop_Sample();
        }
    }

    inline void Stop_Sample(u16 sample)
    {
        SoundTracker* st;

        for (int i = 0; i < MAX_SAMPLE_TRACKERS; i++) {
            st = Get_Sample_Tracker(i);

            if (st->Get_Sample_16() == sample && st->Is_Sample_Playing())
                st->Stop_Sample();
        }
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

    int Play_Sample(void const* sample,
                    int priority,
                    int volume,
                    signed short panloc,
                    u16 handle,
                    bool hwuncompress,
                    bool is_music)
    {
        int free_tracker = Get_Free_Sound_Tracker(priority);
        SoundTracker* st = Get_Sample_Tracker(free_tracker);

        // Stop sound if currently playing
        st->Stop_Sample();
        return st->Play_Sample(sample, priority, volume, panloc, handle, hwuncompress, is_music);
    }

    void Print_Priorities()
    {
        for (int i = 0; i < MAX_SAMPLE_TRACKERS; i++) {
            SoundTracker* st = Get_Sample_Tracker(i);

            nocashPrintf("%d ", (int)st->Get_Priority());
        }
        nocashPrintf("\n");
    }

    void Update_Trackers()
    {
        for (int i = MAX_SAMPLE_TRACKERS - 1; i >= 0; i--) {
            SoundTracker* st = Get_Sample_Tracker(i);
            st->Update();
        }
    }

    void Stop_Trackers()
    {
        for (int i = MAX_SAMPLE_TRACKERS - 1; i >= 0; i--) {
            SoundTracker* st = Get_Sample_Tracker(i);
            st->Stop_Sample();
        }
    }

private:
    SoundTracker Trackers[MAX_SAMPLE_TRACKERS];
};

static SoundTrackers Trackers;

int SoundTracker::Get_Channel_Index()
{
    return ((unsigned long)this - (unsigned long)Trackers.Get_Sample_Tracker(0)) / sizeof(SoundTracker);
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
    int channel = Get_Channel_Index();

    if (hwuncompress) {
        sample = Sample;
        format = DS_Sound_Format(Compression, Bits);
        freq = Frequency;
        size = SampleSize;
        Active = false; // No need to set this flag to true, as we querry
        // directly to the hardware.
    } else {
        // Decomp_Buffer_Index should have been updated by Update and it should
        // point to the correct buffer.
        sample = Decomp_Buff[Decomp_Buffer_Index];
        format = DS_Sound_Format(SCOMP_NONE, Bits);
        size = Decomp_Buff_Size[Decomp_Buffer_Index];
        freq = Frequency;
        if (size == 0) {
            SCHANNEL_CR(channel) &= ~SCHANNEL_ENABLE;
            if (IsMusic && Active) {
                // Request more data so that the ARM9 chip realize that the
                // stream ended, so it can queue the next music.
                Request_Music_Data();
            }
            Active = false;
        } else {
            Active = true;
        }
    }

    if (size > 0) {
        SCHANNEL_SOURCE(channel) = (u32)sample;
        SCHANNEL_REPEAT_POINT(channel) = 0;
        SCHANNEL_LENGTH(channel) = size >> 2;
        SCHANNEL_TIMER(channel) = SOUND_FREQ(freq);
        SCHANNEL_CR(channel) =
            SCHANNEL_ENABLE | SOUND_VOL(volume) | SOUND_PAN(panloc) | (format << 29) | (SOUND_ONE_SHOT);
    }

    return SoundHandle;
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

void Sound_Update()
{
    Trackers.Update_Trackers();
}

// End software decompression code.
//-----------------------------------------------------------------------------

// MESSAGES_MAX must be a power of two.
template <int MESSAGES_MAX> class MessageQueue
{
public:
    MessageQueue()
    {
        memset(this, 0, sizeof(*this));
    }

    int Pop_Message(USR1::FifoMessage* msg)
    {
        if (Is_Empty())
            return 0;

        memcpy(msg, &Messages[Tail], sizeof(*msg));
        Tail = (Tail + 1) % MESSAGES_MAX;
        return 1;
    }

    int Push_Message(USR1::FifoMessage* msg)
    {
        if (Is_Full()) {
            return 0;
        }

        memcpy(&Messages[Head], msg, sizeof(*msg));
        Head = (Head + 1) % MESSAGES_MAX;
        return 1;
    }

private:
    inline bool Is_Full(void)
    {
        return (Tail + 1) % MESSAGES_MAX == Head;
    }

    inline bool Is_Empty(void)
    {
        return Head == Tail;
    }

    USR1::FifoMessage Messages[MESSAGES_MAX];
    unsigned Head, Tail;
};

static MessageQueue<32> MQueue;

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
        Trackers.Stop_Trackers();
        SCHANNEL_CR(VQA_CHANNEL) &= ~SCHANNEL_ENABLE;
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
    case USR1::MUSIC_CHUNK_UPDATED:
        break;

    case USR1::STOP_SAMPLE_HANDLE:
        Trackers.Stop_Sample_Handle(data);

    case USR1::STOP_SAMPLE:
        Trackers.Stop_Sample(data);

    default:
        break;
    }
}

void Process_Queue()
{
    USR1::FifoMessage msg;
    if (MQueue.Pop_Message(&msg) == 0)
        return;

    if (msg.type == USR1::SOUND_PLAY_MESSAGE) {
        const void* sample = msg.SoundPlay.data;
        u16 handle = msg.SoundPlay.handle;
        u8 priority = msg.SoundPlay.priority;
        u8 volume = msg.SoundPlay.volume;
        u8 panloc = msg.SoundPlay.pan;
        u8 hwuncompress = msg.SoundPlay.hwuncompress;
        bool is_music = msg.SoundPlay.is_music;

        Trackers.Play_Sample(sample, priority, volume, panloc, handle, hwuncompress, is_music);
    } else if (msg.type == USR1::SOUND_VQA_MESSAGE) {
        const void* sample = msg.SoundVQAChunk.data;
        u16 freq = msg.SoundVQAChunk.freq;
        u32 size = msg.SoundVQAChunk.size;
        u8 volume = msg.SoundVQAChunk.volume;
        u8 bits = msg.SoundVQAChunk.bits;
        unsigned format = SoundTracker::DS_Sound_Format(SCOMP_NONE, bits);

        SCHANNEL_SOURCE(VQA_CHANNEL) = (u32)sample;
        SCHANNEL_REPEAT_POINT(VQA_CHANNEL) = 0;
        SCHANNEL_LENGTH(VQA_CHANNEL) = size >> 2;
        SCHANNEL_TIMER(VQA_CHANNEL) = SOUND_FREQ(freq);
        SCHANNEL_CR(VQA_CHANNEL) =
            SCHANNEL_ENABLE | SOUND_VOL(volume) | SOUND_PAN(64) | (format << 29) | (SOUND_REPEAT);

        SCHANNEL_REPEAT_POINT(VQA_CHANNEL) = 0;
    }
}

void user01DataHandler(int bytes, void* user_data)
{
    USR1::FifoMessage msg;
    fifoGetDatamsg(FIFO_USER_01, bytes, (u8*)&msg);
    MQueue.Push_Message(&msg);

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
