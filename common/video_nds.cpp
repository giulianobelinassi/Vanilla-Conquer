//

// Copyright 2020 Electronic Arts Inc.
//
// TiberianDawn.DLL and RedAlert.dll and corresponding source code is free
// software: you can redistribute it and/or modify it under the terms of
// the GNU General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.

// TiberianDawn.DLL and RedAlert.dll and corresponding source code is distributed
// in the hope that it will be useful, but with permitted additional restrictions
// under Section 7 of the GPL. See the GNU General Public License in LICENSE.TXT
// distributed with this program. You should have received a copy of the
// GNU General Public License along with permitted additional restrictions
// with this program. If not, see https://github.com/electronicarts/CnC_Remastered_Collection

/***************************************************************************
 **   C O N F I D E N T I A L --- W E S T W O O D   A S S O C I A T E S   **
 ***************************************************************************
 *                                                                         *
 *                 Project Name : Westwood Win32 Library                   *
 *                                                                         *
 *                    File Name : DDRAW.CPP                                *
 *                                                                         *
 *                   Programmer : Philip W. Gorrow                         *
 *                                                                         *
 *                   Start Date : October 10, 1995                         *
 *                                                                         *
 *                  Last Update : October 10, 1995   []                    *
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*=========================================================================*/
/* The following PRIVATE functions are in this file:                       */
/*=========================================================================*/

/*= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =*/

#include "gbuffer.h"
#include "palette.h"
#include "video.h"
#include "video.h"
#include "wwkeyboard.h"
#include <cstdio>
#include <nds.h>
#include <stdarg.h>

void pause(const char* format, ...)
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

static struct
{
    void* Raw;
    void* Surface;

    void* Last_Raw;

    bool Clip;
    int X;
    int Y;
    int W;
    int H;
    int HotX;
    int HotY;
} hwcursor;

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

/***********************************************************************************************
 * Set_Video_Mode -- Initializes Direct Draw and sets the required Video Mode                  *
 *                                                                                             *
 * INPUT:           int width           - the width of the video mode in pixels                *
 *                  int height          - the height of the video mode in pixels               *
 *                  int bits_per_pixel  - the number of bits per pixel the video mode supports *
 *                                                                                             *
 * OUTPUT:     none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   09/26/1995 PWG : Created.                                                                 *
 *=============================================================================================*/

static int bg3;

void create_chess_pattern(int w, int h, unsigned char* _dest, unsigned char c1, unsigned char c2)
{
#define DEST(i, j) (_dest[(i) * (512) + (j)])

    const int step = 16;

    int i, j, ii, jj;
    for (i = 0; i < h; i += step) {
        for (j = 0; j < w; j += step) {
            for (ii = i; ii < i + step; ii++) {
                for (jj = j; jj < j + step; jj++) {
                    char color = ((i + j) / step) % 2;
                    if (color)
                        DEST(ii, jj) = c1;
                    else
                        DEST(ii, jj) = c2;
                }
            }
        }
    }
#undef DEST
}

void VBlank_Mouse()
{
    uint32_t keys_current = keysCurrent();
    int x = 0, y = 0;

    if (keys_current & KEY_UP) {
        y -= 2;
    } else if (keys_current & KEY_DOWN) {
        y += 2;
    }
    if (keys_current & KEY_LEFT) {
        x -= 2;
    } else if (keys_current & KEY_RIGHT) {
        x += 2;
    }
    if (x || y) {
        Move_Video_Mouse(x, y);
    }
}

bool Set_Video_Mode(int w, int h, int bits_per_pixel)
{
    powerOn(POWER_ALL);
    irqSet(IRQ_VBLANK, VBlank_Mouse);
    //    defaultExceptionHandler();

    TIMER0_DATA = 0; // Set up the timer
    TIMER1_DATA = 0;
    TIMER0_CR = TIMER_DIV_1024 | TIMER_ENABLE;
    TIMER1_CR = TIMER_CASCADE | TIMER_ENABLE;

    videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE); // BG3 only - extended rotation

    bg3 = bgInit(3, BgType_Bmp8, BgSize_B8_512x512, 0, 0);

    vramSetBankA(VRAM_A_MAIN_BG_0x06000000); // same as VRAM_A_MAIN_BG
    //vramSetBankB(VRAM_B_MAIN_BG_0x06020000); // use second bank for main screen - 256 KiB

    REG_BG3CNT = BG_BMP8_512x512;  // BG3 Control register, 8 bits
    REG_BG3PA = (320 * 256) / 256; //1 << 8;
    REG_BG3PB = 0;                 // BG SCALING X
    REG_BG3PC = 0;                 // BG SCALING Y
    REG_BG3PD = (200 * 256) / 192; // << 8;
    REG_BG3X = 0;
    REG_BG3Y = 0;

    bgUpdate();

    vramSetBankF(VRAM_F_MAIN_SPRITE_0x06400000);
    oamInit(&oamMain, SpriteMapping_1D_256, false);

    hwcursor.Surface = oamAllocateGfx(&oamMain, SpriteSize_32x32, SpriteColorFormat_256Color);

    hwcursor.X = 160;
    hwcursor.Y = 100;

    oamSet(&oamMain,
           0,
           hwcursor.X,
           hwcursor.Y,
           0,
           0,
           SpriteSize_32x32,
           SpriteColorFormat_256Color,
           hwcursor.Surface,
           0,
           false,
           false,
           false,
           false,
           false);

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
unsigned Get_Video_Hardware_Capabilities(void)
{
    return 0;
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
    //swiWaitForVBlank();
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

    dmaCopy(BG_PALETTE, SPRITE_PALETTE, 256);
}

/***********************************************************************************************
 * Wait_Blit -- waits for the DirectDraw blitter to become idle                                *
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
 *   07-25-95 03:53pm ST : Created                                                             *
 *=============================================================================================*/

void Wait_Blit(void)
{
}

void Set_Video_Cursor_Clip(bool clipped)
{
    hwcursor.Clip = clipped;
}

void Get_Video_Mouse(int& x, int& y)
{
    x = hwcursor.X;
    y = hwcursor.Y;
}

void Move_Video_Mouse(int xrel, int yrel)
{
    //if (hwcursor.Clip) {
    hwcursor.X += xrel;
    hwcursor.Y += yrel;
    //}

    if (hwcursor.X >= 320) {
        hwcursor.X = 319;
    } else if (hwcursor.X < 0) {
        hwcursor.X = 0;
    }

    if (hwcursor.Y >= 200) {
        hwcursor.Y = 199;
    } else if (hwcursor.Y < 0) {
        hwcursor.Y = 0;
    }
}

void Set_Video_Cursor(void* cursor, int w, int h, int hotx, int hoty)
{

    hwcursor.Raw = cursor;
    hwcursor.W = w;
    hwcursor.H = h;
    hwcursor.HotX = hotx;
    hwcursor.HotY = hoty;

    uint8_t* src = (uint8_t*)hwcursor.Raw;
    uint8_t* dst = (uint8_t*)hwcursor.Surface;

    if (hwcursor.Raw != hwcursor.Last_Raw) {
        hwcursor.Raw = hwcursor.Last_Raw;

        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                for (int ii = 0; ii < 8; ii++) {
                    for (int jj = 0; jj < 8; jj++) {
                        int real_j = 8 * j + jj;
                        int real_i = 8 * i + ii;

                        if (real_j < w && real_i < h)
                            dst[256 * i + 8 * ii + 64 * j + jj] = src[real_i * w + real_j];
                    }
                }
            }
        }
    }
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
    /* Update sprite representing the mouse cursor.  */
    const int x_scaled = ((hwcursor.X - hwcursor.HotX) * 256) / 320;
    const int y_scaled = ((hwcursor.Y - hwcursor.HotY) * 192) / 200;

    oamSetXY(&oamMain, 0, x_scaled, y_scaled);
    oamUpdate(&oamMain);
}

/*
** VideoSurfaceDDraw
*/

class VideoSurfaceNDS;
static VideoSurfaceNDS* frontSurface = nullptr;

class VideoSurfaceNDS : public VideoSurface
{
public:
    VideoSurfaceNDS(int w, int h, GBC_Enum flags)
        : flags(flags)
        , windowSurface(nullptr)
    {
        if (w == 320 && h == 200) {

            if (flags & GBC_VISIBLE) {
                surface = (char*)bgGetGfxPtr(bg3);
                windowSurface = surface;
                frontSurface = this;
            } else {
                surface = (char*)malloc(320 * 200);
                if (!surface) {
                    printf("ERROR - Can't allocate surface buffer\n");
                    while (1)
                        ;
                }
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
        } else if (surface) {
            free(surface);
        }
    }

    virtual void* GetData() const
    {
        return surface;
    }
    virtual long GetPitch() const
    {
        if (frontSurface == this)
            return 512;
        return 320;
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
        return false;
    }

    virtual bool LockWait()
    {
        return true;
    }

    virtual bool Unlock()
    {
        return true;
    }

    virtual void Blt(const Rect& destRect, VideoSurface* src, const Rect& srcRect, bool mask)
    {
        printf("Trying to blit\n");
    }

    virtual void FillRect(const Rect& rect, unsigned char color)
    {
        printf("Trying to fill rect\n");
    }

    inline void RenderSurface()
    {
        //swiWaitForVBlank();
        bgUpdate();

        /* Update sprite representing the mouse cursor.  */
        const int x_scaled = ((hwcursor.X - hwcursor.HotX) * 256) / 320;
        const int y_scaled = ((hwcursor.Y - hwcursor.HotY) * 192) / 200;

        oamSetXY(&oamMain, 0, x_scaled, y_scaled);
        oamUpdate(&oamMain);
    }

private:
    char* surface;
    char* windowSurface;
    GBC_Enum flags;
};

void Video_Render_Frame()
{
    if (frontSurface) {
        frontSurface->RenderSurface();
    }
}

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
