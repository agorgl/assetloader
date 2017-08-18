#include "assets/image/imageload.h"
#include "assets/error.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <GL/gl.h>

/*===============================
 * KTX File Format Specification
 *===============================*/
/* KTX is a format for storing textures for OpenGL and OpenGL ES applications.
 * It is distinguished by the simplicity of the loader required to instantiate
 * a GL texture object from the file contents. */

#define KTX_MAGIC { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A }

struct ktx_header {
    /* The file identifier is a unique set of bytes that will differentiate the file from other types of files.
     * It consists of 12 bytes, as follows:
     *
     * Byte[12] file_identifier = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A }
     * This can also be expressed using C-style character definitions as:
     * Byte[12] file_identifier = { '«', 'K', 'T', 'X', ' ', '1', '1', '»', '\r', '\n', '\x1A', '\n' }
     *
     * The rationale behind the choice values in the identifier is based on the rationale for the identifier
     * in the PNG specification. This identifier both identifies the file as a KTX file and provides for
     * immediate detection of common file-transfer problems.
     *
     * Byte  [0]    Is chosen as a non-ASCII value to reduce the probability that a text file may be
     *              misrecognized as a KTX file.
     * Byte  [0]    Also catches bad file transfers that clear bit 7.
     * Bytes [1..6] Identify the format, and are the ascii values for the string "KTX 11".
     * Byte  [7] is For aesthetic balance with byte 1 (they are a matching pair of double-angle quotation marks).
     * Bytes [8..9] Form a CR-LF sequence which catches bad file transfers that alter newline sequences.
     * Byte  [10]   Is a control-Z character, which stops file display under MS-DOS, and further
     *              reduces the chance that a text file will be falsely recognised.
     * Byte  [11]   Is a final line feed, which checks for the inverse of the CR-LF translation problem. */
    uint8_t  identifier[12];
    /* Endianness contains the number 0x04030201 written as a 32 bit integer. If the file is little endian then
     * this is represented as the bytes 0x01 0x02 0x03 0x04. If the file is big endian then this is represented as
     * the bytes 0x04 0x03 0x02 0x01. When reading endianness as a 32 bit integer produces the value 0x04030201 then
     * the endianness of the file matches the the endianness of the program that is reading the file and no conversion
     * is necessary. When reading endianness as a 32 bit integer produces the value 0x01020304 then the endianness of
     * the file is opposite the endianness of the program that is reading the file, and in that case the program
     * reading the file must endian convert all header bytes to the endianness of the program (i.e. a little endian
     * program must convert from big endian, and a big endian program must convert to little endian). */
    uint32_t endianness;
    /* For compressed textures, gl_type must equal 0. For uncompressed textures, gl_type specifies the type parameter
     * passed to glTex{,Sub}Image*D, usually one of the values from table 8.2 of the OpenGL 4.4 specification
     * [OPENGL44] (UNSIGNED_BYTE, UNSIGNED_SHORT_5_6_5, etc.) */
    uint32_t gl_type;
    /* Specifies the data type size that should be used when endianness conversion is required for the texture data
     * stored in the file. If gl_type is not 0, this should be the size in bytes corresponding to gl_type. For texture
     * data which does not depend on platform endianness, including compressed texture data, gl_type_size must equal 1. */
    uint32_t gl_type_size;
    /* For compressed textures, gl_format must equal 0. For uncompressed textures, gl_format specifies the format
     * parameter passed to glTex{,Sub}Image*D, usually one of the values from table 8.3 of the OpenGL 4.4 specification
     * [OPENGL44] (RGB, RGBA, BGRA, etc.) */
    uint32_t gl_format;
    /* For compressed textures, gl_internal_format must equal the compressed internal format, usually one of the values
     * from table 8.14 of the OpenGL 4.4 specification [OPENGL44]. For uncompressed textures, gl_internal_format specifies
     * the internal_format parameter passed to glTexStorage*D or glTexImage*D, usually one of the sized internal formats
     * from tables 8.12 & 8.13 of the OpenGL 4.4 specification [OPENGL44]. The sized format should be chosen to match
     * the bit depth of the data provided. gl_internal_format is used when loading both compressed and uncompressed textures,
     * except when loading into a context that does not support sized formats, such as an unextended OpenGL ES 2.0
     * context where the internalformat parameter is required to have the same value as the format parameter. */
    uint32_t gl_internal_format;
    /* For both compressed and uncompressed textures, gl_base_internal_format specifies the base internal format
     * of the texture, usually one of the values from table 8.11 of the OpenGL 4.4 specification [OPENGL44]
     * (RGB, RGBA, ALPHA, etc.). For uncompressed textures, this value will be the same as gl_format and is used
     * as the internal_format parameter when loading into a context that does not support sized formats, such as
     * an unextended OpenGL ES 2.0 context. */
    uint32_t gl_base_internal_format;
    /* The size of the texture image for level 0, in pixels.
     * No rounding to block sizes should be applied for block compressed textures.
     * For 1D textures pixel_height and pixel_depth must be 0.
     * For 2D and cube textures pixel_depth must be 0. */
    uint32_t pixel_width;
    uint32_t pixel_height;
    uint32_t pixel_depth;
    /* Specifies the number of array elements.
     * If the texture is not an array texture, number_of_array_elements must equal 0. */
    uint32_t number_of_array_elements;
    /* Specifies the number of cubemap faces.
     * For cubemaps and cubemap arrays this should be 6.
     * For non cubemaps this should be 1.
     * Cube map faces are stored in the order: +X, -X, +Y, -Y, +Z, -Z. */
    uint32_t number_of_faces;
    /* Must equal 1 for non-mipmapped textures.
     * For mipmapped textures, it equals the number of mipmaps.
     * Mipmaps are stored in order from largest size to smallest size.
     * The first mipmap level is always level 0.
     * A KTX file does not need to contain a complete mipmap pyramid.
     * If number_of_mipmap_levels equals 0, it indicates that a full mipmap pyramid
     * should be generated from level 0 at load time (this is usually not allowed for compressed formats). */
    uint32_t number_of_mipmap_levels;
    /* An arbitrary number of key/value pairs may follow the header.
     * This can be used to encode any arbitrary data.
     * The bytes_of_key_value_data field indicates the total number of bytes of key/value data
     * including all key_and_value_byte_size fields, all key_and_value fields, and all value_padding fields.
     * The file offset of the first image_size field is located at the file offset of the
     * bytes_of_key_value_data field plus the value of the bytes_of_key_value_data field plus 4. */
    uint32_t bytes_of_key_value_data;
};

#if 0
for each key_value_pair that fits in bytes_of_key_value_data
    /* The number of bytes of combined key and value data in one key/value pair following the header.
     * This includes the size of the key, the NUL byte terminating the key, and all the bytes of data in the value.
     * If the value is a UTF-8 string it should be NUL terminated and the key_and_value_byte_size should include
     * the NUL character (but code that reads KTX files must not assume that value fields are NUL terminated).
     * key_and_value_byte_size does not include the bytes in value_padding. */
    uint32 key_and_value_byte_size

    /* Contains 2 separate sections. First it contains a key encoded in UTF-8.
     * The key must be terminated by a NUL character (a single 0x00 byte).
     * Keys that begin with the 3 ascii characters 'KTX' or 'ktx' are reserved and must not
     * be used except as described by this spec (this version of the KTX spec defines a single key).
     * Immediately following the NUL character that terminates the key is the Value data.
     * The Value data may consist of any arbitrary data bytes. Any byte value is allowed.
     * It is encouraged that the value be a NUL terminated UTF-8 string, but this is not required.
     * If the Value data is binary, it is a sequence of bytes rather than of words.
     * It is up to the vendor defining the key to specify how those bytes are to be interpreted
     * (including the endianness of any encoded numbers).
     * If the Value data is a string of bytes then the NUL termination should be included in
     * the key_and_value_byte_size byte count (but programs that read KTX files must not rely on this). */
    byte key_and_value[key_and_value_byte_size]

    /* Contains between 0 and 3 bytes to ensure that the byte
     * following the last byte in value_padding is at a file offset that is a multiple of 4.
     * This ensures that every key_and_value_byte_size field, and the first imageSize field, is 4 byte aligned.
     * This padding is included in the bytes_of_key_value_data field but not
     * the individual key_and_value_byte_size fields. */
    byte value_padding[3 - ((key_and_value_byte_size + 3) % 4)]
end

for each mipmap_level in number_of_mipmap_levels(1)
    /* For most textures image_size is the number of bytes of pixel data in the current LOD level.
     * This includes all z slices, all faces, all rows (or rows of blocks) and all pixels (or blocks)
     * in each row for the mipmap level. It does not include any bytes in mip_padding.
     * The exception is non-array cubemap textures (any texture where number_of_faces is 6 and number_of_array_elements is 0).
     * For these textures image_size is the number of bytes in each face of the texture for the current LOD level,
     * not including bytes in cube_padding or mip_padding. */
    uint32 image_size

    for each array_element in number_of_array_elements(1)
       for each face in number_of_faces
           for each z_slice in pixel_depth(1)
               for each row or row_of_blocks in pixel_height(1)
                   for each pixel or block_of_pixels in pixel_width
                       byte data[format-specific-number-of-bytes](2)
                   end
               end
           end

           /* For non-array cubemap textures (any texture where number_of_faces is 6 and number_of_array_elements is 0)
            * cube_padding contains between 0 and 3 bytes to ensure that the data in each face begins at a file offset
            * that is a multiple of 4. In all other cases cube_padding is empty (0 bytes long). */
           byte cube_padding[0-3]
       end
    end

    /* Between 0 and 3 bytes to make sure that all image_size fields are at a file offset that is a multiple of 4. */
    byte mip_padding[3 - ((image_size + 3) % 4)]
end

(1) Replace with 1 if this field is 0.
(2) Uncompressed texture data matches a GL_UNPACK_ALIGNMENT of 4.

/* -= General comments =- */

/* The unpack alignment is 4. I.e. uncompressed pixel data is packed according to the rules described
 * in section 8.4.4.1 of the OpenGL 4.4 specification [OPENGL44] for a GL_UNPACK_ALIGNMENT of 4.
 * Values listed in tables referred to in the OpenGL 4.4 specification [OPENGL44] may be supplemented by extensions.
 * The references are given as examples and do not imply that all of those texture types can be loaded in OpenGL ES
 * or earlier versions of OpenGL. */

/* Texture data in a KTX file are arranged so that the first pixel in the data stream for each face and/or array element
 * is closest to the origin of the texture coordinate system. In OpenGL that origin is conventionally described as being
 * at the lower left, but this convention is not shared by all image file formats and content creation tools, so there
 * is abundant room for confusion. */
#endif

static inline int is_ktx(const void* data)
{
    const unsigned char magic[12] = KTX_MAGIC;
    struct ktx_header* h = (struct ktx_header*)data;
    return memcmp(h->identifier, magic, sizeof(magic)) == 0;
}

static inline const void* mip_data_and_size(const void* data, unsigned int face_idx, unsigned int mip_idx, uint32_t* sz)
{
    const void* result = 0;
    const struct ktx_header* h = data;
    const void* ptr = data + sizeof(struct ktx_header) + h->bytes_of_key_value_data;
    for (unsigned int i = 0; i < mip_idx + 1; ++i) {
        const unsigned int face_size = *(unsigned int*)ptr;
        /* Set result to start of face data within mip level */
        result = ptr + 4 + (face_size * face_idx);
        *sz = face_size;
        /* Advance to start of next mip level */
        int mip_padding = 3 - ((face_size + 3) % 4);
        ptr += 4 + (face_size * h->number_of_faces) + mip_padding;
    }
    return result;
}

struct image* image_from_ktx(const unsigned char* data, size_t sz)
{
    (void)sz;

    /* Header */
    struct ktx_header* h = (struct ktx_header*)data;
    /* Check if ktx */
    if (!is_ktx(data)) {
        set_last_asset_load_error("Ktx identifier mismatch");
        return 0;
    }
    /* Check if endianness matches */
    if (h->endianness != 0x04030201) {
        set_last_asset_load_error("Mismatching endianness!");
        return 0;
    }
    /* Check if array texture */
    if (h->number_of_array_elements > 0) {
        set_last_asset_load_error("Array textures unsupported!");
        return 0;
    }
    /* Check number of faces */
    if (!(h->number_of_faces == 1 || h->number_of_faces == 6)) {
        set_last_asset_load_error("Incorrect number of faces!");
        return 0;
    }

    /* Get mipmap 0 */
    uint32_t image_size = 0;
    const void* image_data = mip_data_and_size(data, 0, 0, &image_size);

    /* Create image */
    const int level = 0; /* Load base mipmap */
    struct image* im = calloc(1, sizeof(struct image));
    im->width = (h->pixel_width  >> level);
    im->height = (h->pixel_height >> level);
    im->channels = h->gl_format == GL_RGB ? 3 : 4;
    if (h->gl_type != 0) {
        im->compression_type = 0;
        im->data_sz = im->width * im->height * im->channels * sizeof(unsigned char);
        im->data = calloc(1, im->data_sz);
    } else {
        im->compression_type = h->gl_internal_format;
        im->data_sz = image_size;
        im->data = calloc(1, im->data_sz);
    }

    /* Copy image data */
    memcpy(im->data, image_data, image_size);

    return im;
}
