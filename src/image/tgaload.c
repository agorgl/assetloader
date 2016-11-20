#include "assets/image/imageload.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

enum tga_data_type
{
    TGA_DATA_TYPE_NULL      = 0,  /* No image data included */
    TGA_DATA_TYPE_CMAP      = 1,  /* Uncompressed, color-mapped images */
    TGA_DATA_TYPE_RGB       = 2,  /* Uncompressed, true-color image */
    TGA_DATA_TYPE_MONO      = 3,  /* Uncompressed, black-and-white image */
    TGA_DATA_TYPE_RLE_CMAP  = 9,  /* Run-length encoded, color-mapped image */
    TGA_DATA_TYPE_RLE_RGB   = 10, /* Run-length encoded, true-color image */
    TGA_DATA_TYPE_RLE_MONO  = 11, /* Run-length encoded, black-and-white image */
    TGA_DATA_TYPE_CMP_CMAP  = 32, /* Compressed (Huffman/Delta/RLE) color-mapped image */
    TGA_DATA_TYPE_CMP_CMAP4 = 33  /* Compressed (Huffman/Delta/RLE) color-mapped four pass image */
};

struct tga_header
{
    uint8_t  id_length;         /* The length of a string located after the header */
    uint8_t  colour_map_type;   /* Whether a color map is included */
    uint8_t  data_type_code;    /* Compression and color type */
    uint16_t colour_map_origin; /* First entry index (offset into the color map table) */
    uint16_t colour_map_length; /* Color map length (number of entries) */
    uint8_t  colour_map_depth;  /* Color map entry size, in bits (number of bits per pixel) */
    uint16_t x_origin;          /* X-origin of image (absolute coordinate of lower-left corner) */
    uint16_t y_origin;          /* Y-origin of image (as for X-origin) */
    uint16_t width;             /* Image width */
    uint16_t height;            /* Image height */
    uint8_t  bits_per_pixel;    /* Bits per pixel [0]* */
    uint8_t  image_descriptor;  /* Image descriptor, bits 0-3 give the alpha channel depth bits 4-5 give direction */
};

/*
   [0]: When 23 or 32 the normal conventions apply. For 16 bits each color component is stored as 5 bits
   and the remaining bit is a binary alpha value. The color components are converted into single byte components
   by simply shifting each component up by 3 bits (multiply by 8)
 */

#define member_size(type, member) sizeof(((type *)0)->member)

#define parse_field(field, offset)            \
    memcpy(                                   \
        &header.field,                        \
        begin + offset,                       \
        member_size(struct tga_header, field) \
    );

static void image_flip(struct image* im)
{
    size_t stride = im->width * im->channels;
    unsigned char* row_buf = malloc(stride);

    for (int i = 0; i < im->height / 2; ++i) {
        memcpy(row_buf, im->data + i * stride, stride);
        memcpy(im->data + i * stride, im->data + (im->height - i - 1) * stride, stride);
        memcpy(im->data + (im->height - i - 1) * stride, row_buf, stride);
    }

    free(row_buf);
}

struct image* image_from_tga(const unsigned char* data, size_t sz) {
    (void) sz;

    // Parse header
    struct tga_header header;
    memset(&header, 0, sizeof(struct tga_header));
    unsigned char* begin = (unsigned char*)data;

    parse_field(id_length, 0)
    parse_field(colour_map_type, 1)
    parse_field(data_type_code, 2)
    parse_field(colour_map_origin, 3)
    parse_field(colour_map_length, 5)
    parse_field(colour_map_depth, 7)
    parse_field(x_origin, 8)
    parse_field(y_origin, 10)
    parse_field(width, 12)
    parse_field(height, 14)
    parse_field(bits_per_pixel, 16)
    parse_field(image_descriptor, 17)

    /* Guard for unsupported forms */
    if (header.data_type_code != TGA_DATA_TYPE_RGB
     && header.data_type_code != TGA_DATA_TYPE_RLE_RGB)
        return 0;

    /* Gather image info */
    uint32_t width = header.width;
    uint32_t height = header.height;
    uint32_t pixel_sz = header.bits_per_pixel / 8;

    /* Allocate and fill the image object */
    struct image* im = image_blank(width, height, header.bits_per_pixel / 8);

    /* Pointer to the data */
    unsigned char* image_data = begin + 18 + header.id_length;

    if (header.data_type_code == TGA_DATA_TYPE_RGB) {
        /* Copy data */
        memcpy(im->data, image_data, width * height * pixel_sz);
    } else if (header.data_type_code == TGA_DATA_TYPE_RLE_RGB) {
        /* Number of bytes read from source */
        size_t cur_byte = 0;
        /* Number of bytes written to dest */
        size_t i = 0;
        while (i < width * height * pixel_sz) {
            /* Get header */
            unsigned char id = image_data[cur_byte++];
            if (id & (1 << 7)) { /* Bit 7 set, its a run length packet */
                /* The lower 7 bits are the repetition count minus 1 */
                unsigned char payload = (id ^ (1 << 7)) + 1;
                /* Copy repeated data */
                while (payload > 0) {
                    im->data[i] = image_data[cur_byte];
                    im->data[i + 1] = image_data[cur_byte + 1];
                    im->data[i + 2] = image_data[cur_byte + 2];
                    if (im->channels == 4)
                        im->data[i + 3] = image_data[cur_byte + 3];
                    i += im->channels;
                    --payload;
                }
                cur_byte += im->channels;
            } else { /* Bit 7 not set, its a raw packet */
                /* The lower 7 bits are the number of pixels minus 1 */
                unsigned char payload = id + 1;
                memcpy(im->data + i, image_data + cur_byte, payload * im->channels);
                i += payload * im->channels;
                cur_byte += payload * im->channels;
            }
        }
    }

    /* Convert BGR to RGB */
    for (size_t i = 0; i < width * height * pixel_sz; i += im->channels) {
        unsigned char tmp = im->data[i];
        im->data[i] = im->data[i + 2];
        im->data[i + 2] = tmp;
    }

    /* Get screen origin bit (0 = lower left, 1 = upper left) */
    unsigned char screen_origin = header.image_descriptor & (1 << 5);
    /* Flip image on y axis */
    if (screen_origin) {
        image_flip(im);
    }

    /* Return loaded image */
    return im;
}
