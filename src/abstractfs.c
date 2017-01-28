#include "assets/abstractfs.h"
#include <assert.h>
#include <physfs.h>

void afs_init()
{
    PHYSFS_init(0);
}

int afs_initialized()
{
    return PHYSFS_isInit();
}

void afs_deinit()
{
    PHYSFS_deinit();
}

void afs_mount(const char* dir, const char* mountpoint, int append)
{
    assert(afs_initialized());
    PHYSFS_mount(dir, mountpoint, append);
}

int afs_exists(const char* fname)
{
    return PHYSFS_exists(fname);
}

long afs_file_length(const char* fname)
{
    PHYSFS_File* fp = PHYSFS_openRead(fname);
    if (!fp)
        return -1;
    long flen = PHYSFS_fileLength(fp);
    PHYSFS_close(fp);
    return flen;
}

int afs_read_file_to_mem(const char* fname, unsigned char* buf, size_t buf_sz)
{
    PHYSFS_File* fp = PHYSFS_openRead(fname);
    if (!fp)
        return 0;
    PHYSFS_read(fp, buf, 1, buf_sz);
    PHYSFS_close(fp);
    return 1;
}
