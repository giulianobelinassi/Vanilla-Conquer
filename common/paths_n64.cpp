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
#include <pwd.h>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#if defined(__linux__)
#include <linux/limits.h>
#else
#include <limits.h>
#endif

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#if defined(__APPLE__)
#define _DARWIN_BETTER_REALPATH
#include <mach-o/dyld.h>
#include <TargetConditionals.h>
#elif defined(__DragonFly__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__)
#include <sys/sysctl.h>
#endif

namespace
{
    const std::string& User_Home()
    {
	return std::string("");
    }

    std::string Get_Posix_Default(const char* env_var, const char* relative_path)
    {
      return std::string("");
    }
} // namespace

#if defined(__sun)
#define PROC_SELF_EXE "/proc/self/path/a.out"
#else
#define PROC_SELF_EXE "/proc/self/exe"
#endif

const char* PathsClass::Program_Path()
{
    return "";
}

const char* PathsClass::Data_Path()
{
	return "";
}

const char* PathsClass::User_Path()
{
    return "";
}

bool PathsClass::Create_Directory(const char* dirname)
{
    return true;
}

bool PathsClass::Is_Absolute(const char* path)
{
    return true;
}

std::string PathsClass::Argv_Path(const char* cmd_arg)
{
    return std::string("");
}
