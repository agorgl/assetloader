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
#ifndef _TEXTURE_ATLAS_H_
#define _TEXTURE_ATLAS_H_

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "vector.h"

/**
 * Texture atlas
 *
 * A texture atlas is used to pack several small regions into a single texture.
 *
 * The actual implementation is based on the article by Jukka Jylänki : "A
 * Thousand Ways to Pack the Bin - A Practical Approach to Two-Dimensional
 * Rectangle Bin Packing", February 27, 2010.
 * More precisely, this is an implementation of the Skyline Bottom-Left
 * algorithm based on C++ sources provided by Jukka Jylänki at:
 * http://clb.demon.fi/files/RectangleBinPack/
 *
 * Example Usage:
 * #include "texture-atlas.h"
 * ...
 * // Creates a new atlas of 512x512 with a depth of 1
 * texture_atlas_t* atlas = texture_atlas_new(512, 512, 1);
 *
 * // Allocates a region of 20x20
 * ivec4 region = texture_atlas_get_region(atlas, 20, 20);
 *
 * // Fill region with some data
 * texture_atlas_set_region(atlas, region.x, region.y, region.width, region.height, data, stride)
 * ...
 */

typedef union {
    int data[4];
    struct {
        int x;
        int y;
        int z;
        int w;
    };
    struct {
        int x_;
        int y_;
        int width;
        int height;
    };
    struct {
        int r;
        int g;
        int b;
        int a;
    };
} ivec4;

typedef union {
    int data[3];
    struct {
        int x;
        int y;
        int z;
    };
    struct {
        int r;
        int g;
        int b;
    };
} ivec3;

/**
 * A texture atlas is used to pack several small regions into a single texture.
 */
typedef struct texture_atlas_t {
    /** Allocated nodes */
    struct vector nodes;
    /** Width (in pixels) of the underlying texture */
    size_t width;
    /** Height (in pixels) of the underlying texture */
    size_t height;
    /** Depth (in bytes) of the underlying texture */
    size_t depth;
    /** Allocated surface size */
    size_t used;
    /** Texture identity (OpenGL) */
    unsigned int id;
    /** Atlas data */
    unsigned char* data;
} texture_atlas_t;

/**
 * Creates a new empty texture atlas,
 * with the given width, height and bit depth.
 */
texture_atlas_t* texture_atlas_new(const size_t width,
                                   const size_t height,
                                   const size_t depth);

/** Deletes a texture atlas. */
void texture_atlas_delete(texture_atlas_t* self);

/**
 * Allocate a new region in the atlas,
 * of the given width and height.
 * Returns coordinates of the allocated region.
 */
ivec4 texture_atlas_get_region(texture_atlas_t* self,
                               const size_t width,
                               const size_t height);

/**
 *  Upload data to the specified atlas region.
 *  With:
 *    self   : a texture atlas structure
 *    x      : x coordinate the region
 *    y      : y coordinate the region
 *    width  : width of the region
 *    height : height of the region
 *    data   : data to be uploaded into the specified region
 *    stride : stride of the data
 */
void texture_atlas_set_region(texture_atlas_t* self,
                              const size_t x,
                              const size_t y,
                              const size_t width,
                              const size_t height,
                              const unsigned char* data,
                              const size_t stride);

/** Remove all allocated regions from the atlas. */
void texture_atlas_clear(texture_atlas_t* self);

#ifdef __cplusplus
}
#endif

#endif /* ! _TEXTURE_ATLAS_H_ */
