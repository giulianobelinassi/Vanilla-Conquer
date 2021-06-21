#include "file.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <fnmatch.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

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
    void* Directory;
    void* DirEntry;
    const char* FileFilter;
    char FullName[PATH_MAX];
    char DirName[PATH_MAX];

    bool FindNextWithFilter();
};

Find_File_Data_Libdragon::Find_File_Data_Libdragon()
    : Directory(nullptr)
    , DirEntry(nullptr)
{
}

Find_File_Data_Libdragon::~Find_File_Data_Libdragon()
{
    Close();
}

const char* Find_File_Data_Libdragon::GetName() const
{
    if (DirEntry == nullptr) {
        return nullptr;
    }
    return FullName;
}

unsigned long Find_File_Data_Libdragon::GetTime() const
{
    return 0;
}

bool Find_File_Data_Libdragon::FindNextWithFilter()
{
    return true;
}

bool Find_File_Data_Libdragon::FindFirst(const char* fname)
{
    return true;
}

bool Find_File_Data_Libdragon::FindNext()
{
    return false;
}

void Find_File_Data_Libdragon::Close()
{
    Directory = nullptr;
}

Find_File_Data* Find_File_Data::CreateFindData()
{
    return new Find_File_Data_Libdragon();
}
