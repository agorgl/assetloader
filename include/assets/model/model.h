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

/* Frame */
struct frame {
    /* Joint */
    struct joint {
        struct joint* parent;
        float position[3];
        float rotation[4]; /* quat */
        float scaling[3];
    }* joints;
    size_t num_joints;
};

/* Model */
struct model {
    /* Meshes */
    struct mesh {
        /* Vertices */
        struct vertex {
            float position[3];
            float normal[3];
            float tangent[3];
            float binormal[3];
            float color[4];
            float uvs[2];
        }* vertices;
        struct vertex_weight {
            uint32_t bone_ids[4];
            float bone_weights[4];
        }* weights;
        int num_verts;
        /* Indices */
        uint32_t* indices;
        int num_indices;
        /* Material index */
        int mat_index; /* Relative to the parent mesh group */
    }** meshes;
    int num_meshes;

    /* Skeleton */
    struct skeleton {
        struct frame* rest_pose;
        char** joint_names;
    }* skeleton;
    /* Frameset */
    struct frameset {
        struct frame** frames;
        size_t num_frames;
    }* frameset;

    /* Model nodes */
    struct mesh_group {
        const char* name;
        unsigned int num_materials;
        size_t* mesh_offsets;
        size_t num_mesh_offs;
    }** mesh_groups;
    size_t num_mesh_groups;
};

struct model* model_new();
void model_delete(struct model*);

struct mesh* mesh_new();
void mesh_delete(struct mesh*);

struct mesh_group* mesh_group_new();
void mesh_group_delete(struct mesh_group*);

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
