#include "util.h"
#include <string.h>
#include <plat.h>

const char* get_filename_ext(const char* filename)
{
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename)
        return "";
    return dot + 1;
}

int strcmpi(const char* s1, const char* s2)
{
#ifdef OS_WINDOWS
    return _stricmp(s1, s2);
#else
    return strcasecmp(s1, s2);
#endif
}
