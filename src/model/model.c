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
    free(m);
}

void mesh_delete(struct mesh* mesh)
{
    free(mesh->vertices);
    free(mesh->indices);
    free(mesh);
}

struct skeleton* skeleton_new()
{
    struct skeleton* skel = malloc(sizeof(struct skeleton));
    memset(skel, 0, sizeof(struct skeleton));
    skel->joints = malloc(0);
    return skel;
}

void skeleton_delete(struct skeleton* skel)
{
    free(skel->joints);
    free(skel);
}

void skeleton_joint_transform(struct joint* j, float trans[16])
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
        skeleton_joint_transform(j->parent, par_trans);
        memcpy(&par_mat, par_trans, sizeof(mat4));
        res = mat4_mul_mat4(par_mat, res);
    }
    memcpy(trans, &res, 16 * sizeof(float));
}
