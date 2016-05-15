#include "assets/sound/soundload.h"
#include <stdlib.h>
#include <string.h>
#include <ogg/os_types.h>
#include <ogg/ogg.h>
#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

/* Callback data struct */
struct ogg_callback_data {
    unsigned char* begin;
    unsigned char* cur;
    size_t size;
};

static size_t ogg_read_cb(
    void* dest,        /* Ptr to the data that vorbis file needs (to be feeded) */
    size_t bsize,      /* Byte size on this particular system */
    size_t sz_to_read, /* Maximum number of items to be read */
    void* cbdata) {    /* A pointer to the callback data */
    struct ogg_callback_data* cbd = (struct ogg_callback_data*)(cbdata);

    /* Calculate size to read */
    size_t space_to_eof = cbd->size - (cbd->cur - cbd->begin);
    size_t actual_size_to_read = 0;
    if ((sz_to_read * bsize) < space_to_eof)
        actual_size_to_read = (sz_to_read * bsize);
    else
        actual_size_to_read = space_to_eof;

    /* Copy data from memory buffer */
    memcpy(dest, cbd->cur, actual_size_to_read);
    cbd->cur += actual_size_to_read;
    return actual_size_to_read;
}

static int ogg_seek_cb(void* cbdata, ogg_int64_t offset, int type) {
    struct ogg_callback_data* cbd = (struct ogg_callback_data*)(cbdata);
    switch (type) {
        case SEEK_CUR: {
            cbd->cur += offset;
            break;
        }
        case SEEK_END:
            cbd->cur = cbd->begin + cbd->size - offset;
            break;
        case SEEK_SET:
            cbd->cur = cbd->begin + offset;
            break;
        default:
            return -1;
    }
    /* Lower limit */
    if (cbd->cur < cbd->begin) {
        cbd->cur = cbd->begin;
        return -1;
    }
    /* Upper limit */
    if (cbd->cur > cbd->begin + cbd->size) {
        cbd->cur = cbd->begin + cbd->size;
        return -1;
    }
    return 0;
}

static long ogg_tell_cb(void* cbdata) {
    struct ogg_callback_data* cbd = (struct ogg_callback_data*)(cbdata);
    return (long)(cbd->cur - cbd->begin);
}

static int ogg_close_cb(void* cbdata) {
    (void) cbdata;
    return 0;
}

struct sound* sound_from_ogg(const unsigned char* data, size_t sz) {
    /* Initialize callback data */
    struct ogg_callback_data cbdata;
    cbdata.begin = (unsigned char*) data;
    cbdata.cur = cbdata.begin;
    cbdata.size = sz;

    /* Setup custom processing callbacks */
    ov_callbacks callbacks;
    /* Feeds vorbis decoder with data from the memory buffer */
    callbacks.read_func = ogg_read_cb;
    callbacks.seek_func = ogg_seek_cb;
    callbacks.tell_func = ogg_tell_cb;
    callbacks.close_func = ogg_close_cb;

    /* Begin decompression */
    OggVorbis_File oggfile;
    memset(&oggfile, 0, sizeof(OggVorbis_File));
    ov_open_callbacks((void*)(&cbdata), &oggfile, 0, 0, callbacks);

    /* Allocate sound structure */
    struct sound* snd = malloc(sizeof(struct sound));

    /* Gather audio info */
    vorbis_info* vi = ov_info(&oggfile, -1);
    snd->channels = vi->channels;
    snd->samplerate = vi->rate;
    snd->bits_per_sample = 16;
    snd->data_sz = ov_pcm_total(&oggfile, -1) * vi->channels * 2;
    /* Allocate destination buffer */
    snd->data = malloc(snd->data_sz);

    /* Decode data */
    unsigned long bytes = 0;
    while (bytes < snd->data_sz) {
        int bitstream;
        long r = ov_read(&oggfile, (char*)(snd->data + bytes), snd->data_sz - bytes, 0, 2, 1, &bitstream);
        if (r > 0)
            bytes += r;
        else {
            /* Error? */
        }
    }

    /* Release decompression object */
    ov_clear(&oggfile);

    return snd;
}
