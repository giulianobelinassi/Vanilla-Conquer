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
#include "paths.h"
#include "debugstring.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <vector>
#include <fat.h>
#include <nds.h>

#include <limits.h>

#include "xstraw.h"
#include "pkstraw.h"
#include "shastraw.h"
#include "rndstraw.h"

#define MIN(a, b) (((a) > (b)) ? (b) : (a))

/* DS is quasi-posix compliant: it don't provide some functions.  */
extern "C" {

const char* basename(const char* path)
{
    const char* base = strrchr(path, '/');

    /* If strrchr returned non-null, it means that it found the last '/' in the
       * path, so add one to get the base name.  */
    if (base) {
        return base + 1;
    }

    return path;
}
}

void Set_Video_Mode(int, int, int);

/* Nintendo DS require its filesystem structures to be explicitely initialized. */
void DS_Filesystem_Init()
{
    static bool fs_initialized = false;

    if (fs_initialized)
        return;

    if (!fatInitDefault()) {
        Set_Video_Mode(320, 200, 8);
        DBG_LOG("FATAL ERROR: Unable to initialize file system");
        swiWaitForVBlank();
        while (1)
            ;
    }

    fs_initialized = true;
}

/* Nintendo DS Expansion Pak requires that writes are 16-bit wide.  This
   function ensures that the file is cached correctly.  */
int DS_Cache_File(void *data, int datasize, Straw *straw)
{
    if (isDSiMode()) {
      /* DSi doesn't support the memory expansion pak, so load it directly
         into the RAM.  */
      return straw->Get(data, datasize);
    }

    /* Define a buffer on on-board memory, as it is more flexible.  */
    unsigned char buf[4096];
    unsigned char* datab = (unsigned char*) data;
    int actual;
    int i;

    while (actual < datasize) {
      int to_read = MIN(datasize - actual, 4096);
      i = straw->Get(buf, to_read);

      /* If the number of bytes read isn't multiple of 2, then we align upwards
         to ensure a 16-bit read.  */
      memcpy((u16*) &datab[actual], buf, i + (i & 1));
      actual += i;
    }

    return actual;
}

namespace
{
    const std::string& User_Home()
    {
        return std::string("/vanilla-conquer");
    }

    std::string Get_Posix_Default(const char* env_var, const char* relative_path)
    {
        const char* tmp = std::getenv(env_var);

        if (tmp != nullptr && tmp[0] != '\0') {
            if (tmp[0] != '/') {
                char buffer[200];
                std::snprintf(buffer,
                              sizeof(buffer),
                              "'%s' should start with '/' as per XDG specification that the value must be absolute. "
                              "The current value is: "
                              "'%s'.",
                              tmp,
                              env_var);
                DBG_WARN(buffer);
            } else {
                return tmp;
            }
        }

        return User_Home() + "/" + relative_path;
    }
} // namespace

const char* PathsClass::Program_Path()
{
    if (ProgramPath.empty()) {
        ProgramPath = std::string("/vanilla-conquer/") + Suffix;
    }

    return ProgramPath.c_str();
}

const char* PathsClass::Data_Path()
{
    if (DataPath.empty()) {
        DataPath = std::string("/vanilla-conquer/") + Suffix;
    }

    return DataPath.c_str();
}

const char* PathsClass::User_Path()
{
    if (UserPath.empty()) {
        UserPath = std::string("/vanilla-conquer/") + Suffix;
    }

    return UserPath.c_str();
}

bool PathsClass::Create_Directory(const char* dirname)
{
    bool ret = true;

    if (dirname == nullptr) {
        return ret;
    }

    std::string temp = dirname;
    size_t pos = 0;
    do {
        pos = temp.find_first_of("/", pos + 1);
        if (mkdir(temp.substr(0, pos).c_str(), 0700) != 0) {
            if (errno != EEXIST) {
                ret = false;
                break;
            }
        }
    } while (pos != std::string::npos);

    return ret;
}

bool PathsClass::Is_Absolute(const char* path)
{
    return path != nullptr && path[0] == '/';
}

std::string PathsClass::Argv_Path(const char* cmd_arg)
{
    return std::string("/vanilla-conquer");
}
