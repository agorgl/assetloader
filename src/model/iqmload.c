#include <assets/model/modelload.h>
#include "iqmfile.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <hashmap.h>
#include <linalgb.h>

static struct skeleton* iqm_read_skeleton(struct iqm_file* iqm)
{
    struct iqm_header* h = &iqm->header;
    unsigned char* base = iqm->base;

    struct skeleton* skel = skeleton_new();
    skel->num_joints = h->num_joints;
    skel->joints = realloc(skel->joints, skel->num_joints * sizeof(struct joint));
    memset(skel->joints, 0, skel->num_joints * sizeof(struct joint));

    printf("Num joints: %u\n", h->num_joints);
    for (uint32_t i = 0; i < h->num_joints; ++i) {
        struct iqm_joint* joint = (struct iqm_joint*)(base + h->ofs_joints + i * sizeof(struct iqm_joint));

        /* Set joint parent */
        struct joint* j = skel->joints + i;
        j->parent = joint->parent == -1 ? 0 : skel->joints + joint->parent;

        /* Copy joint data */
        memcpy(j->position, joint->translate, 3 * sizeof(float));
        memcpy(j->rotation, joint->rotate, 4 * sizeof(float));
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

    /* Populate vertices */
    for (int i = 0; i < m->num_verts; ++i) {
        struct vertex* cur_vert = m->vertices + i;
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

static size_t int_hash(void* key) { return (size_t)key; }
static int int_eql(void* k1, void* k2) { return (uint32_t)k1 == (uint32_t)k2; }

static struct model* iqm_read_model(struct iqm_file* iqm)
{
    struct hashmap material_ids;
    hashmap_init(&material_ids, int_hash, int_eql);
    struct model* model = model_new();
    for (uint32_t i = 0; i < iqm->header.num_meshes; ++i) {
        struct mesh* nm = iqm_read_mesh(iqm, i, i == 0 ? 0 : model->meshes[i - 1]->num_verts);
        model->num_meshes++;
        model->meshes = realloc(model->meshes, model->num_meshes * sizeof(struct mesh*));
        model->meshes[model->num_meshes - 1] = nm;

        /* Assign actual material index */
        void* material = hashmap_get(&material_ids, (void*)nm->mat_index);
        if (material) {
            nm->mat_index = *(uint32_t*)material;
        } else {
            nm->mat_index = model->num_materials;
            ++model->num_materials;
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
        return m;
    }

    return 0;
}
