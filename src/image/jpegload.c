#include "assets/image/imageload.h"
#include "assets/error.h"
#include <stdio.h>
#include "jpeglib.h"

struct image* image_from_jpeg(const unsigned char* data, size_t sz) {
    /* Create decompress struct */
    struct jpeg_decompress_struct cinfo;

    /* Set the error handling mechanism */
    struct jpeg_error_mgr err_mgr;
    cinfo.err = jpeg_std_error(&err_mgr);

    /* Initialize decompression object */
    jpeg_create_decompress(&cinfo);

    /* Set memory buffer as source */
    jpeg_mem_src(&cinfo, (unsigned char*) data, sz);
    if (!jpeg_read_header(&cinfo, TRUE)) {
        jpeg_destroy_decompress(&cinfo);
        set_last_asset_load_error("Incorrect jpeg header");
        return 0;
    }

    /* Start the decompression procedure */
    jpeg_start_decompress(&cinfo);

    /* Gather image data info */
    int width = cinfo.output_width;
    int height = cinfo.output_height;
    /* int pixel_size = cinfo.output_components; */

    /* Allocate and fill the image object */
    struct image* im = image_blank(width, height, 3);

    /* Read by lines */
    unsigned char* bufp = im->data;
    int nsamples;
    while (cinfo.output_scanline < cinfo.output_height) {
        nsamples = jpeg_read_scanlines(&cinfo, (JSAMPARRAY)&bufp, 1);
        bufp += nsamples* cinfo.image_width * cinfo.num_components;
    }

    /* Finish decompression procedure */
    jpeg_finish_decompress(&cinfo);

    /* Destroy decompress struct */
    jpeg_destroy_decompress(&cinfo);

    /* Return loaded image */
    return im;
}
