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

static bool dfs_initialized = false;

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
    int fp;
    char FullName[PATH_MAX];
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
	if (!strcmp(FullName, ""))
		return nullptr;
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
    if (fname[0] != '/')
	sprintf(FullName, "/%s", fname);
    else
	strcpy(FullName, fname);

    if (!dfs_initialized)
	if (dfs_init(0xB0401000) != DFS_ESUCCESS)
	  {
	    volatile int x = fname[0] / INT_MAX;
	    x = 4 / x;
	  }
	else
	  dfs_initialized = true;

    fp = dfs_open(FullName);
    return fp >= 0;
}

bool Find_File_Data_Libdragon::FindNext()
{
    return false;
}

void Find_File_Data_Libdragon::Close()
{
	dfs_close(fp);
	strcpy(FullName, "");
}

Find_File_Data* Find_File_Data::CreateFindData()
{
    return new Find_File_Data_Libdragon();
}
