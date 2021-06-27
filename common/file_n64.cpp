#include "file.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <fnmatch.h>
#include <libdragon.h>

#ifndef PATH_MAX
#define PATH_MAX 512
#endif

typedef struct
{
    uint32_t type;
    char filename[MAX_FILENAME_LEN+1];
} direntry_t;

static void maybe_initialize_dragonfs()
{
    static bool dfs_initialized = false;
    if (dfs_initialized)
        return;

    if (dfs_init(0xB1001000) != DFS_ESUCCESS)
    {
        volatile int x = ((int) dfs_initialized) / INT_MAX;
        x = 4 / x;
    }
    debug_init_isviewer();
    dfs_initialized = true;
}

class Find_File_Data_Libdragon : public Find_File_Data
{
public:
    Find_File_Data_Libdragon();
    virtual ~Find_File_Data_Libdragon();

    virtual const char* GetName() const;
    virtual unsigned long GetTime() const;

    virtual bool FindFirst(const char* fname);
    virtual bool FindNext();
    virtual void Close();

private:
    dir_t DirEntry;
    const char* FileFilter;
    char FullName[PATH_MAX];
    char DirName[PATH_MAX];
    bool FindNextWithFilter();
};

Find_File_Data_Libdragon::Find_File_Data_Libdragon()
{
	strcpy(FullName, "");
}

Find_File_Data_Libdragon::~Find_File_Data_Libdragon()
{
    Close();
}

const char* Find_File_Data_Libdragon::GetName() const
{
    if (FullName[0] == '\0')
        return nullptr;
    return FullName;
}

unsigned long Find_File_Data_Libdragon::GetTime() const
{
    return 0;
}

bool Find_File_Data_Libdragon::FindNextWithFilter()
{
    while (true) {
        int ret = dir_findnext("rom://", &DirEntry);
        if (ret != 0) {
            return false;
        }
        if (strstr(FileFilter, DirEntry.d_name)) {
            strcpy(FullName, DirName);
            strcat(FullName, DirEntry.d_name);
            break;
        }
    }

    return true;
}

bool Find_File_Data_Libdragon::FindFirst(const char* fname)
{
    maybe_initialize_dragonfs();
    Close();

    if (fname && fname[0] == '\0')
        return false;

    int ret;

    // split directory and file from the path
    char* fdir = strrchr((char*)fname, '/');
    if (fdir != nullptr) {
        strncat(DirName, fname, (fdir - fname + 1));
        FileFilter = fdir + 1;
    }
    else
        FileFilter = fname;

    ret = dir_findfirst("rom://", &DirEntry);
    if (!ret && strstr(FileFilter, DirEntry.d_name)) {
        strcpy(FullName, DirName);
        strcat(FullName, DirEntry.d_name);
        return true;
    }
    else
        return FindNextWithFilter();
}

bool Find_File_Data_Libdragon::FindNext()
{
    if (FileFilter[0] == '\0')
        return false;

    return FindNextWithFilter();
}

void Find_File_Data_Libdragon::Close()
{
    FullName[0] = '\0';
    DirName[0] = '\0';
}

Find_File_Data* Find_File_Data::CreateFindData()
{
    return new Find_File_Data_Libdragon();
}
