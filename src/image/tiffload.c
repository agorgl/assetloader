#include "assets/image/imageload.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "tiff.h"
#include "tiffio.h"

struct tiff_callback_data {
    unsigned char* src;
    tsize_t size;
    unsigned char* cur_pos;
};

tsize_t tiff_read_cb(thandle_t handle, tdata_t buf, tsize_t sz)
{
    struct tiff_callback_data* cbdata = (struct tiff_callback_data*) handle;
    /* Read sz data to buf from cbdata->src */
    tsize_t available = (cbdata->src + cbdata->size) - cbdata->cur_pos;
    assert(sz <= available);
    memcpy(buf, cbdata->cur_pos, sz);
    cbdata->cur_pos += sz;
    return sz;
}

tsize_t tiff_write_cb(thandle_t handle, tdata_t buf, tsize_t sz)
{
    (void) handle; (void) buf; (void) sz;
    return 0;
}

int tiff_close_cb(thandle_t handle)
{
    (void) handle;
    return 0;
}

toff_t tiff_seek_cb(thandle_t handle, toff_t off, int whence)
{
    struct tiff_callback_data* cbdata = (struct tiff_callback_data*) handle;
    switch(whence) {
        case SEEK_SET:
            cbdata->cur_pos = cbdata->src + off;
            break;
        case SEEK_CUR:
            cbdata->cur_pos = cbdata->cur_pos + off;
            break;
        case SEEK_END:
            cbdata->cur_pos = cbdata->src + cbdata->size + off;
            break;
    }
    return cbdata->cur_pos - cbdata->src;
}

toff_t tiff_sz_cb(thandle_t handle)
{
    struct tiff_callback_data* cbdata = (struct tiff_callback_data*) handle;
    return cbdata->size;
}

static void tiff_warn_handler(thandle_t th, const char* module, const char* fmt, va_list args)
{
    /* TODO: implement with future AssetLoader trace function */
    (void) th;
    (void) module;
    (void) fmt;
    (void) args;
}

struct image* image_from_tiff(const unsigned char* data, size_t sz)
{
    /* Set warning handler */
    TIFFSetWarningHandler(0); /* Remove default first */
    TIFFSetWarningHandlerExt(tiff_warn_handler);

    /* Fill read callback info */
    struct tiff_callback_data cbdata;
    cbdata.src = (unsigned char*) data;
    cbdata.cur_pos = cbdata.src;
    cbdata.size = sz;

    /* Open from memory buffer */
    TIFF* tif = TIFFClientOpen(
        "MemBuf", "rm",
        (thandle_t)&cbdata,
        tiff_read_cb,
        tiff_write_cb,
        tiff_seek_cb,
        tiff_close_cb,
        tiff_sz_cb,
        0, 0);

    int width;
    int height;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);

    struct image* im = image_blank(width, height, 4);
    TIFFReadRGBAImage(tif, width, height, (uint32*)im->data, 0);

    TIFFClose(tif);
    return im;
}
