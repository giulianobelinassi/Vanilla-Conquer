#ifndef AUDIO_FIFOCOMMON
#define AUDIO_FIFOCOMMON

#define MUSIC_CHUNK_SIZE 32768

// USR1: ARM9 to ARM7
namespace USR1
{

    //! Enum values for the fifo sound commands.
    typedef enum
    {
        SOUND_SET_PAN = 0 << 20,
        SOUND_SET_VOLUME = 1 << 20,
        SOUND_SET_FREQ = 2 << 20,
        SOUND_SET_WAVEDUTY = 3 << 20,
        SOUND_MASTER_ENABLE = 4 << 20,
        SOUND_MASTER_DISABLE = 5 << 20,
        SOUND_PAUSE = 6 << 20,
        SOUND_RESUME = 7 << 20,
        SOUND_KILL = 8 << 20,
        SOUND_SET_MASTER_VOL = 9 << 20,
        MIC_STOP = 10 << 20,
        MUSIC_CHUNK_UPDATED = 11 << 20,
        STOP_SAMPLE_HANDLE = 12 << 20,
        STOP_SAMPLE = 13 << 20,
    } FifoSoundCommand;

    typedef enum
    {
        SOUND_PLAY_MESSAGE = 0x1234,
        SOUND_PSG_MESSAGE,
        SOUND_NOISE_MESSAGE,
        MIC_RECORD_MESSAGE,
        MIC_BUFFER_FULL_MESSAGE,
        SYS_INPUT_MESSAGE,
        SDMMC_SD_READ_SECTORS,
        SDMMC_SD_WRITE_SECTORS,
        SDMMC_NAND_READ_SECTORS,
        SDMMC_NAND_WRITE_SECTORS,
        SOUND_VQA_MESSAGE,
        MUSIC_CHUNK_MESSAGE,
    } FifoSoundMessageType;

    typedef struct FifoMessage
    {
        u16 type;

        union
        {
            struct
            {
                const void* data;
                u16 handle;
                u16 freq;
                u8 volume;
                u8 pan;
                u8 priority;
                u8 hwuncompress : 1;
                u8 is_music : 1;
            } SoundPlay;

            struct
            {
                const void* data;
                u32 size;
                u16 freq;
                u8 volume;
                u8 bits;
            } SoundVQAChunk;
        };

    } ALIGN(4) FifoSoundMessage;
} // namespace USR1

// USR2: ARM7 to ARM9
namespace USR2
{
    //! Enum values for the fifo sound commands.
    typedef enum
    {
        MUSIC_REQUEST_CHUNK = 1 << 20,
    } SoundMusicChunk;
} // namespace USR2

#endif //AUDIO_FIFOCOMMON
