#include "assets/image/imageload.h"
#include "assets/fileload.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>

struct image* image_from_mem_buf(const unsigned char* data, size_t sz, const char* hint) {
    if(strcmp(hint, "png") == 0) {
        return image_from_png(data, sz);
    } else if (strcmp(hint, "jpg") == 0 || strcmp(hint, "jpeg") == 0) {
        return image_from_jpeg(data, sz);
    } else if (strcmp(hint, "tiff") == 0) {
        return image_from_tiff(data, sz);
    } else if (strcmp(hint, "tga") == 0) {
        return image_from_tga(data, sz);
    }
    /* No image parser found */
    return 0;
}

struct image* image_from_file(const char* fpath) {
    /* Check file for existence */
    long filesz = filesize(fpath);
    if (filesz == -1)
        return 0;

    /* Gather file contents */
    unsigned char* data_buf = malloc(filesz);
    read_file_to_mem(fpath, data_buf, filesz);

    /* Parse image data from memory */
    const char* ext = get_filename_ext(fpath);
    struct image* im = image_from_mem_buf(data_buf, filesz, ext);
    free(data_buf);

    /* Return parsed image */
    return im;
}
