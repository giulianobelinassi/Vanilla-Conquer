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

/* $Header: /CounterStrike/CCFILE.H 1     3/03/97 10:24a Joe_bostic $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : CCFILE.H                                                     *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : October 17, 1994                                             *
 *                                                                                             *
 *                  Last Update : October 17, 1994   [JLB]                                     *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef CCFILE_H
#define CCFILE_H

//#include	<wwlib32.h>
#include <limits.h>
#include "mixfile.h"
#include "cdfile.h"
#include "buff.h"

/*
**	This derived class for file access knows about mixfiles (packed files). It can handle opening
**	a file that is embedded within a mixfile. This is true if the mixfile is cached or resides on
**	disk. It is functionally similar to pakfiles, except much faster and less RAM intensive.
*/
class CCFileClass : public CDFileClass
{
public:
    CCFileClass(char const* filename);
    CCFileClass(void);
    virtual ~CCFileClass(void)
    {
        Position = 0;
    };

    // Delete should be overloaded here as well. Don't allow deletes of mixfiles.

    bool Is_Resident(void) const
    {
        return (Data.Get_Buffer() != NULL);
    }
    virtual int Is_Available(int forced = false);
    virtual int Is_Open(void) const;
    virtual int Open(char const* filename, int rights = READ)
    {
        Set_Name(filename);
        return Open(rights);
    };
    virtual int Open(int rights = READ);
    virtual int Read(void* buffer, int size);
    virtual int Seek(int pos, int dir = SEEK_CUR);
    virtual int Size(void);
    virtual int Write(void const* buffer, int size);
    virtual void Close(void);
    virtual void Error(int error, int canretry = false, char const* filename = NULL);

protected:
    /*
    **	This indicates the file is actually part of a resident image of the mixfile
    **	itself. In this case, the embedded file handle is invalid. All file access actually
    **	gets routed through the cached version of the file. This is a pointer to the start
    **	of the RAM image of the file.
    */
    ::Buffer Data;
    //		void * Pointer;

    /*
    **	This is the size of the file if it was embedded in a mixfile. The size must be manually
    **	kept track of because the DOS file size is invalid.
    */
    //		long Length;

    /*
    **	This is the current seek position of the file. It is duplicated here if the file is
    **	part of a mixfile since the DOS seek position is not accurate. This value will
    **	range from zero to the size of the file in bytes.
    */
    int Position;

private:
    // Force these to never be invoked.
    CCFileClass const& operator=(CCFileClass const& c);
    CCFileClass(CCFileClass const&);
};

#ifdef _N64
#include <libdragon.h>

class N64ROMCCFileClass
{
    public:
    N64ROMCCFileClass(void);

    int Is_Available(const char *filename, int forced = false);
    int Is_Open(void) const;
    int Open(const char *filename, int rights = READ);
    int Read(void* buffer, int size);
    int Seek(int pos, int dir = SEEK_CUR);
    int Size(void);
    void Close(void);

    private:

    struct ROMCacheEntry
    {
      int32_t crc;
      uint32_t ROMAddr;
      uint32_t size;
    };

    static class ROMAddrCache
    {
      private:
      enum {MAX_ENTRIES = 64};

      ROMCacheEntry Elem[MAX_ENTRIES];
      unsigned NumElem;

      static int ROMCacheCmpFunc(const void *p1, const void *p2)
      {
        const struct ROMCacheEntry *pa = (const struct ROMCacheEntry *) p1;
        const struct ROMCacheEntry *pb = (const struct ROMCacheEntry *) p2;

        return (pa->crc > pb->crc) - (pa->crc < pb->crc);
      }

      public:
      ROMCacheEntry *Add_From_String(const char *name, uint32_t romaddr, uint32_t size)
      {
        assert(NumElem < MAX_ENTRIES);

        int32_t crc = Calculate_CRC(name, strlen(name));
        Elem[NumElem].crc = crc;
        Elem[NumElem].ROMAddr = romaddr;
        Elem[NumElem].size = size;
        NumElem++;

        qsort(Elem, NumElem, sizeof(ROMCacheEntry), ROMCacheCmpFunc);  
        return Get_From_CRC(crc);
      }

      ROMCacheEntry *Get_From_String(const char *name)
      {
        int slen = strlen(name);
        int32_t crc = Calculate_CRC(name, slen);

        return Get_From_CRC(crc);
      }

      ROMCacheEntry *Get_From_CRC(int crc)
      {
        ROMCacheEntry key = { .crc = crc };
        ROMCacheEntry *ret;

        ret = (ROMCacheEntry *)
          bsearch(&key, Elem, NumElem, sizeof(ROMCacheEntry), ROMCacheCmpFunc);

        return ret;
      }
    } ROMAddrCache;

  uint32_t FileROMAddr;
  uint32_t FileSize;
  int Position;
};

#endif

#endif
