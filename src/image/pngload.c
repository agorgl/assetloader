#include "assets/image/imageload.h"
#include "assets/error.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <png.h>

/* Custom callback userdata struct */
struct png_read_callback_data {
    unsigned char* src_buf;
    size_t sz;
};

/* Custom read function for reading from memory buffer */
void png_read_callback_fn(png_struct* png, png_byte* data, png_size_t length) {
    struct png_read_callback_data* cbdata = (struct png_read_callback_data*) png_get_io_ptr(png);
    memcpy(data, cbdata->src_buf, length);
    cbdata->src_buf += length;
}

struct image* image_from_png(const unsigned char* data, size_t sz) {
    /* Check file data header */
    if (png_sig_cmp(data, 0, 8)) {
        set_last_asset_load_error("Incorrect png header");
        return 0;
    }

    /* Allocate needed structures */
    png_struct* png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    png_info* info = png_create_info_struct(png);
    png_info* end_info = png_create_info_struct(png);

    /* Register custom reader function */
    struct png_read_callback_data cbdata;
    cbdata.src_buf = (unsigned char*)data;
    cbdata.sz = sz;
    png_set_read_fn(png, (void*)&cbdata, png_read_callback_fn);

    /* Read image info */
    png_set_sig_bytes(png, 0);
    png_read_info(png, info);

    /* Gather image info */
    int width = png_get_image_width(png, info);
    int height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    /* Read any color_type into 8bit depth, RGBA format */
    if(bit_depth == 16)
        png_set_strip_16(png);

    if(color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    /* PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth */
    if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    /* Expand any tRNS channels to alpha */
    if(png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    /* These color_type don't have an alpha channel then fill it with 0xff */
    if(color_type == PNG_COLOR_TYPE_RGB ||
       color_type == PNG_COLOR_TYPE_GRAY ||
       color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    /* Expand grayscale to rgb */
    if(color_type == PNG_COLOR_TYPE_GRAY ||
       color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    /* Update info struct */
    png_read_update_info(png, info);

    /* Get the number of channels (should be 4!) */
    int channels = png_get_channels(png, info);
    assert(channels == 4);

    /* Image to be returned */
    struct image* im = image_blank(width, height, channels);

    /* Read by row */
    png_byte** row_ptrs = malloc(height * sizeof(png_byte*));
    const size_t stride = png_get_rowbytes(png, info);
    for (int i = 0; i < height; ++i) {
        int q = (height - i - 1) * stride;
        row_ptrs[i] = im->data + q;
    }
    png_read_image(png, row_ptrs);
    free(row_ptrs);

    /* Free allocated structures */
    png_destroy_read_struct(&png, &info, &end_info);

    /* Return read image */
    return im;
}
