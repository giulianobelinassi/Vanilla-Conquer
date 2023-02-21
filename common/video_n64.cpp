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
#include <cstdio>
#include <libdragon.h>
#include "debugstring.h"

class SurfaceMonitorClassDummy : public SurfaceMonitorClass
{

public:
    SurfaceMonitorClassDummy()
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

SurfaceMonitorClassDummy AllSurfacesDummy;           // List of all direct draw surfaces
SurfaceMonitorClass& AllSurfaces = AllSurfacesDummy; // List of all direct draw surfaces

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
void Timer_VBlank();

bool Set_Video_Mode(int w, int h, int bits_per_pixel)
{
    init_interrupts();
    rdp_init();
    rdpq_init();
    controller_init();
    timer_init();

    switch (w) {
      case 320:
        display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
      break;

      case 512:
        display_init(RESOLUTION_512x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
      break;

      case 640:
        display_init(RESOLUTION_640x480, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
      break;

      default:
        DBG_LOG("Video mode not supported");
        display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    }

    register_VI_handler(Timer_VBlank);

    return true;
}

bool Is_Video_Fullscreen()
{
    return false;
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
    // Overlapping regions do not work with RDP.
    OverlappedVideoBlits = false;

    // Return the fancy features we support.
    return VIDEO_BLITTER | VIDEO_BLITTER_ASYNC | VIDEO_COLOR_FILL;
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

#define RGB8(r,g,b)  (((r)>>3)|(((g)>>3)<<5)|(((b)>>3)<<10))
#define RGB15(r,g,b)  ((b)|((g)<<5)|((r)<<10))
#define RGB5(r,g,b)  ((r)|((g)<<5)|((b)<<10))

static uint16_t CurrN64Pal[256];

void Set_DD_Palette(void* palette)
{
    const char *cpalette = (const char *) palette;
    unsigned r, g, b;

    for (int i = 0; i < 256; i++)
    {
      r = cpalette[3*i + 0] << 2;
      g = cpalette[3*i + 1] << 2;
      b = cpalette[3*i + 2] << 2;

      CurrN64Pal[i] = (uint16_t) (graphics_make_color(r, g, b, 0xff)) ;

      //CurrN64Pal[i] = RGB15(r, g, b);//graphics_make_color(r,g,b,a);
    }
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

class VideoSurfaceN64;
static VideoSurfaceN64 *frontSurface;

/*
** VideoSurfaceDDraw
*/

class VideoSurfaceN64 : public VideoSurface
{
public:
    VideoSurfaceN64(int w, int h, GBC_Enum flags)
    {
      Surface = surface_alloc(FMT_CI8, w, h);

      if (!Surface.buffer) {
        DBG_LOG("Not enough memory to allocate 320x200 surface");
      }

      if (flags & GBC_VISIBLE) {
        frontSurface = this;
      }
    }

    virtual ~VideoSurfaceN64()
    {
    }

    virtual void* GetData() const
    {
        return Surface.buffer;
    }
    virtual int GetPitch() const
    {
        return Surface.stride;
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

    virtual void Blt(const Rect& destRect, VideoSurface* src, const Rect& srcRect, bool mask)
    {
      short src_pitch = src->GetPitch();
      char *src_ptr = static_cast<char*>(src->GetData());

      // Move head of src_ptr to the rectangle upper left corner.
      //src_ptr += srcRect.Y * src_pitch + srcRect.X;

      // Compute the dimensions of src surface.
      short src_w = srcRect.Width + srcRect.X;
      short src_h = srcRect.Height + srcRect.Y;

      // Create src surface using our pointer data.
      surface_t src_surface = surface_make(src_ptr, FMT_I8, src_w, src_h, src_pitch);

      // Do the same thing for our dst surface.
      short dst_pitch = this->GetPitch();
      char *dst_ptr = static_cast<char*>(this->GetData());

      //dst_ptr += destRect.Y * dst_pitch + destRect.X;

      short dst_w = destRect.Width + destRect.X;
      short dst_h = destRect.Height + destRect.Y;

      surface_t dst_surface = surface_make(dst_ptr, FMT_CI8, dst_w, dst_h, dst_pitch);

      // Enable validator
      //rdpq_debug_start();

      // Attach the mighty RDP
      rdpq_attach(&dst_surface, NULL);

      // Set copy render mode, without transparency.
      rdpq_mode_tlut(TLUT_NONE);
      rdpq_set_mode_standard();

      // Blit
      rdpq_blitparms_t blt_parms = {
        .s0 = srcRect.X,
        .t0 = srcRect.Y,
        .width = srcRect.Width,
        .height = srcRect.Height,
      };
      rdpq_tex_blit(&src_surface, destRect.X, destRect.Y, &blt_parms);

      // Detatch the RDP and show
      rdpq_detach();
      //rdpq_debug_stop();
    }

    virtual void FillRect(const Rect& rect, unsigned char color)
    {
      #if 0
      static unsigned char __attribute__((alinged (64))) pixel[1];
      pixel[0] = color;

      surface_t src_surface = surface_make_linear(&pixel, FMT_I8, 1, 1);


      // Enable validator
      //rdpq_debug_start();

      // Attach the mighty RDP
      rdpq_attach(&Surface);

      // Set copy render mode, without transparency.
      rdpq_mode_tlut(TLUT_NONE);
      rdpq_set_mode_standard();

      // Blit
      rdpq_blitparms_t blt_parms = {
        .s0 = 0,
        .t0 = 0,
        .width = 1,
        .height = 1,
        .scale_x = rect.Width,
        .scale_y = rect.Height,
      };
      rdpq_tex_blit(&src_surface, rect.X, rect.Y, &blt_parms);

      // Detatch the RDP and show
      rdpq_detach();
      //rdpq_debug_stop();
      #endif
    }

    void RenderSurface(void)
    {
      surface_t *disp;

      // Make sure the RDP has finished drawing whatever it was drawing before.
      //rdpq_sync_full(NULL, NULL);

      while( !(disp = display_lock()) );


      // Attach the mighty RDP
      rdpq_attach(disp, NULL);

      // Load the palette
      data_cache_hit_writeback(CurrN64Pal, 256*2);
      rdpq_tex_load_tlut(CurrN64Pal, 0, 256);

      // Set copy render mode, with palette lookup
      rdpq_set_mode_copy(false);
      rdpq_mode_tlut(TLUT_RGBA16);

      // Blit
      //static const rdpq_blitparms_t fix_aspect_ratio = {.scale_y = 1.2f};
      rdpq_tex_blit(&Surface, 0, 20, NULL);

      // Detatch the RDP and show
      rdpq_detach_show();

/*
      // Software render.
      int16_t w = Surface.width;
      int16_t h = Surface.height;
      uint32_t len = w*h;

      const uint8_t *src_buffer = (const uint8_t*) Surface.buffer;
      uint16_t *dest_buffer = (uint16_t*) disp->buffer;
      dest_buffer += 20 * w;

      for (int i = 0; i < len; i++)
        *dest_buffer++ = CurrN64Pal[*src_buffer++];

      display_show(disp);
*/
    }

    surface_t Surface;
};

void Video_Render_Frame(void)
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
    return new VideoSurfaceN64(w, h, flags);
}
