#include "assets/model/model.h"
#include <stdlib.h>
#include <string.h>
#include <linalgb.h>

struct model* model_new()
{
    struct model* m = malloc(sizeof(struct model));
    memset(m, 0, sizeof(struct model));
    m->meshes = malloc(0);
    return m;
}

struct mesh* mesh_new()
{
    struct mesh* mesh = malloc(sizeof(struct mesh));
    memset(mesh, 0, sizeof(struct mesh));
    mesh->vertices = malloc(0);
    mesh->indices = malloc(0);
    return mesh;
}

void model_delete(struct model* m)
{
    for (int i = 0; i < m->num_meshes; ++i)
        mesh_delete(m->meshes[i]);
    free(m->meshes);
    if (m->skeleton)
        skeleton_delete(m->skeleton);
    if (m->frameset)
        frameset_delete(m->frameset);
    free(m);
}

void mesh_delete(struct mesh* mesh)
{
    if (mesh->weights)
        free(mesh->weights);
    free(mesh->vertices);
    free(mesh->indices);
    free(mesh);
}

struct skeleton* skeleton_new()
{
    struct skeleton* skel = malloc(sizeof(struct skeleton));
    memset(skel, 0, sizeof(struct skeleton));
    skel->rest_pose = frame_new();
    skel->joint_names = malloc(0);
    return skel;
}

void skeleton_delete(struct skeleton* skel)
{
    for (uint32_t i = 0; i < skel->rest_pose->num_joints; ++i)
        if (skel->joint_names[i])
            free(skel->joint_names[i]);
    free(skel->joint_names);
    frame_delete(skel->rest_pose);
    free(skel);
}

struct frame* frame_new()
{
    struct frame* f = malloc(sizeof(struct frame));
    memset(f, 0, sizeof(struct frame));
    f->joints = malloc(0);
    return f;
}

struct frame* frame_copy(struct frame* f)
{
    struct frame* nf = frame_new();
    nf->num_joints = f->num_joints;
    nf->joints = realloc(nf->joints, nf->num_joints * sizeof(struct joint));
    for (size_t i = 0; i < nf->num_joints; ++i) {
        struct joint* j = f->joints + i;
        struct joint* nj = nf->joints + i;
        memcpy(nj->position, j->position, 3 * sizeof(float));
        memcpy(nj->rotation, j->rotation, 4 * sizeof(float));
        nj->parent = j->parent ? nf->joints + (j->parent - f->joints) : 0;
    }
    return nf;
}

void frame_delete(struct frame* f)
{
    free(f->joints);
    free(f);
}

void frame_joint_transform(struct joint* j, float trans[16])
{
    mat4 res = mat4_id();
    res = mat4_mul_mat4(
        res, mat4_translation(
            vec3_new(j->position[0], j->position[1], j->position[2]))
    );
    res = mat4_mul_mat4(
        res, mat4_rotation_quat(
            quat_new(j->rotation[0], j->rotation[1], j->rotation[2], j->rotation[3]))
    );

    if (j->parent) {
        float par_trans[16];
        mat4 par_mat = mat4_id();
        frame_joint_transform(j->parent, par_trans);
        memcpy(&par_mat, par_trans, sizeof(mat4));
        res = mat4_mul_mat4(par_mat, res);
    }
    memcpy(trans, &res, 16 * sizeof(float));
}

struct frame* frame_interpolate(struct frame* f0, struct frame* f1, float t)
{
    struct frame* fi = frame_copy(f0);
    for (size_t i = 0; i < fi->num_joints; ++i) {
        /* Position with linear interpolation */
        vec3 f0p = vec3_new(f0->joints[i].position[0], f0->joints[i].position[1], f0->joints[i].position[2]);
        vec3 f1p = vec3_new(f1->joints[i].position[0], f1->joints[i].position[1], f1->joints[i].position[2]);
        vec3 fip = vec3_lerp(f0p, f1p, t);
        memcpy(fi->joints[i].position, &fip, 3 * sizeof(float));
        /* Rotation with spherical linear interpolation */
        vec3 f0r = vec3_new(f0->joints[i].rotation[0], f0->joints[i].rotation[1], f0->joints[i].rotation[2]);
        vec3 f1r = vec3_new(f1->joints[i].rotation[0], f1->joints[i].rotation[1], f1->joints[i].rotation[2]);
        vec3 fir = vec3_lerp(f0r, f1r, t);
        memcpy(fi->joints[i].rotation, &fir, 3 * sizeof(float));
    }
    return fi;
}

struct frameset* frameset_new()
{
    struct frameset* fs = malloc(sizeof(struct frameset));
    return fs;
}

void frameset_delete(struct frameset* fs)
{
    for (size_t i = 0; i < fs->num_frames; ++i)
        frame_delete(fs->frames[i]);
    free(fs->frames);
    free(fs);
}
