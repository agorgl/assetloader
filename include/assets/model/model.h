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
#ifndef _MODEL_H_
#define _MODEL_H_

#include <stddef.h>
#include <stdint.h>

/* Vertex */
struct vertex {
    float position[3];
    float normal[3];
    float tangent[3];
    float binormal[3];
    float color[4];
    float uvs[2];
};

/* Vertex weight */
struct vertex_weight {
    uint32_t bone_ids[4];
    float bone_weights[4];
};

/* Mesh */
struct mesh {
    int num_verts;
    int num_indices;
    struct vertex* vertices;
    uint32_t* indices;
    int mat_index;
    struct vertex_weight* weights;
};

/* Joint */
struct joint {
    struct joint* parent;
    float position[3];
    float rotation[4]; /* quat */
};

/* Frame */
struct frame {
    struct joint* joints;
    size_t num_joints;
};

/* Frameset */
struct frameset {
    struct frame** frames;
    size_t num_frames;
};

/* Skeleton */
struct skeleton {
    struct frame* rest_pose;
    char** joint_names;
};

/* Model */
struct model {
    int num_meshes;
    struct mesh** meshes;
    int num_materials;
    struct skeleton* skeleton;
    struct frameset* frameset;
};

struct model* model_new();
void model_delete(struct model*);

struct mesh* mesh_new();
void mesh_delete(struct mesh*);

struct skeleton* skeleton_new();
void skeleton_delete(struct skeleton*);

struct frame* frame_new();
struct frame* frame_copy(struct frame* f);
void frame_delete(struct frame*);
void frame_joint_transform(struct joint* j, float trans[16]);
struct frame* frame_interpolate(struct frame* f0, struct frame* f1, float t);

struct frameset* frameset_new();
void frameset_delete(struct frameset* fs);

#endif /* ! _MODEL_H_ */
