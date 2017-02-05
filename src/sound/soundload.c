#include "assets/sound/soundload.h"
#include "assets/fileload.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>

struct sound* sound_from_mem_buf(const unsigned char* data, size_t sz, const char* hint) {
    if(strcmpi(hint, "wav") == 0) {
        return sound_from_wav(data, sz);
    } else if (strcmpi(hint, "ogg") == 0) {
        return sound_from_ogg(data, sz);
    }
    /* No image parser found */
    return 0;
}

struct sound* sound_from_file(const char* fpath) {
    /* Check file for existence */
    long filesz = filesize(fpath);
    if (filesz == -1)
        return 0;

    /* Gather file contents */
    unsigned char* data_buf = malloc(filesz);
    read_file_to_mem(fpath, data_buf, filesz);

    /* Parse image data from memory */
    const char* ext = get_filename_ext(fpath);
    struct sound* snd = sound_from_mem_buf(data_buf, filesz, ext);
    free(data_buf);

    /* Return parsed image */
    return snd;
}
