#ifndef AUDIO_FIFOCOMMON
#define AUDIO_FIFOCOMMON

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
        MIC_STOP = 10 << 20
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
        SDMMC_NAND_WRITE_SECTORS
    } FifoSoundMessageType;

    typedef struct FifoMessage
    {
        u16 type;

        union
        {

            struct
            {
                const void* data;
                u32 handle;
                u16 freq;
                u8 volume;
                u8 pan;
                u8 priority;
            } SoundPlay;
        };

    } ALIGN(4) FifoSoundMessage;
} // namespace USR1

#endif //AUDIO_FIFOCOMMON
