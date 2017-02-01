#include "assets/font/texture_font.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utf8_utils.h"
#include "ttf_font.h"

#ifdef HAS_DISTANCE_FIELD
#include "distance_field.h"
#endif

/*-----------------------------------------------------------------
 * Texture glyph
 *-----------------------------------------------------------------*/
texture_glyph_t* texture_glyph_new(void)
{
    texture_glyph_t* self = (texture_glyph_t*) malloc(sizeof(texture_glyph_t));
    self->codepoint = -1;
    self->width = 0;
    self->height = 0;
    self->rendermode = RENDER_NORMAL;
    self->outline_thickness = 0.0;
    self->offset_x = 0;
    self->offset_y = 0;
    self->advance_x = 0.0;
    self->advance_y = 0.0;
    self->s0 = 0.0;
    self->t0 = 0.0;
    self->s1 = 0.0;
    self->t1 = 0.0;
    vector_init(&self->kerning, sizeof(kerning_t));
    return self;
}

void texture_glyph_delete(texture_glyph_t* self)
{
    assert(self);
    vector_destroy(&self->kerning);
    free(self);
}

float texture_glyph_get_kerning(const texture_glyph_t* self, const char* codepoint)
{
    size_t i;
    uint32_t ucodepoint = utf8_to_utf32(codepoint);
    assert(self);
    for (i = 0; i < self->kerning.size; ++i) {
        kerning_t* kerning = (kerning_t*) vector_at((struct vector*)&self->kerning, i);
        if (kerning->codepoint == ucodepoint)
            return kerning->kerning;
    }
    return 0;
}

/*-----------------------------------------------------------------
 * Texture font
 *-----------------------------------------------------------------*/
static int texture_font_init(texture_font_t* self)
{
    /* Proxy */
    switch (self->type) {
        case TEXTURE_FONT_TYPE_TRUETYPE:
            return ttf_font_init(self);
        case TEXTURE_FONT_TYPE_BITMAP:
        default:
            assert(0 && "Invalid font type");
            break;
    }
    return 0;
}

/* Extension without the dot */
static int texture_font_type_from_extension(const char* ext)
{
    assert(ext);
    if (strncmp(ext, "ttf", 3) == 0)
        return TEXTURE_FONT_TYPE_TRUETYPE;
    else if (strncmp(ext, "pcf", 3) == 0)
        return TEXTURE_FONT_TYPE_BITMAP;
    return -1;
}

texture_font_t* texture_font_new_from_file(texture_atlas_t* atlas, const float pt_size, const char* filename)
{
    texture_font_t* self;
    assert(filename);

    self = calloc(1, sizeof(*self));
    self->atlas = atlas;
    self->size = pt_size;
    self->location = TEXTURE_FONT_FILE;
    self->filename = strdup(filename);

    /* Set type by extension */
    const char* ext = strrchr(self->filename, '.');
    self->type = texture_font_type_from_extension(ext + 1);

    if (texture_font_init(self)) {
        texture_font_delete(self);
        return 0;
    }

    return self;
}

texture_font_t* texture_font_new_from_memory(
    texture_atlas_t* atlas,
    float pt_size,
    const void* memory_base,
    size_t memory_size,
    int font_type
)
{
    texture_font_t* self;
    assert(memory_base);
    assert(memory_size);

    self = calloc(1, sizeof(*self));
    self->atlas = atlas;
    self->size = pt_size;
    self->location = TEXTURE_FONT_MEMORY;
    self->memory.base = memory_base;
    self->memory.size = memory_size;
    self->type = font_type;

    if (texture_font_init(self)) {
        texture_font_delete(self);
        return 0;
    }

    return self;
}

void texture_font_delete(texture_font_t* self)
{
    size_t i;
    texture_glyph_t* glyph;
    assert(self);

    if (self->location == TEXTURE_FONT_FILE && self->filename)
        free(self->filename);
    for (i = 0; i < self->glyphs.size; ++i) {
        glyph = *(texture_glyph_t**)vector_at(&self->glyphs, i);
        texture_glyph_delete(glyph);
    }

    vector_destroy(&self->glyphs);
    free(self);
}

texture_glyph_t* texture_font_find_glyph(texture_font_t* self, const char* codepoint)
{
    size_t i;
    texture_glyph_t* glyph;
    uint32_t ucodepoint = utf8_to_utf32(codepoint);

    for (i = 0; i < self->glyphs.size; ++i) {
        glyph = *(texture_glyph_t**)vector_at(&self->glyphs, i);
        /* If codepoint is -1, we don't care about outline type or thickness */
        if ((glyph->codepoint == ucodepoint) &&
            (((int32_t)ucodepoint == -1) ||
             ((glyph->rendermode == self->rendermode) &&
              (glyph->outline_thickness == self->outline_thickness)))) {
            return glyph;
        }
    }

    return 0;
}

int texture_font_load_glyph(texture_font_t* self, const char* codepoint)
{
    /* Proxy */
    switch (self->type) {
        case TEXTURE_FONT_TYPE_TRUETYPE:
            return ttf_font_load_glyph(self, codepoint);
        case TEXTURE_FONT_TYPE_BITMAP:
        default:
            break;
    }
    return 0;
}

size_t texture_font_load_glyphs(texture_font_t* self, const char* codepoints)
{
    size_t i;
    /* Load each glyph */
    for (i = 0; i < utf8_strlen(codepoints); i += utf8_surrogate_len(codepoints + i)) {
        if (!texture_font_load_glyph(self, codepoints + i))
            return utf8_strlen(codepoints + i);
    }
    return 0;
}

texture_glyph_t* texture_font_get_glyph(texture_font_t* self, const char* codepoint)
{
    texture_glyph_t* glyph;
    assert(self);
    assert(self->filename);
    assert(self->atlas);
    /* Check if codepoint has been already loaded */
    if ((glyph = texture_font_find_glyph(self, codepoint)))
        return glyph;
    /* Glyph has not been already loaded */
    if (texture_font_load_glyph(self, codepoint))
        return texture_font_find_glyph(self, codepoint);
    return 0;
}
