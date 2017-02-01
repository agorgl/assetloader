#include "ttf_font.h"
#include <math.h>
#include <assert.h>
#include "utf8_utils.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H
#include FT_LCD_FILTER_H

/*-----------------------------------------------------------------
 * Texture font
 *-----------------------------------------------------------------*/
#define HRES 64
#define HRESf 64.f
#define DPI 72

#undef __FTERRORS_H__
#define FT_ERRORDEF( e, v, s )  { e, s },
#define FT_ERROR_START_LIST     {
#define FT_ERROR_END_LIST       { 0, 0 } };
const struct {
    int          code;
    const char*  message;
} FT_Errors[] =
#include FT_ERRORS_H

static int ttf_font_load_face(texture_font_t* self, float size, FT_Library* library, FT_Face* face)
{
    FT_Error error;
    FT_Matrix matrix = {
        (int)((1.0 / HRES) * 0x10000L),
        (int)((0.0) * 0x10000L),
        (int)((0.0) * 0x10000L),
        (int)((1.0) * 0x10000L)};

    assert(library);
    assert(size);

    /* Initialize library */
    error = FT_Init_FreeType(library);
    if (error) {
        fprintf(stderr, "FT_Error (0x%02x) : %s\n",
                FT_Errors[error].code, FT_Errors[error].message);
        goto cleanup;
    }

    /* Load face */
    switch (self->location) {
        case TEXTURE_FONT_FILE:
            error = FT_New_Face(*library, self->filename, 0, face);
            break;
        case TEXTURE_FONT_MEMORY:
            error = FT_New_Memory_Face(*library, self->memory.base, self->memory.size, 0, face);
            break;
    }

    if (error) {
        fprintf(stderr, "FT_Error (line %d, code 0x%02x) : %s\n",
                __LINE__, FT_Errors[error].code, FT_Errors[error].message);
        goto cleanup_library;
    }

    /* Select charmap */
    error = FT_Select_Charmap(*face, FT_ENCODING_UNICODE);
    if (error) {
        fprintf(stderr, "FT_Error (line %d, code 0x%02x) : %s\n",
                __LINE__, FT_Errors[error].code, FT_Errors[error].message);
        goto cleanup_face;
    }

    /* Set char size */
    error = FT_Set_Char_Size(*face, (int)(size * HRES), 0, DPI * HRES, DPI);

    if (error) {
        fprintf(stderr, "FT_Error (line %d, code 0x%02x) : %s\n",
                __LINE__, FT_Errors[error].code, FT_Errors[error].message);
        goto cleanup_face;
    }

    /* Set transform matrix */
    FT_Set_Transform(*face, &matrix, NULL);

    return 1;

cleanup_face:
    FT_Done_Face(*face);
cleanup_library:
    FT_Done_FreeType(*library);
cleanup:
    return 0;
}

static void ttf_font_generate_kerning(texture_font_t* self, FT_Library* library, FT_Face* face)
{
    (void) library;
    size_t i, j;
    FT_UInt glyph_index, prev_index;
    texture_glyph_t *glyph, *prev_glyph;
    FT_Vector kerning;

    assert(self);

    /* For each glyph couple combination, check if kerning is necessary */
    /* Starts at index 1 since 0 is for the special backgroudn glyph */
    for (i = 1; i < self->glyphs.size; ++i) {
        glyph = *(texture_glyph_t**)vector_at(&self->glyphs, i);
        glyph_index = FT_Get_Char_Index(*face, glyph->codepoint);
        vector_clear(&glyph->kerning);

        for (j = 1; j < self->glyphs.size; ++j) {
            prev_glyph = *(texture_glyph_t**)vector_at(&self->glyphs, j);
            prev_index = FT_Get_Char_Index(*face, prev_glyph->codepoint);
            FT_Get_Kerning(*face, prev_index, glyph_index, FT_KERNING_UNFITTED, &kerning);
            // printf("%c(%d)-%c(%d): %ld\n",
            //       prev_glyph->codepoint, prev_glyph->codepoint,
            //       glyph_index, glyph_index, kerning.x);
            if (kerning.x) {
                kerning_t k = {prev_glyph->codepoint, kerning.x / (float)(HRESf * HRESf)};
                vector_append(&glyph->kerning, &k);
            }
        }
    }
}

int ttf_font_init(texture_font_t* self)
{
    FT_Library library;
    FT_Face face;
    FT_Size_Metrics metrics;

    assert(self->atlas);
    assert(self->size > 0);
    assert((self->location == TEXTURE_FONT_FILE && self->filename)
        || (self->location == TEXTURE_FONT_MEMORY && self->memory.base && self->memory.size));

    vector_init(&self->glyphs, sizeof(texture_glyph_t*));
    self->height = 0;
    self->ascender = 0;
    self->descender = 0;
    self->rendermode = RENDER_NORMAL;
    self->outline_thickness = 0.0;
    self->hinting = 1;
    self->kerning = 1;
    self->filtering = 1;

    /* FT_LCD_FILTER_LIGHT   is (0x00, 0x55, 0x56, 0x55, 0x00) */
    /* FT_LCD_FILTER_DEFAULT is (0x10, 0x40, 0x70, 0x40, 0x10) */
    self->lcd_weights[0] = 0x10;
    self->lcd_weights[1] = 0x40;
    self->lcd_weights[2] = 0x70;
    self->lcd_weights[3] = 0x40;
    self->lcd_weights[4] = 0x10;

    if (!ttf_font_load_face(self, self->size * 100.f, &library, &face))
        return -1;

    self->underline_position = face->underline_position / (float)(HRESf * HRESf) * self->size;
    self->underline_position = round(self->underline_position);
    if (self->underline_position > -2)
        self->underline_position = -2.0;

    self->underline_thickness = face->underline_thickness / (float)(HRESf * HRESf) * self->size;
    self->underline_thickness = round(self->underline_thickness);
    if (self->underline_thickness < 1)
        self->underline_thickness = 1.0;

    metrics = face->size->metrics;
    self->ascender = (metrics.ascender >> 6) / 100.0;
    self->descender = (metrics.descender >> 6) / 100.0;
    self->height = (metrics.height >> 6) / 100.0;
    self->linegap = self->height - self->ascender + self->descender;
    FT_Done_Face(face);
    FT_Done_FreeType(library);

    /* NULL is a special glyph */
    texture_font_get_glyph(self, NULL);

    return 0;
}

int ttf_font_load_glyph(texture_font_t* self, const char* codepoint)
{
    size_t i, x, y;

    FT_Library library;
    FT_Error error;
    FT_Face face;
    FT_Glyph ft_glyph;
    FT_GlyphSlot slot;
    FT_Bitmap ft_bitmap;

    FT_UInt glyph_index;
    texture_glyph_t* glyph;
    FT_Int32 flags = 0;
    int ft_glyph_top = 0;
    int ft_glyph_left = 0;

    ivec4 region;
    size_t missed = 0;
    (void) missed;

    if (!ttf_font_load_face(self, self->size, &library, &face))
        return 0;

    /* Check if codepoint has been already loaded */
    if (texture_font_find_glyph(self, codepoint)) {
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return 1;
    }

    /*
     * Codepoint NULL is special : it is used for line drawing (overline,
     * underline, strikethrough) and background.
     */
    if (!codepoint) {
        ivec4 region = texture_atlas_get_region(self->atlas, 5, 5);
        texture_glyph_t* glyph = texture_glyph_new();
        static unsigned char data[4 * 4 * 3] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
        if (region.x < 0) {
            fprintf(stderr, "Texture atlas is full (line %d)\n", __LINE__);
            return 0;
        }
        texture_atlas_set_region(self->atlas, region.x, region.y, 4, 4, data, 0);
        glyph->codepoint = -1;
        glyph->s0 = (region.x + 2) / (float)self->atlas->width;
        glyph->t0 = (region.y + 2) / (float)self->atlas->height;
        glyph->s1 = (region.x + 3) / (float)self->atlas->width;
        glyph->t1 = (region.y + 3) / (float)self->atlas->height;
        vector_append(&self->glyphs, &glyph);

        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return 1;
    }

    flags = 0;
    ft_glyph_top = 0;
    ft_glyph_left = 0;
    glyph_index = FT_Get_Char_Index(face, (FT_ULong)utf8_to_utf32(codepoint));
    /*
     * WARNING: We use texture-atlas depth to guess if user wants
     *          LCD subpixel rendering
     */

    if (self->rendermode != RENDER_NORMAL && self->rendermode != RENDER_SIGNED_DISTANCE_FIELD) {
        flags |= FT_LOAD_NO_BITMAP;
    } else {
        flags |= FT_LOAD_RENDER;
    }

    if (!self->hinting) {
        flags |= FT_LOAD_NO_HINTING | FT_LOAD_NO_AUTOHINT;
    } else {
        flags |= FT_LOAD_FORCE_AUTOHINT;
    }

    if (self->atlas->depth == 3) {
        FT_Library_SetLcdFilter(library, FT_LCD_FILTER_LIGHT);
        flags |= FT_LOAD_TARGET_LCD;

        if (self->filtering) {
            FT_Library_SetLcdFilterWeights(library, self->lcd_weights);
        }
    }

    error = FT_Load_Glyph(face, glyph_index, flags);
    if (error) {
        fprintf(stderr, "FT_Error (line %d, code 0x%02x) : %s\n",
                __LINE__, FT_Errors[error].code, FT_Errors[error].message);
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return 0;
    }

    if (self->rendermode == RENDER_NORMAL || self->rendermode == RENDER_SIGNED_DISTANCE_FIELD) {
        slot = face->glyph;
        ft_bitmap = slot->bitmap;
        ft_glyph_top = slot->bitmap_top;
        ft_glyph_left = slot->bitmap_left;
    } else {
        FT_Stroker stroker;
        FT_BitmapGlyph ft_bitmap_glyph;

        error = FT_Stroker_New(library, &stroker);

        if (error) {
            fprintf(stderr, "FT_Error (0x%02x) : %s\n",
                    FT_Errors[error].code, FT_Errors[error].message);
            goto cleanup_stroker;
        }

        FT_Stroker_Set(stroker,
                       (int)(self->outline_thickness * HRES),
                       FT_STROKER_LINECAP_ROUND,
                       FT_STROKER_LINEJOIN_ROUND,
                       0);

        error = FT_Get_Glyph(face->glyph, &ft_glyph);

        if (error) {
            fprintf(stderr, "FT_Error (0x%02x) : %s\n",
                    FT_Errors[error].code, FT_Errors[error].message);
            goto cleanup_stroker;
        }

        if (self->rendermode == RENDER_OUTLINE_EDGE)
            error = FT_Glyph_Stroke(&ft_glyph, stroker, 1);
        else if (self->rendermode == RENDER_OUTLINE_POSITIVE)
            error = FT_Glyph_StrokeBorder(&ft_glyph, stroker, 0, 1);
        else if (self->rendermode == RENDER_OUTLINE_NEGATIVE)
            error = FT_Glyph_StrokeBorder(&ft_glyph, stroker, 1, 1);

        if (error) {
            fprintf(stderr, "FT_Error (0x%02x) : %s\n",
                    FT_Errors[error].code, FT_Errors[error].message);
            goto cleanup_stroker;
        }

        if (self->atlas->depth == 1)
            error = FT_Glyph_To_Bitmap(&ft_glyph, FT_RENDER_MODE_NORMAL, 0, 1);
        else
            error = FT_Glyph_To_Bitmap(&ft_glyph, FT_RENDER_MODE_LCD, 0, 1);

        if (error) {
            fprintf(stderr, "FT_Error (0x%02x) : %s\n",
                    FT_Errors[error].code, FT_Errors[error].message);
            goto cleanup_stroker;
        }

        ft_bitmap_glyph = (FT_BitmapGlyph)ft_glyph;
        ft_bitmap = ft_bitmap_glyph->bitmap;
        ft_glyph_top = ft_bitmap_glyph->top;
        ft_glyph_left = ft_bitmap_glyph->left;

    cleanup_stroker:
        FT_Stroker_Done(stroker);

        if (error) {
            FT_Done_Face(face);
            FT_Done_FreeType(library);
            return 0;
        }
    }

    struct {
        int left;
        int top;
        int right;
        int bottom;
    } padding = {0, 0, 1, 1};

    if (self->rendermode == RENDER_SIGNED_DISTANCE_FIELD) {
        padding.top = 1;
        padding.left = 1;
    }

    size_t src_w = ft_bitmap.width / self->atlas->depth;
    size_t src_h = ft_bitmap.rows;

    size_t tgt_w = src_w + padding.left + padding.right;
    size_t tgt_h = src_h + padding.top + padding.bottom;

    region = texture_atlas_get_region(self->atlas, tgt_w, tgt_h);

    if (region.x < 0) {
        fprintf(stderr, "Texture atlas is full (line %d)\n", __LINE__);
        return 0;
    }

    x = region.x;
    y = region.y;

    unsigned char* buffer = calloc(tgt_w * tgt_h, sizeof(unsigned char));
    for (i = 0; i < src_h; i++) {
        memcpy(buffer + (i + padding.top) * tgt_w + padding.left, ft_bitmap.buffer + i * ft_bitmap.pitch, src_w);
    }

#ifdef HAS_DISTANCE_FIELD
    if (self->rendermode == RENDER_SIGNED_DISTANCE_FIELD) {
        unsigned char* sdf = make_distance_mapb(buffer, tgt_w, tgt_h);
        free(buffer);
        buffer = sdf;
    }
#endif

    texture_atlas_set_region(self->atlas, x, y, tgt_w, tgt_h, buffer, tgt_w);

    free(buffer);

    glyph = texture_glyph_new();
    glyph->codepoint = utf8_to_utf32(codepoint);
    glyph->width = tgt_w;
    glyph->height = tgt_h;
    glyph->rendermode = self->rendermode;
    glyph->outline_thickness = self->outline_thickness;
    glyph->offset_x = ft_glyph_left;
    glyph->offset_y = ft_glyph_top;
    glyph->s0 = x / (float)self->atlas->width;
    glyph->t0 = y / (float)self->atlas->height;
    glyph->s1 = (x + glyph->width) / (float)self->atlas->width;
    glyph->t1 = (y + glyph->height) / (float)self->atlas->height;

    /* Discard hinting to get advance */
    FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER | FT_LOAD_NO_HINTING);
    slot = face->glyph;
    glyph->advance_x = slot->advance.x / HRESf;
    glyph->advance_y = slot->advance.y / HRESf;

    vector_append(&self->glyphs, &glyph);

    if (self->rendermode != RENDER_NORMAL && self->rendermode != RENDER_SIGNED_DISTANCE_FIELD)
        FT_Done_Glyph(ft_glyph);

    ttf_font_generate_kerning(self, &library, &face);

    FT_Done_Face(face);
    FT_Done_FreeType(library);

    return 1;
}
