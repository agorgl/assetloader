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
#ifndef _IQMFILE_H_
#define _IQMFILE_H_

#include <stddef.h>
#include <stdint.h>

#define IQM_MAGIC "INTERQUAKEMODEL"
#define IQM_VERSION 2

/* ofs_* fields are relative to the beginning of the iqmheader struct
   ofs_* fields must be set to 0 when the particular data is empty
   ofs_* fields must be aligned to at least 4 byte boundaries */
struct iqm_header {
    uint8_t magic[16];
    uint32_t version;
    uint32_t filesize;
    uint32_t flags;
    uint32_t num_text, ofs_text;
    uint32_t num_meshes, ofs_meshes;
    uint32_t num_vertexarrays, num_vertexes, ofs_vertexarrays;
    uint32_t num_triangles, ofs_triangles, ofs_adjacency;
    uint32_t num_joints, ofs_joints;
    uint32_t num_poses, ofs_poses;
    uint32_t num_anims, ofs_anims;
    uint32_t num_frames, num_framechannels, ofs_frames, ofs_bounds;
    uint32_t num_comment, ofs_comment;
    uint32_t num_extensions, ofs_extensions;
};

struct iqm_file {
    struct iqm_header header;
    unsigned char* base;
    size_t size;
};

struct iqm_mesh {
    uint32_t name;      /* String */
    uint32_t material;  /* String */
    uint32_t first_vertex, num_vertexes;
    uint32_t first_triangle, num_triangles;
};

/* All vertex array entries must ordered as defined below, if present
   i.e. position comes before normal comes before ... comes before custom
   where a format and size is given, this means models intended for portable use should use these
   an IQM implementation is not required to honor any other format/size than those recommended
   however, it may support other format/size combinations for these types if it desires */
enum {
    IQM_POSITION     = 0,   /* float, 3 */
    IQM_TEXCOORD     = 1,   /* float, 2 */
    IQM_NORMAL       = 2,   /* float, 3 */
    IQM_TANGENT      = 3,   /* float, 4 */
    IQM_BLENDINDEXES = 4,   /* float, 4 */
    IQM_BLENDWEIGHTS = 5,   /* float, 4 */
    IQM_COLOR        = 6,   /* float, 4 */
    /* All values up to IQM_CUSTOM are reserved for future use
       any value >= IQM_CUSTOM is interpreted as CUSTOM type
       the value then defines an offset into the string table, where offset = value - IQM_CUSTOM
       this must be a valid string naming the type */
    IQM_CUSTOM       = 0x10
};

/* Vertex array format */
enum {
    IQM_BYTE   = 0,
    IQM_UBYTE  = 1,
    IQM_SHORT  = 2,
    IQM_USHORT = 3,
    IQM_INT    = 4,
    IQM_UINT   = 5,
    IQM_HALF   = 6,
    IQM_FLOAT  = 7,
    IQM_DOUBLE = 8
};

struct iqm_vertexarray {
    uint32_t type;   /* Type or Custom name */
    uint32_t flags;
    uint32_t format; /* Component format */
    uint32_t size;   /* Number of components */
    /* Offset to array of tightly packed components, with num_vertexes * size total entries
       Offset must be aligned to max(sizeof(format), 4) */
    uint32_t offset;
};

struct iqm_triangle {
    uint32_t vertex[3];
};

struct iqm_adjacency {
    /* Each value is the index of the adjacent triangle for edge 0, 1, and 2,
     * where ~0 (= -1) indicates no adjacent triangle
       indexes are relative to the iqmheader.ofs_triangles array and span all meshes,
       where 0 is the first triangle, 1 is the second, 2 is the third, etc. */
    uint32_t triangle[3];
};

struct iqm_jointv1 {
    uint32_t name;
    int32_t parent; /* Parent < 0 means this is a root bone */
    /* Translate is translation <Tx, Ty, Tz>, and rotate is quaternion rotation <Qx, Qy, Qz, Qw>
       rotation is in relative/parent local space
       scale is pre-scaling <Sx, Sy, Sz>
       output = (input*scale)*rotation + translation */
    float translate[3], rotate[3], scale[3];
};

struct iqm_joint {
    uint32_t name;
    int32_t parent;
    float translate[3], rotate[4], scale[3];
};

struct iqm_posev1 {
    int32_t parent;
    uint32_t mask;
    float channeloffset[9];
    float channelscale[9];
};

struct iqm_pose {
    int32_t parent;
    uint32_t mask;
    /* Channels 0..2 are translation <Tx, Ty, Tz> and channels 3..6 are quaternion rotation <Qx, Qy, Qz, Qw>
       rotation is in relative/parent local space
       channels 7..9 are scale <Sx, Sy, Sz>
       output = (input*scale)*rotation + translation */
    float channeloffset[10];
    float channelscale[10];
};

/* iqm_anim flags */
enum {
    IQM_LOOP = 1<<0
};

struct iqm_anim {
    uint32_t name;
    uint32_t first_frame, num_frames;
    float framerate;
    uint32_t flags;
};

struct iqm_bounds {
    float bbmin[3], bbmax[3];
    float xyradius, radius;
};

struct iqm_extension {
    uint32_t name;
    uint32_t num_data, ofs_data;
    uint32_t ofs_extensions; /* Pointer to next extension */
};

int iqm_read_header(struct iqm_file* iqm);
size_t iqm_va_fmt_size(int va_fmt);

#endif /* ! _IQMFILE_H_ */
