/*********************************************************************************************************************/
/*                                                  /===-_---~~~~~~~~~------____                                     */
/*                                                 |===-~___                _,-'                                     */
/*                  -==\\                         `//~\\   ~~~~`---.___.-~~                                          */
/*              ______-==|                         | |  \\           _-~`                                            */
/*        __--~~~  ,-/-==\\                        | |   `\        ,'                                                */
/*     _-~       /'    |  \\                      / /      \      /                                                  */
/*   .'        /       |   \\                   /' /        \   /'                                                   */
/*  /  ____  /         |    \`\.__/-~~ ~ \ _ _/'  /          \/'                                                     */
/* /-'~    ~~~~~---__  |     ~-/~         ( )   /'        _--~`                                                      */
/*                   \_|      /        _)   ;  ),   __--~~                                                           */
/*                     '~~--_/      _-~/-  / \   '-~ \                                                               */
/*                    {\__--_/}    / \\_>- )<__\      \                                                              */
/*                    /'   (_/  _-~  | |__>--<__|      |                                                             */
/*                   |0  0 _/) )-~     | |__>--<__|     |                                                            */
/*                   / /~ ,_/       / /__>---<__/      |                                                             */
/*                  o o _//        /-~_>---<__-~      /                                                              */
/*                  (^(~          /~_>---<__-      _-~                                                               */
/*                 ,/|           /__>--<__/     _-~                                                                  */
/*              ,//('(          |__>--<__|     /                  .----_                                             */
/*             ( ( '))          |__>--<__|    |                 /' _---_~\                                           */
/*          `-)) )) (           |__>--<__|    |               /'  /     ~\`\                                         */
/*         ,/,'//( (             \__>--<__\    \            /'  //        ||                                         */
/*       ,( ( ((, ))              ~-__>--<_~-_  ~--____---~' _/'/        /'                                          */
/*     `~/  )` ) ,/|                 ~-_~>--<_/-__       __-~ _/                                                     */
/*   ._-~//( )/ )) `                    ~~-'_/_/ /~~~~~~~__--~                                                       */
/*    ;'( ')/ ,)(                              ~~~~~~~~~~                                                            */
/*   ' ') '( (/                                                                                                      */
/*     '   '  `                                                                                                      */
/*********************************************************************************************************************/
#ifndef _TEXTURE_FONT_H_
#define _TEXTURE_FONT_H_

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "assets/texture_atlas.h"
#include "vector.h"

/**
 * A list of possible ways to render a glyph.
 */
typedef enum rendermode_t {
    RENDER_NORMAL,
    RENDER_OUTLINE_EDGE,
    RENDER_OUTLINE_POSITIVE,
    RENDER_OUTLINE_NEGATIVE,
    RENDER_SIGNED_DISTANCE_FIELD
} rendermode_t;

/**
 * A structure that hold a kerning value relatively to a Unicode
 * codepoint.
 *
 * This structure cannot be used alone since the (necessary) right
 * Unicode codepoint is implicitely held by the owner of this structure.
 */
typedef struct kerning_t {
    /** Left Unicode codepoint in the kern pair in UTF-32 LE encoding. */
    uint32_t codepoint;
    /** Kerning value (in fractional pixels). */
    float kerning;
} kerning_t;

/*
 * Glyph metrics:
 * --------------
 *
 *                       xmin                     xmax
 *                        |                         |
 *                        |<-------- width -------->|
 *                        |                         |
 *              |         +-------------------------+----------------- ymax
 *              |         |    ggggggggg   ggggg    |     ^        ^
 *              |         |   g:::::::::ggg::::g    |     |        |
 *              |         |  g:::::::::::::::::g    |     |        |
 *              |         | g::::::ggggg::::::gg    |     |        |
 *              |         | g:::::g     g:::::g     |     |        |
 *    offset_x -|-------->| g:::::g     g:::::g     |  offset_y    |
 *              |         | g:::::g     g:::::g     |     |        |
 *              |         | g::::::g    g:::::g     |     |        |
 *              |         | g:::::::ggggg:::::g     |     |        |
 *              |         |  g::::::::::::::::g     |     |      height
 *              |         |   gg::::::::::::::g     |     |        |
 *  baseline ---*---------|---- gggggggg::::::g-----*--------      |
 *            / |         |             g:::::g     |              |
 *     origin   |         | gggggg      g:::::g     |              |
 *              |         | g:::::gg   gg:::::g     |              |
 *              |         |  g::::::ggg:::::::g     |              |
 *              |         |   gg:::::::::::::g      |              |
 *              |         |     ggg::::::ggg        |              |
 *              |         |         gggggg          |              v
 *              |         +-------------------------+----------------- ymin
 *              |                                   |
 *              |------------- advance_x ---------->|
 */

/** A structure that describe a glyph. */
typedef struct texture_glyph_t {
    /** Unicode codepoint this glyph represents in UTF-32 LE encoding. */
    uint32_t codepoint;
    /** Glyph's width in pixels. */
    size_t width;
    /** Glyph's height in pixels. */
    size_t height;
    /** Glyph's left bearing expressed in integer pixels. */
    int offset_x;
    /**
     * Glyphs's top bearing expressed in integer pixels.
     *
     * Remember that this is the distance from the baseline to the top-most
     * glyph scanline, upwards y coordinates being positive.
     */
    int offset_y;
    /**
     * For horizontal text layouts, this is the horizontal distance (in
     * fractional pixels) used to increment the pen position when the glyph is
     * drawn as part of a string of text.
     */
    float advance_x;
    /**
     * For vertical text layouts, this is the vertical distance (in fractional
     * pixels) used to increment the pen position when the glyph is drawn as
     * part of a string of text.
     */
    float advance_y;
    /** First normalized texture coordinate (x) of top-left corner */
    float s0;
    /** Second normalized texture coordinate (y) of top-left corner */
    float t0;
    /** First normalized texture coordinate (x) of bottom-right corner */
    float s1;
    /** Second normalized texture coordinate (y) of bottom-right corner */
    float t1;
    /** A vector of kerning pairs relative to this glyph. */
    struct vector kerning;
    /** Mode this glyph was rendered */
    rendermode_t rendermode;
    /** Glyph outline thickness */
    float outline_thickness;
} texture_glyph_t;

/** Texture font structure. */
typedef struct texture_font_t {
    /** Vector of glyphs contained in this font. */
    struct vector glyphs;
    /** Atlas structure to store glyphs data. */
    texture_atlas_t* atlas;
    /** Font type */
    enum {
        TEXTURE_FONT_TYPE_TRUETYPE = 0,
        TEXTURE_FONT_TYPE_BITMAP
    } type;
    /** Font location */
    enum {
        TEXTURE_FONT_FILE = 0,
        TEXTURE_FONT_MEMORY,
    } location;

    union {
        /** Font filename, for when location == TEXTURE_FONT_FILE */
        char* filename;
        /** Font memory address, for when location == TEXTURE_FONT_MEMORY */
        struct {
            const void* base;
            size_t size;
        } memory;
    };
    /** Font size */
    float size;
    /** Whether to use autohint when rendering font */
    int hinting;
    /** Mode the font is rendering its next glyph */
    rendermode_t rendermode;
    /** Outline thickness */
    float outline_thickness;
    /** Whether to use our own lcd filter. */
    int filtering;
    /** LCD filter weights */
    unsigned char lcd_weights[5];
    /** Whether to use kerning if available */
    int kerning;
    /**
     * This field is simply used to compute a default line spacing (i.e., the
     * baseline-to-baseline distance) when writing text with this font. Note
     * that it usually is larger than the sum of the ascender and descender
     * taken as absolute values. There is also no guarantee that no glyphs
     * extend above or below subsequent baselines when using this distance. */
    float height;
    /**
     * This field is the distance that must be placed between two lines of
     * text. The baseline-to-baseline distance should be computed as:
     * ascender - descender + linegap
     */
    float linegap;
    /**
     * The ascender is the vertical distance from the horizontal baseline to
     * the highest 'character' coordinate in a font face. Unfortunately, font
     * formats define the ascender differently. For some, it represents the
     * ascent of all capital latin characters (without accents), for others it
     * is the ascent of the highest accented character, and finally, other
     * formats define it as being equal to bbox.yMax.
     */
    float ascender;
    /**
     * The descender is the vertical distance from the horizontal baseline to
     * the lowest 'character' coordinate in a font face. Unfortunately, font
     * formats define the descender differently. For some, it represents the
     * descent of all capital latin characters (without accents), for others it
     * is the ascent of the lowest accented character, and finally, other
     * formats define it as being equal to bbox.yMin. This field is negative
     * for values below the baseline.
     */
    float descender;
    /**
     * The position of the underline line for this face. It is the center of
     * the underlining stem. Only relevant for scalable formats.
     */
    float underline_position;
    /**
     * The thickness of the underline for this face. Only relevant for scalable
     * formats.
     */
    float underline_thickness;
} texture_font_t;

/**
 * This function creates a new texture font from given filename and size.  The
 * texture atlas is used to store glyph on demand. Note the depth of the atlas
 * will determine if the font is rendered as alpha channel only (depth = 1) or
 * RGB (depth = 3) that correspond to subpixel rendering (if available on your
 * freetype implementation). Returns A new empty font (no glyph inside yet)
 */
texture_font_t* texture_font_new_from_file(
    texture_atlas_t* atlas,
    const float pt_size,
    const char* filename);

/** Same as above but load from memory */
texture_font_t* texture_font_new_from_memory(texture_atlas_t* atlas,
                                             float pt_size,
                                             const void* memory_base,
                                             size_t memory_size,
                                             int font_type);

/**
 * Delete a texture font. Note that this does not delete the glyph from the
 * texture atlas.
 */
void texture_font_delete(texture_font_t* self);

/**
 * Request a new glyph from the font. If it has not been created yet, it will
 * be. Codepoint character to be loaded should be in UTF-8 encoding.
 * Returns 0 if the texture atlas has no space left to host the glyph
 */
texture_glyph_t* texture_font_get_glyph(texture_font_t* self,
                                        const char* codepoint);

/**
 * Search for a loaded glyph in the font.
 * Codepoint character to searched should be in UTF-8 encoding.
 * Returns 0 if the glyph is not loaded in the atlas
 */
texture_glyph_t* texture_font_find_glyph(texture_font_t* self,
                                         const char* codepoint);

/**
 * Request the loading of a given glyph.
 * Returns 1 if the glyph could be loaded, 0 if not.
 */
int texture_font_load_glyph(texture_font_t* self,
                            const char* codepoint);

/**
 * Request the loading of several glyphs at once.
 * Returns the number of missed glyph if the texture is not big enough to hold
 * all glyphs.
 */
size_t texture_font_load_glyphs(texture_font_t* self,
                                const char* codepoints);

/** Get the kerning between two horizontal glyphs. */
float texture_glyph_get_kerning(
    const texture_glyph_t* self,
    const char* codepoint);

/** Creates a new empty glyph */
texture_glyph_t* texture_glyph_new(void);

#ifdef __cplusplus
}
#endif

#endif /* ! _TEXTURE_FONT_H_ */
