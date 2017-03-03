#include <assets/model/modelload.h>
#include "iqmfile.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <hashmap.h>
#include <linalgb.h>
#include <assert.h>

static struct frameset* iqm_read_frames(struct iqm_file* iqm)
{
    struct iqm_header* h = &iqm->header;
    unsigned char* base = iqm->base;
    unsigned short* framedata = (unsigned short*)(base + h->ofs_frames);
    struct frameset* frameset = frameset_new();

    struct frame** frames = malloc(h->num_frames * sizeof(struct frame*));
    memset(frames, 0, h->num_frames * sizeof(struct frame*));

    for (uint32_t i = 0; i < h->num_frames; ++i) {
        /* Setup empty frame */
        struct frame* f = frame_new();
        f->num_joints = h->num_poses;
        f->joints = realloc(f->joints, f->num_joints * sizeof(struct joint));
        memset(f->joints, 0, f->num_joints * sizeof(struct joint));

        for (uint32_t j = 0; j < h->num_poses; ++j) {
            struct iqm_pose* pose = (struct iqm_pose*)(base + h->ofs_poses) + j;

            float fc[10] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1};
            for (int k = 0; k < 10; ++k) {
                fc[k] = pose->channeloffset[k];
                if (pose->mask & (1 << k)) {
                    fc[k] += *framedata * pose->channelscale[k];
                    ++framedata;
                }
            }

            /* Copy joint data */
            struct joint* jnt = f->joints + j;
            memcpy(jnt->position, fc + 0, 3 * sizeof(float));
            memcpy(jnt->rotation, fc + 3, 4 * sizeof(float));
            memcpy(jnt->scaling, fc + 7, 3 * sizeof(float));
            jnt->parent = pose->parent < 0 ? 0 : f->joints + pose->parent;
        }

        /* Append new frame to the list */
        frames[i] = f;
    }

    frameset->num_frames = h->num_frames;
    frameset->frames = frames;
    return frameset;
}

static struct skeleton* iqm_read_skeleton(struct iqm_file* iqm)
{
    struct iqm_header* h = &iqm->header;
    unsigned char* base = iqm->base;
    struct skeleton* skel = skeleton_new();

    /* Joints */
    skel->rest_pose->num_joints = h->num_joints;
    skel->rest_pose->joints = realloc(skel->rest_pose->joints, skel->rest_pose->num_joints * sizeof(struct joint));
    memset(skel->rest_pose->joints, 0, skel->rest_pose->num_joints * sizeof(struct joint));
    /* Joint names */
    skel->joint_names = realloc(skel->joint_names, skel->rest_pose->num_joints * sizeof(char*));
    memset(skel->joint_names, 0, skel->rest_pose->num_joints * sizeof(char*));

    for (uint32_t i = 0; i < h->num_joints; ++i) {
        struct iqm_joint* joint = (struct iqm_joint*)(base + h->ofs_joints + i * sizeof(struct iqm_joint));

        /* Set joint parent */
        struct joint* j = skel->rest_pose->joints + i;
        j->parent = joint->parent == -1 ? 0 : skel->rest_pose->joints + joint->parent;

        /* Copy joint name */
        const char* name = (const char*)(base + h->ofs_text + joint->name);
        size_t name_sz = strlen(name) * sizeof(char);
        skel->joint_names[i] = malloc(name_sz + 1);
        memcpy(skel->joint_names[i], name, name_sz);
        *(skel->joint_names[i] + name_sz) = 0;

        /* Copy joint data */
        memcpy(j->position, joint->translate, 3 * sizeof(float));
        memcpy(j->rotation, joint->rotate, 4 * sizeof(float));
        memcpy(j->scaling, joint->scale, 3 * sizeof(float));
    }

    return skel;
}

static struct mesh* iqm_read_mesh(struct iqm_file* iqm, uint32_t mesh_idx, uint32_t prev_verts_num)
{
    /* Aliases */
    struct iqm_header* h = &iqm->header;
    unsigned char* base = iqm->base;

    struct iqm_mesh* mesh = (struct iqm_mesh*)(base + h->ofs_meshes + mesh_idx * sizeof(struct iqm_mesh));
    struct mesh* m = mesh_new();

    /* Allocate vertices */
    m->num_verts = mesh->num_vertexes;
    m->vertices = realloc(m->vertices, m->num_verts * sizeof(struct vertex));
    memset(m->vertices, 0, m->num_verts * sizeof(struct vertex));

    /* Allocate indices */
    m->num_indices = mesh->num_triangles * 3;
    m->indices = realloc(m->indices, m->num_indices * sizeof(uint32_t));
    memset(m->indices, 0, m->num_indices * sizeof(uint32_t));

    /* Check is mesh has bone weights and allocate space if needed */
    for (uint32_t j = 0; j < h->num_vertexarrays; ++j) {
        struct iqm_vertexarray* va = (struct iqm_vertexarray*)(base + h->ofs_vertexarrays) + j;
        if (va->type == IQM_BLENDINDEXES || va->type == IQM_BLENDWEIGHTS) {
            m->weights = malloc(m->num_verts * sizeof(struct vertex_weight));
            break;
        }
    }

    /* Populate vertices */
    for (int i = 0; i < m->num_verts; ++i) {
        struct vertex* cur_vert = m->vertices + i;
        struct vertex_weight* cur_weight = m->weights + i;
        /* Iterate vertex arrays filling current vertex with data */
        for (uint32_t j = 0; j < h->num_vertexarrays; ++j) {
            struct iqm_vertexarray* va = (struct iqm_vertexarray*)(
                base + h->ofs_vertexarrays
              + j * sizeof(struct iqm_vertexarray)
            );
            void* data_loc =
                base + va->offset
              + (mesh->first_vertex + i) * iqm_va_fmt_size(va->format) * va->size;

            switch (va->type) {
                case IQM_POSITION:
                    memcpy(cur_vert->position, data_loc, 3 * sizeof(float));
                    break;
                case IQM_TEXCOORD:
                    memcpy(cur_vert->uvs, data_loc, 2 * sizeof(float));
                    break;
                case IQM_NORMAL:
                    memcpy(cur_vert->normal, data_loc, 3 * sizeof(float));
                    break;
                case IQM_TANGENT:
                    memcpy(cur_vert->tangent, data_loc, 3 * sizeof(float));
                    break;
                case IQM_BLENDINDEXES: {
                    assert(iqm_va_fmt_size(va->format) == sizeof(unsigned char));
                    uint32_t bis[4];
                    for (int i = 0; i < 4; ++i)
                        bis[i] = ((unsigned char*) data_loc)[i];
                    memcpy(cur_weight->bone_ids, bis, 4 * sizeof(uint32_t));
                    break;
                }
                case IQM_BLENDWEIGHTS: {
                    assert(iqm_va_fmt_size(va->format) == sizeof(unsigned char));
                    float biw[4];
                    for (int i = 0; i < 4; ++i)
                        biw[i] = ((unsigned char*) data_loc)[i] / 255.0f;
                    memcpy(cur_weight->bone_weights, biw, 4 * sizeof(float));
                    break;
                }
                default:
                    break;
            }
        }
    }

    /* Populate indices */
    for (uint32_t i = 0; i < mesh->num_triangles; ++i) {
        struct iqm_triangle* tri = (struct iqm_triangle*)(
            base + h->ofs_triangles
          + (mesh->first_triangle + i) * sizeof(struct iqm_triangle)
        );
        m->indices[i * 3 + 0] = tri->vertex[0] - prev_verts_num;
        m->indices[i * 3 + 1] = tri->vertex[1] - prev_verts_num;
        m->indices[i * 3 + 2] = tri->vertex[2] - prev_verts_num;
    }

    /* Assign temporary material index */
    m->mat_index = mesh->material;

    return m;
}

static size_t int_hash(hm_ptr key) { return (size_t)key; }
static int int_eql(hm_ptr k1, hm_ptr k2) { return k1 == k2; }

static struct model* iqm_read_model(struct iqm_file* iqm)
{
    struct hashmap material_ids;
    hashmap_init(&material_ids, int_hash, int_eql);

    struct model* model = model_new();
    model->num_mesh_groups = 1;
    model->mesh_groups = realloc(model->mesh_groups, model->num_mesh_groups * sizeof(struct mesh_group*));
    struct mesh_group* mgroup = mesh_group_new();
    mgroup->name = strdup("root_group");
    model->mesh_groups[0] = mgroup;

    for (uint32_t i = 0; i < iqm->header.num_meshes; ++i) {
        struct mesh* nm = iqm_read_mesh(iqm, i, i == 0 ? 0 : model->meshes[i - 1]->num_verts);
        model->num_meshes++;
        model->meshes = realloc(model->meshes, model->num_meshes * sizeof(struct mesh*));
        model->meshes[model->num_meshes - 1] = nm;
        mgroup->num_mesh_offs++;
        mgroup->mesh_offsets = realloc(mgroup->mesh_offsets, mgroup->num_mesh_offs * sizeof(size_t));
        mgroup->mesh_offsets[mgroup->num_mesh_offs - 1] = model->num_meshes - 1;

        /* Assign actual material index */
        void* material = hashmap_get(&material_ids, hm_cast(nm->mat_index));
        if (material) {
            nm->mat_index = *(uint32_t*)material;
        } else {
            nm->mat_index = mgroup->num_materials;
            ++mgroup->num_materials;
        }
    }
    hashmap_destroy(&material_ids);
    return model;
}

struct model* model_from_iqm(const unsigned char* data, size_t sz)
{
    struct iqm_file iqm;
    memset(&iqm, 0, sizeof(struct iqm_file));
    iqm.base = (unsigned char*) data;
    iqm.size = sz;

    /* Read header */
    if (!iqm_read_header(&iqm)) {
        fprintf(stderr, "Not a iqm file!\n");
        return 0;
    }

    /* Read meshdata */
    if (iqm.header.num_meshes > 0) {
        struct model* m = iqm_read_model(&iqm);
        /* Read skeleton */
        if (iqm.header.num_joints > 0) {
            m->skeleton = iqm_read_skeleton(&iqm);
        }
        /* Read frameset */
        if (iqm.header.num_frames > 0) {
            m->frameset = iqm_read_frames(&iqm);
        }
        return m;
    }

    return 0;
}
