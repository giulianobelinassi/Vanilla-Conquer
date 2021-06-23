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

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

static const char *user_home_cstr = "/";

static std::string user_home;
static std::string *user_home_ptr = NULL;

namespace
{
    const std::string& User_Home()
    {
	return user_home;
    }

    std::string Get_Posix_Default(const char* env_var, const char* relative_path)
    {
	return user_home;
    }
} // namespace

const char* PathsClass::Program_Path()
{
    return "/vanillatd";
}

const char* PathsClass::Data_Path()
{
    return user_home_cstr;
}

const char* PathsClass::User_Path()
{
    return user_home_cstr;
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
    if (!user_home_ptr)
      {
	user_home = std::string(user_home_cstr);
	user_home_ptr = &user_home;
      }

    return user_home;
}
