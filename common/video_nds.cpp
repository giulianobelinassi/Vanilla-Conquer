
#include "gbuffer.h"
#include "palette.h"
#include "video.h"
#include "video.h"
#include "wwkeyboard.h"
#include "wwmouse.h"
#include <cstdio>
#include <nds.h>
#include <stdarg.h>

/** Video interface for the Nintendo DSi.
  *
  * Author: mrparrot (aka giulianob)
  *
  * This video engine provides a drawable surface to the game's engine by
  * setting up a 512x256 8-bit background. When the game is ready to draw
  * to the screen, the game `Blt` the 'HidBuff' into this final plane
  * (SeenBuff, or frontSurface). We also provide functions for Zooming in
  * and out the screen, as the game expect at least a 320x200 screen, but
  * the DSi resolution is 256x192.  The default mode is use the GPU's
  * affine transformations to 'compress' the background in order to fit
  * into screen, with the consequence of losing a few lines.
  **/

extern "C" void memcpy32(void* dst, const void* src, unsigned int wdcount);
void *tonccpy(void *dst, const void *src, size_t size);

/* Function used to pause the console for debugging.  */
void DS_Pause(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    while (1) {
        swiWaitForVBlank();
        scanKeys();
        int keys = keysDown();
        if (keys & KEY_A)
            break;
    }
}

// Background 3 identifier. Set on video initialization, and used on the seen
// surface buffer.
static int bg3;

// Mark the status of zoom. True means we are zoomed in, false means we are
// zoomed out.
static bool ZoomState = false;

// Zoom in the Seen Surface.
void DS_SeenBuff_ZoomIn(void)
{
    int scale_x = (SCREEN_WIDTH * 256) / SCREEN_WIDTH;
    int scale_y = (SCREEN_HEIGHT * 256) / SCREEN_HEIGHT;

    bgSetRotateScale(bg3, 0, scale_x, scale_y);
    bgSetScroll(bg3, 32, 4);

    ZoomState = true;

    swiWaitForVBlank();
    bgUpdate();
}

// Zoom out the Seen Surface.
void DS_SeenBuff_ZoomOut(void)
{
    int scale_x = (320 * 256) / SCREEN_WIDTH;
    int scale_y = (200 * 256) / SCREEN_HEIGHT;

    bgSetRotateScale(bg3, 0, scale_x, scale_y);
    bgSetScroll(bg3, 0, 0);

    ZoomState = false;

    swiWaitForVBlank();
    bgUpdate();
}

void DS_SeenBuff_SwitchZoom()
{
    if (ZoomState == true) {
        DS_SeenBuff_ZoomOut();
    } else {
        DS_SeenBuff_ZoomIn();
    }
}

/*
 * We implement a 'hardware' cursor by using a sprite.  It looks better than
 * blitting the cursor into the seenbuff.
 */
class HardwareCursor
{
public:
    void Init()
    {

        // Allocate 16Kb for the mouse sprites.
        vramSetBankG(VRAM_G_MAIN_SPRITE_0x06400000);

        // Initialize the sprte engine.
        oamInit(&oamMain, SpriteMapping_1D_256, false);
        Surface = oamAllocateGfx(&oamMain, SpriteSize_32x32, SpriteColorFormat_256Color);

        X = 160;
        Y = 100;

        oamSet(&oamMain,
               0,
               X,
               Y,
               0,
               0,
               SpriteSize_32x32,
               SpriteColorFormat_256Color,
               Surface,
               0,
               false,
               false,
               false,
               false,
               false);

        // Disable sprite scaling and rotating, we won't need it and we require
        // it to be disabled to hide the sprite.
        oamMain.oamMemory->isRotateScale = false;
    }

    inline void Set_Cursor_Palette(const u16* palette)
    {
        // Copy the palette to the sprite engine.
        dmaCopyWords(3, palette, SPRITE_PALETTE, 2 * 256);
    }

    inline void Set_Video_Cursor(void* cursor, int w, int h, int hotx, int hoty)
    {
        Raw = cursor;
        W = w;
        H = h;
        HotX = hotx;
        HotY = hoty;

        uint8_t* src = (uint8_t*)Raw;
        uint8_t* dst = (uint8_t*)Surface;

        // Mouse sprites aren't stored in VRAM so we only copy the new mouse
        // shape in case it changes.
        if (Raw != Last_Raw) {
            Raw = Last_Raw;
            /* On retail DS writes 8bit writes to VRAM are discarded.  So
               create a temporary surface to transform the bitmap shape
               into tiled.  */
            static uint8_t tiled_surface[24 * 32];
            memset(tiled_surface, 0, sizeof(tiled_surface));

            // DS sprites are tiled, so we remap the texture to be displayed
            // correctly.
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    for (int ii = 0; ii < 8; ii++) {
                        for (int jj = 0; jj < 8; jj++) {
                            int real_j = 8 * j + jj;
                            int real_i = 8 * i + ii;

                            if (real_j < w && real_i < h)
                                tiled_surface[256 * i + 8 * ii + 64 * j + jj] = src[real_i * w + real_j];
                        }
                    }
                }
            }
            /* Blt to sprite.  */
            DC_FlushRange(tiled_surface, 32*24);
            dmaCopyWords(3, tiled_surface, dst, 32*24);
        }
    }

    inline void Get_Video_Mouse(int& x, int& y)
    {
        x = X;
        y = Y;
    }

    inline void Set_Video_Cursor_Clip(bool clipped)
    {
        Clip = clipped;
    }

    inline void Update_HWCursor()
    {
        /* Update sprite representing the mouse cursor.  */
        int x_scaled;
        int y_scaled;

        // Transform the coordinates to show the cursor in the correct
        // screen position.
        if (ZoomState) {
            x_scaled = (X - HotX) - 32;
            y_scaled = (Y - HotY) - 4;
        } else {
            x_scaled = ((X - HotX) * SCREEN_WIDTH) / 320;
            y_scaled = ((Y - HotY) * SCREEN_HEIGHT) / 200;
        }

        // Hide or show the cursor accordingly.
        oamMain.oamMemory->isHidden = Get_Mouse_State();

        // Update cursor sprite position
        oamSetXY(&oamMain, 0, x_scaled, y_scaled);
        oamUpdate(&oamMain);
    }

    void Ensure_Mouse_Boundary(void)
    {
        if (X >= 320) {
            X = 319;
        } else if (X < 0) {
            X = 0;
        }

        if (Y >= 200) {
            Y = 199;
        } else if (Y < 0) {
            Y = 0;
        }
    }

    void Move_Video_Mouse(int xrel, int yrel)
    {
        X += xrel;
        Y += yrel;

        Ensure_Mouse_Boundary();
    }

    void Set_Video_Mouse(int x, int y)
    {
        if (ZoomState) {
            X = x + 32;
            Y = y + 4;
        } else {
            X = (x * 320) / SCREEN_WIDTH;
            Y = (y * 200) / SCREEN_HEIGHT;
        }

        // We don't need to ensure it.  The input comes from the screen.
        // Ensure_Mouse_Boundary();
    }

private:
    void* Raw;      // Stores the cursor sprite untouched.
    void* Surface;  // Stores the sprite in 8x8 tiled format for the DS.
    void* Last_Raw; // Stores the pointer to the last used sprite.

    bool Clip; // Flag to show or hide the mouse.
    int X;     // X position.
    int Y;
    int W;    // Width.
    int H;    // Height
    int HotX; // Cursor bias according to icon.
    int HotY;
};

static HardwareCursor HWCursor;

// Unsused.  Required by the game's engine.
class SurfaceMonitorClassNDS : public SurfaceMonitorClass
{

public:
    SurfaceMonitorClassNDS()
    {
    }

    virtual void Restore_Surfaces()
    {
    }

    virtual void Set_Surface_Focus(bool in_focus)
    {
    }

    virtual void Release()
    {
    }
};

SurfaceMonitorClassNDS AllSurfacesNDS;             // List of all direct draw surfaces
SurfaceMonitorClass& AllSurfaces = AllSurfacesNDS; // List of all direct draw surfaces

// VBlank timer function.
void Timer_VBlank(void);

void On_VBlank()
{
    // Update the number of ticks passed.  We use this to implement a 60FPS
    // timer. Altough vanilla-conquer uses a milliseconds based timer, original
    // game used a 60FPS timer.
    Timer_VBlank();
}

bool Set_Video_Mode(int w, int h, int bits_per_pixel)
{
    // Allocate memory for our console.  This has to be static because it must
    // persists when this function exit, even if only used here.
    static PrintConsole cs0;

    // Only turns on the 2D engine. The 3D chip is unused and disabling it
    // should save battery life.
    powerOn(POWER_ALL_2D);

    // If the ARM9 is set to 67MHz, set it to 133MHz now.
    setCpuClock(true);

    // Allocate 128Kb for the console on the upper screen.  It is a bit
    // overkill, but we got plenty of VRAM so far so it is OK.
    vramSetBankC(VRAM_C_SUB_BG_0x06200000);
    videoSetModeSub(MODE_0_2D);

    cpuStartTiming(0);

    // Initialize the console on the top screen.
    consoleInit(&cs0, 0, BgType_Text4bpp, BgSize_T_256x256, 2, 0, false, true);

    // Setup what should run on a VBlank interrupt.
    irqSet(IRQ_VBLANK, On_VBlank);

    // Install the default Nintendo DS exception handler. It sucks, but that is
    // what we got.
    defaultExceptionHandler();

    // Allocate 128kb of VRAM for the background that will hold the visible surface.
    // The backgroung is 512x256, which is 128Kb, a full memory bank.
    vramSetBankA(VRAM_A_MAIN_BG_0x06000000);

    // Put DS into 2D mode with extended background scaling/rotation support. This
    // is necessary so we can downscale the game to fit into the DS low resolution
    // screen.
    videoSetMode(MODE_3_2D | DISPLAY_BG3_ACTIVE);
    bg3 = bgInit(3, BgType_Bmp8, BgSize_B8_512x256, 0, 0);

    // Set downscaling 320x200 => 256x192
    DS_SeenBuff_ZoomOut();

    // Initialize the Hardware cursor, which is basically a spite that is
    // displayed on top of the background.
    HWCursor.Init();

    // Swap the LCD screen so that the main 2D engine is on the bottom screen.
    lcdSwap();

    if (w != 320 || h != 200 || bits_per_pixel != 8)
        return false;

    return true;
}

bool Is_Video_Fullscreen()
{
    return true;
}

/***********************************************************************************************
 * Reset_Video_Mode -- Resets video mode and deletes Direct Draw Object                        *
 *                                                                                             *
 * INPUT:		none                                                                            *
 *                                                                                             *
 * OUTPUT:     none                                                                            *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   09/26/1995 PWG : Created.                                                                 *
 *=============================================================================================*/
void Reset_Video_Mode(void)
{
}

/***********************************************************************************************
 * Get_Free_Video_Memory -- returns amount of free video memory                                *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   bytes of available video RAM                                                      *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    11/29/95 12:52PM ST : Created                                                            *
 *=============================================================================================*/
unsigned int Get_Free_Video_Memory(void)
{
    return 1000000000;
}

/***********************************************************************************************
 * Get_Video_Hardware_Caps -- returns bitmask of direct draw video hardware support            *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   hardware flags                                                                    *
 *                                                                                             *
 * WARNINGS: Must call Set_Video_Mode 1st to create the direct draw object                     *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    1/12/96 9:14AM ST : Created                                                              *
 *=============================================================================================*/
extern bool OverlappedVideoBlits;
unsigned Get_Video_Hardware_Capabilities(void)
{
    // We do not implement overlapping capabilities on Blt. It is also strategically faster to
    // disable this flag because it will force a blit from seenbuffer to hidbuffer in order
    // to scroll the screen, which is faster than bliting from main RAM to main RAM.
    OverlappedVideoBlits = false;

    // Return the features we implement.
    return VIDEO_BLITTER | VIDEO_COLOR_FILL;
}

/***********************************************************************************************
 * Wait_Vert_Blank -- Waits for the start (leading edge) of a vertical blank                   *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *=============================================================================================*/
void Wait_Vert_Blank(void)
{
    swiWaitForVBlank();
}

/***********************************************************************************************
 * Set_Palette -- set a direct draw palette                                                    *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    ptr to 768 rgb palette bytes                                                      *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    10/11/95 3:33PM ST : Created                                                             *
 *=============================================================================================*/
void Set_DD_Palette(void* palette)
{
    unsigned char r, g, b;

    unsigned char* rcolors = (unsigned char*)palette;
    for (int i = 0; i < 256; i++) {
        r = (unsigned char)rcolors[i * 3] << 2;
        g = (unsigned char)rcolors[i * 3 + 1] << 2;
        b = (unsigned char)rcolors[i * 3 + 2] << 2;

        BG_PALETTE[i] = RGB8(r, g, b);
    }

    HWCursor.Set_Cursor_Palette(BG_PALETTE);
}

void Wait_Blit(void)
{
}

void Set_Video_Cursor_Clip(bool clipped)
{
    HWCursor.Set_Video_Cursor_Clip(clipped);
}

void Get_Video_Mouse(int& x, int& y)
{
    HWCursor.Get_Video_Mouse(x, y);
}

void Set_Video_Mouse(int x, int y)
{
    HWCursor.Set_Video_Mouse(x, y);
}

void Set_Video_Cursor(void* cursor, int w, int h, int hotx, int hoty)
{
    HWCursor.Set_Video_Cursor(cursor, w, h, hotx, hoty);
}

/***********************************************************************************************
 * SMC::SurfaceMonitorClass -- constructor for surface monitor class                           *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    11/3/95 3:23PM ST : Created                                                              *
 *=============================================================================================*/

SurfaceMonitorClass::SurfaceMonitorClass()
{
    SurfacesRestored = false;
}

void Update_HWCursor()
{
    HWCursor.Update_HWCursor();
}

/*
** VideoSurfaceDDraw
*/

class VideoSurfaceNDS;
static VideoSurfaceNDS* frontSurface = nullptr;

// The hidden surface buffer.
static char HidSurfaceBuf[320 * 200];

#define ALIGNED(ptr, n) (((uintptr_t)(ptr) % (n)) == 0)

class VideoSurfaceNDS : public VideoSurface
{
public:
    VideoSurfaceNDS(int w, int h, GBC_Enum flags)
        : flags(flags)
        , windowSurface(nullptr)
    {
        if (w == 320 && h == 200) {
            // The DS renderer works as follows: we pass the background
            // buffer in VRAM to the game's software engine, which draws
            // things there. The background is a 512x256 surface, but
            // only 512x200 pixels are used.

            if (flags & GBC_VISIBLE) {
                Pitch = 512;
                surface = (char*)bgGetGfxPtr(bg3);
                windowSurface = surface;
                frontSurface = this;
            } else {
                // The hid surface will be allocated in main RAM. The game
                // uses a software engine that memcpy the graphics, and in
                // this case it is faster to allocate this in main RAM.
                Pitch = 320;
                surface = HidSurfaceBuf;
            }
        } else {
            swiWaitForVBlank();
            printf("ERROR - Unsupported surface size\n");
            while (1)
                ;
        }
    }

    virtual ~VideoSurfaceNDS()
    {
        if (frontSurface == this) {
            frontSurface = NULL;
        }
    }

    virtual void* GetData() const
    {
        return surface;
    }
    virtual int GetPitch() const
    {
        return Pitch;
    }
    virtual bool IsAllocated() const
    {
        return false;
    }

    virtual void AddAttachedSurface(VideoSurface* surface)
    {
    }

    virtual bool IsReadyToBlit()
    {
        return true;
    }

    virtual bool LockWait()
    {
        return true;
    }

    virtual bool Unlock()
    {
        return true;
    }

    // Use the DMA controller to implement the BitBlt algorithm.
    virtual void Blt(const Rect& destRect, VideoSurface* src, const Rect& srcRect, bool mask)
    {
      short src_pitch = src->GetPitch();
      char *src_ptr = static_cast<char*>(src->GetData());

      // Move head of src_ptr to the rectangle upper left corner.
      src_ptr += srcRect.Y * src_pitch + srcRect.X;

      // Do the same thing for our dst surface.
      short dst_pitch = this->GetPitch();
      char *dst_ptr = static_cast<char*>(this->GetData());

      dst_ptr += destRect.Y * dst_pitch + destRect.X;

      short w = srcRect.Width;
      short h = srcRect.Height;

      bool src_in_main_ram = (uintptr_t) src_ptr < 0x06000000;
      bool dst_in_main_ram = (uintptr_t) dst_ptr < 0x06000000;

      if (src_in_main_ram && dst_in_main_ram) {
        // In case src and dst is in main ram, then memcpy is simply faster.
        w = w >> 2;
        while (h-- > 0) {
          memcpy32(dst_ptr, src_ptr, w);
          src_ptr += src_pitch;
          dst_ptr += dst_pitch;
        }
      } else {
        short dma = 0;
        int flushrange = 320*(h - 1) + w;

        // Careful with alignment.
        if (ALIGNED(src_ptr, 4) && ALIGNED(dst_ptr, 4)) {
          if (src_in_main_ram) {
            DC_FlushRange(src_ptr, flushrange);
          }
          else if (dst_in_main_ram)
            DC_FlushRange(dst_ptr, flushrange);

          // Unroll iterations:
          short h_div = h / 4;
          short h_mod = h % 4;

          for (short i = 0; i < h_div; i++) {
            dmaCopyWordsAsynch(0, src_ptr, dst_ptr, w);
            src_ptr += src_pitch;
            dst_ptr += dst_pitch;
            dmaCopyWordsAsynch(1, src_ptr, dst_ptr, w);
            src_ptr += src_pitch;
            dst_ptr += dst_pitch;
            dmaCopyWordsAsynch(2, src_ptr, dst_ptr, w);
            src_ptr += src_pitch;
            dst_ptr += dst_pitch;
            dmaCopyWordsAsynch(3, src_ptr, dst_ptr, w);
            src_ptr += src_pitch;
            dst_ptr += dst_pitch;
          }

          for (short i = 0; i < h_mod; i++) {
            dmaCopyWordsAsynch(i, src_ptr, dst_ptr, w);
            src_ptr += src_pitch;
            dst_ptr += dst_pitch;
          }


        } else if (ALIGNED(src_ptr, 2) && ALIGNED(dst_ptr, 2)) {

          if (src_in_main_ram)
            DC_FlushRange(src_ptr, flushrange);
          else if (dst_in_main_ram)
            DC_FlushRange(dst_ptr, flushrange);

          // Unroll iterations:
          short h_div = h / 4;
          short h_mod = h % 4;
          for (short i = 0; i < h_div; i++) {
            dmaCopyHalfWordsAsynch(0, src_ptr, dst_ptr, w);
            src_ptr += src_pitch;
            dst_ptr += dst_pitch;
            dmaCopyHalfWordsAsynch(1, src_ptr, dst_ptr, w);
            src_ptr += src_pitch;
            dst_ptr += dst_pitch;
            dmaCopyHalfWordsAsynch(2, src_ptr, dst_ptr, w);
            src_ptr += src_pitch;
            dst_ptr += dst_pitch;
            dmaCopyHalfWordsAsynch(3, src_ptr, dst_ptr, w);
            src_ptr += src_pitch;
            dst_ptr += dst_pitch;
          }

          for (short i = 0; i < h_mod; i++) {
            dmaCopyHalfWordsAsynch(i, src_ptr, dst_ptr, w);
            src_ptr += src_pitch;
            dst_ptr += dst_pitch;
          }

        } else {
          // No align fix possible.

          //u32 start = cpuGetTiming();

          short dst_unalign = (uintptr_t)(dst_ptr) & 1;

          for (short i = 0; i < h; i++) {
            short size = w;

            unsigned short *dst16 = (unsigned short *) dst_ptr;
            unsigned char *src8 = (unsigned char *) src_ptr;

            if (dst_unalign) {
              dst16 = (unsigned short *) ((uintptr_t)dst16 & ~1);
              *dst16++ = (*dst16 & 0xFF) | *src8++ << 8;
              size--;
            }

            int count = size/2;
            while(count--)
            {
              *dst16++ = src8[0] | src8[1]<<8;
              src8 += 2;
            }

            if (size & 1)
              *dst16 = (*dst16 &~ 0xFF) | *src8;


            dst_ptr += dst_pitch;
            src_ptr += src_pitch;
          }

#if 0
          short src_unalign = (((uintptr_t)(src_ptr) + 3) & ~3) - 
                              (uintptr_t)(src_ptr);

          for (short i = 0; i < h; ++i) {
            unsigned char *dst_ptr8 = (unsigned char*)(dst_ptr);
            unsigned char *src_ptr8 = (unsigned char*)(src_ptr);

            for (short i = 0; i < src_unalign; i++) {
              *dst_ptr8++ = *src_ptr8++;
            }

            unsigned int *src_ptr32 = (unsigned int *)(src_ptr8);
            short count = (w - src_unalign) / 4;
            //short remain = (w - src_unalign) % 4;

            while(count--) {
              unsigned int val = *src_ptr32++;
              *dst_ptr8++ = val & 0xFF;
              *dst_ptr8++ = (val >> 8) & 0xFF;
              *dst_ptr8++ = (val >> 16) & 0xFF;
              *dst_ptr8++ = (val >> 24) & 0xFF;
            }
/*
            src_ptr8 = (unsigned char*)src_ptr32;
            while (remain--) {
              *dst_ptr8++ = *src_ptr8++;
            }
*/
            src_ptr += src_pitch;
            dst_ptr += dst_pitch;
          }
#endif
          //u32 end = cpuGetTiming();
          //printf("Ticks: %lu\n", end - start);

        }
      }


    }

    virtual void FillRect(const Rect& rect, unsigned char color)
    {
      short dst_pitch = this->GetPitch();
      char *dst_ptr = static_cast<char*>(this->GetData());

      dst_ptr += rect.Y * dst_pitch + rect.X;

      short w = rect.Width;
      short h = rect.Height;

      bool buffer_in_vram = (uintptr_t) dst_ptr >= 0x06000000;

      if (buffer_in_vram) {

        u32 c32 = color;
        c32 = c32 | c32 << 8 | c32 << 16 | c32 << 24;

        short w_cpu = w % 4;

        for (short i = 0; i < h; i++) {
          dmaFillWords(c32, dst_ptr, w);
          memset(dst_ptr + ((w >> 2) << 2), c32, w_cpu);
          dst_ptr += dst_pitch;
        }
      } else {
        while (h-- > 0) {
          memset(dst_ptr, color, w);
          dst_ptr += dst_pitch;
        }
      }
    }

    inline void RenderSurface()
    {
    }

private:
    int Pitch;
    char* surface;
    char* windowSurface;
    GBC_Enum flags;
};

/*
** Video
*/

Video::Video()
{
}

Video::~Video()
{
}

Video& Video::Shared()
{
    static Video video;
    return video;
}

VideoSurface* Video::CreateSurface(int w, int h, GBC_Enum flags)
{
    return new VideoSurfaceNDS(w, h, flags);
}

//! VRAM-safe cpy. Stolen from libtonc
/*!	This version mimics memcpy in functionality, with
	the benefit of working for VRAM as well. It is also
	slightly faster than the original memcpy, but faster
	implementations can be made.
	\param dst	Destination pointer.
	\param src	Source pointer.
	\param size	Fill-length in bytes.
	\return		\a dst.
	\note	The pointers and size need not be word-aligned.
*/
void *tonccpy(void *dst, const void *src, size_t size)
{
  if (isDSiMode())
    return memcpy(dst, src, size);

  uint count;
  u16 *dst16;		// hword destination
  u8  *src8;		// byte source

  // Ideal case: copy by 4x words. Leaves tail for later.
  if( ((u32)src|(u32)dst)%4==0 && size>=4)
  {
    u32 *src32= (u32*)src, *dst32= (u32*)dst;

    count= size/4;
    uint tmp= count&3;
    count /= 4;

    // Duff, bitch!
    switch(tmp) {
      do {	*dst32++ = *src32++;
        case 3:		*dst32++ = *src32++;
        case 2:		*dst32++ = *src32++;
        case 1:		*dst32++ = *src32++;
        case 0:		; }	while(count--);
    }

    // Check for tail
    size &= 3;
    if(size == 0)
      return dst;

    src8= (u8*)src32;
    dst16= (u16*)dst32;
  }
  else		// Unaligned.
  {
    uint dstOfs= (u32)dst&1;
    src8= (u8*)src;
    dst16= (u16*)(dst-dstOfs);

    // Head: 1 byte.
    if(dstOfs != 0)
    {
      *dst16= (*dst16 & 0xFF) | *src8++<<8;
      dst16++;
      if(--size==0)
        return dst;
    }
  }

  // Unaligned main: copy by 2x byte.
  count= size/2;
  while(count--)
  {
    *dst16++ = src8[0] | src8[1]<<8;
    src8 += 2;
  }

  // Tail: 1 byte.
  if(size&1)
    *dst16= (*dst16 &~ 0xFF) | *src8;

  return dst;
}
