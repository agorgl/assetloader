#include "assets/model/modelload.h"
#include "assets/fileload.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>

struct model* model_from_mem_buf(const unsigned char* data, size_t sz, const char* hint)
{
    if (strcmp(hint, "obj") == 0)
        return model_from_obj(data, sz);
    else if (strcmp(hint, "fbx") == 0)
        return model_from_fbx(data, sz);
    else if (strcmp(hint, "iqm") == 0)
        return model_from_iqm(data, sz);
    /* No image parser found */
    return 0;
}

struct model* model_from_file(const char* fpath)
{
    /* Check file for existence */
    long filesz = filesize(fpath);
    if (filesz == -1)
        return 0;

    /* Gather file contents */
    unsigned char* data_buf = malloc(filesz);
    read_file_to_mem(fpath, data_buf, filesz);

    /* Parse model data from memory */
    const char* ext = get_filename_ext(fpath);
    struct model* m = model_from_mem_buf(data_buf, filesz, ext);
    free(data_buf);

    /* Return parsed image */
    return m;
}
