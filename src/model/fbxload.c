#include "assets/model/model.h"
#include "fbxfile.h"
#define _DEBUG
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <hashmap.h>
#include <vector.h>
#include <linalgb.h>

/* Forward declarations */
static int fbx_joint_index(struct fbx_record* objs, int64_t jnt_id);

struct fbx_vertex_weight {
    int32_t bone_index;
    float bone_weight;
};

/*-----------------------------------------------------------------
 * Model construction
 *-----------------------------------------------------------------*/
/* Copy float array */
static void fbx_cpy_fa(float* dst, void* src, size_t len, size_t unit_sz)
{
    if (unit_sz != sizeof(float)) {
        for (size_t i = 0; i < len; ++i) {
            dst[i] = *(double*)((unsigned char*)src + i * unit_sz);
        }
    } else {
        /* Optimized branch */
        memcpy(dst, src, len * sizeof(float));
    }
}

static size_t vertex_hash(hm_ptr key)
{
    struct vertex* v = (struct vertex*)hm_pcast(key);
    return (size_t)(v->position[0]
                  * v->position[1]
                  * v->position[2]
                  * v->normal[0]
                  * v->normal[1]
                  * v->normal[2]);
}

static int vertex_eql(hm_ptr k1, hm_ptr k2)
{
    return memcmp(hm_pcast(k1), hm_pcast(k2), sizeof(struct vertex)) == 0; /* Compare obj vertex index triplets */
}

static struct fbx_property* fbx_find_layer_property(struct fbx_record* geom, const char* layer, const char* subrec)
{
    struct fbx_record* r1 = fbx_find_subrecord_with_name(geom, layer);
    if (!r1)
        return 0;
    struct fbx_record* r2 = fbx_find_subrecord_with_name(r1, subrec);
    if (!r2)
        return 0;
    return r2->properties;
}

static struct mesh* fbx_read_mesh(struct fbx_record* geom, int* indice_offset, int* mat_id, struct hashmap* vw_index)
{
    /* Check if geometry node contains any vertices */
    struct fbx_record* verts_nod = fbx_find_subrecord_with_name(geom, "Vertices");
    if (!verts_nod)
        return 0;
    struct fbx_property* verts = verts_nod->properties;

    /* Find data nodes */
    struct fbx_property* indices = fbx_find_subrecord_with_name(geom, "PolygonVertexIndex")->properties;
    struct fbx_property* norms = fbx_find_layer_property(geom, "LayerElementNormal", "Normals");
    struct fbx_property* tangents = fbx_find_layer_property(geom, "LayerElementTangent", "Tangents");
    struct fbx_property* binormals = fbx_find_layer_property(geom, "LayerElementBinormal", "Binormals");
    struct fbx_property* uvs = fbx_find_layer_property(geom, "LayerElementUV", "UV");
    struct fbx_property* uv_idxs = fbx_find_layer_property(geom, "LayerElementUV", "UVIndex");
    struct fbx_property* mats = fbx_find_layer_property(geom, "LayerElementMaterial", "Materials");
    struct fbx_property* mats_mapping = fbx_find_layer_property(geom, "LayerElementMaterial", "MappingInformationType");

    /* Create mesh */
    struct mesh* mesh = mesh_new();
    size_t vu_sz = fbx_pt_unit_size(verts->type);
    mesh->num_verts = 0;
    mesh->num_indices = 0;
    int stored_indices = indices->length / fbx_pt_unit_size(indices->type);
    int last_material = -1, cur_material = -1;

    /* Allocate top limit */
    mesh->vertices = realloc(mesh->vertices, stored_indices * sizeof(struct vertex));
    mesh->indices = realloc(mesh->indices, stored_indices * 2 * sizeof(uint32_t));
    memset(mesh->vertices, 0, stored_indices * sizeof(struct vertex));
    if (vw_index) {
        mesh->weights = realloc(mesh->weights, stored_indices * sizeof(struct vertex_weight));
        memset(mesh->weights, 0, stored_indices * sizeof(struct vertex_weight));
    }

    /* Used to find and reuse indices of already stored vertices */
    struct hashmap stored_vertices;
    hashmap_init(&stored_vertices, vertex_hash, vertex_eql);

    /* Populate mesh */
    int fc = 0; /* Counter of vertices in running face */
    int tot_pols = 0; /* Counter of polygons encountered so far */
    for (int i = *indice_offset; i < stored_indices; ++i) {
        /* Check if mesh has multiple materials or not */
        if (mats && strncmp("AllSame", mats_mapping->data.str, 7) != 0) {
            /* Gather material for current vertice */
            cur_material = *(mats->data.ip + tot_pols);
        } else {
            cur_material = 0;
        }
        /* Initial value for last_material */
        if (last_material == -1)
            last_material = cur_material;
        /* Check if new mesh should be created according to current material */
        if (last_material != cur_material) {
            *indice_offset = i;
            goto cleanup;
        }
        last_material = cur_material;
        /* NOTE!
         * Negative array values in the positions' indices array exist
         * to indicate the last index of a polygon.
         * To find the actual indice value we must negate it
         * and substract 1 from that value */
        int32_t pos_ind = indices->data.ip[i];
        if (pos_ind < 0) {
            pos_ind = -1 * pos_ind - 1;
            ++tot_pols;
        }
        uint32_t uv_ind = 0;
        if (uvs)
            uv_ind = uv_idxs->data.ip[i];

        /* Fill temporary vertex */
        struct vertex tv;
        memset(&tv, 0, sizeof(struct vertex));
        fbx_cpy_fa(tv.position, verts->data.dp + pos_ind * 3, 3, vu_sz);
        fbx_cpy_fa(tv.normal, norms->data.dp + i * 3, 3, vu_sz);
        if (tangents)
            fbx_cpy_fa(tv.tangent, tangents->data.dp + i * 3, 3, vu_sz);
        if (binormals)
            fbx_cpy_fa(tv.binormal, binormals->data.dp + i * 3, 3, vu_sz);
        if (uvs)
            fbx_cpy_fa(tv.uvs, uvs->data.dp + uv_ind * 2, 2, vu_sz);

        /* Check if current vertex is already stored */
        hm_ptr* stored_indice = hashmap_get(&stored_vertices, hm_cast(&tv));
        if (stored_indice) {
            mesh->indices[mesh->num_indices] = *(uint32_t*)stored_indice;
        } else {
            ++mesh->num_verts;
            /* Store new vertice */
            uint32_t nidx = mesh->num_verts - 1;
            memcpy(mesh->vertices + nidx, &tv, sizeof(struct vertex));
            /* Set indice */
            mesh->indices[mesh->num_indices] = nidx;
            /* Store vertex ptr to lookup table */
            hashmap_put(&stored_vertices, hm_cast(mesh->vertices + nidx), hm_cast(nidx));
            /* Fill parallel vertex weight array with given vertex weights */
            if (vw_index) {
                hm_ptr* p = hashmap_get(vw_index, pos_ind);
                if (p) {
                    struct vertex_weight* tvw = mesh->weights + nidx;
                    struct vector* wlist = hm_pcast(*p);
                    for (int i = 0; i < 4; ++i) {
                        struct fbx_vertex_weight* fbw = vector_at(wlist, i);
                        tvw->bone_ids[i] = fbw->bone_index;
                        tvw->bone_weights[i] = fbw->bone_weight;
                    }
                }
            }
        }

        /*
         * When fc >= 3 we have a polygon that is no more a triangle.
         * For every additional point in the polygon we add two more indices
         * thus adding an additional triangle face. The bellow pattern
         * splits polygons to triangle fans
         */
        if (fc >= 3) {
            int ci = mesh->indices[mesh->num_indices];
            mesh->indices[mesh->num_indices + 0] = ci;
            mesh->indices[mesh->num_indices + 1] = mesh->indices[mesh->num_indices - 1];
            mesh->indices[mesh->num_indices + 2] = mesh->indices[mesh->num_indices - fc];
            mesh->num_indices += 2;
        }

        /* Reset the fc when we reach the end of a face else increase it */
        fc = indices->data.ip[i] < 0 ? 0 : fc + 1;
        ++mesh->num_indices;
    }

    *indice_offset = -1;
cleanup:
    *mat_id = cur_material;
    hashmap_destroy(&stored_vertices);
    return mesh;
}

/*-----------------------------------------------------------------
 * Connections Index
 *-----------------------------------------------------------------*/
struct fbx_conns_idx { struct hashmap index; struct hashmap rev_index; struct hashmap desc_index; };

static size_t id_hash(hm_ptr key) { return (size_t)key; }
static int id_eql(hm_ptr k1, hm_ptr k2) { return k1 == k2; }

static void fbx_build_connections_fw_index(struct fbx_record* connections, struct fbx_conns_idx* cidx)
{
    /* Allocate internal hashmap resources */
    hashmap_init(&cidx->index, id_hash, id_eql);

    /* Iterate through full connections list */
    struct fbx_record* c = connections->subrecords;
    while (c) {
        int64_t child_id = c->properties[1].data.l;
        int64_t parnt_id = c->properties[2].data.l;
        /* Check if parent list exists, if not create a new one */
        struct vector** par_list = (struct vector**)hashmap_get(&cidx->index, child_id);
        if (!par_list) {
            struct vector* plist = malloc(sizeof(struct vector));
            vector_init(plist, sizeof(int64_t));
            hashmap_put(&cidx->index, child_id, hm_cast(plist));
            par_list = &plist;
        }
        /* Put current pair */
        vector_append(*par_list, &parnt_id);
        /* Next */
        c = c->next;
    }
}

static void fbx_build_connections_rev_index(struct fbx_record* connections, struct fbx_conns_idx* cidx)
{
    /* Allocate internal hashmap resources */
    hashmap_init(&cidx->rev_index, id_hash, id_eql);

    /* Iterate through full connections list */
    struct fbx_record* c = connections->subrecords;
    while (c) {
        int64_t child_id = c->properties[1].data.l;
        int64_t parnt_id = c->properties[2].data.l;
        /* Check if parent list exists, if not create a new one */
        struct vector** child_list = (struct vector**)hashmap_get(&cidx->rev_index, parnt_id);
        if (!child_list) {
            struct vector* clist = malloc(sizeof(struct vector));
            vector_init(clist, sizeof(int64_t));
            hashmap_put(&cidx->rev_index, parnt_id, hm_cast(clist));
            child_list = &clist;
        }
        /* Put current pair */
        vector_append(*child_list, &child_id);
        /* Next */
        c = c->next;
    }
}

static void fbx_build_connections_desc_index(struct fbx_record* connections, struct fbx_conns_idx* cidx)
{
    /* Allocate internal hashmap resources */
    hashmap_init(&cidx->desc_index, id_hash, id_eql);

    /* Iterate through full connections list */
    struct fbx_record* c = connections->subrecords;
    while (c) {
        int64_t child_id = c->properties[1].data.l;
        const char* desc = 0;
        if (c->num_props >= 4)
            desc = c->properties[3].data.str;
        hashmap_put(&cidx->desc_index, child_id, hm_cast(desc));
        /* Next */
        c = c->next;
    }
}

static void fbx_build_connections_index(struct fbx_record* connections, struct fbx_conns_idx* cidx)
{
    fbx_build_connections_fw_index(connections, cidx);
    fbx_build_connections_rev_index(connections, cidx);
    fbx_build_connections_desc_index(connections, cidx);
}

static void id_free_iter_fn(hm_ptr key, hm_ptr value)
{
    (void)key;
    struct vector* v = (struct vector*)hm_pcast(value);
    vector_destroy(v);
    free(v);
}

static void fbx_destroy_connections_index(struct fbx_conns_idx* cidx)
{
    /* Destroy description index */
    hashmap_destroy(&cidx->desc_index);
    /* Destroy forward index */
    hashmap_iter(&cidx->index, id_free_iter_fn);
    hashmap_destroy(&cidx->index);
    /* Destroy reverse index */
    hashmap_iter(&cidx->rev_index, id_free_iter_fn);
    hashmap_destroy(&cidx->rev_index);
}

static int64_t fbx_get_first_connection_id(struct fbx_conns_idx* cidx, int64_t id)
{
    int64_t pid = -1;
    hm_ptr* p = hashmap_get(&cidx->index, id);
    if (p) {
        struct vector* par_list = (struct vector*)hm_pcast(*p);
        pid = *(int64_t*)vector_at(par_list, 0);
    }
    return pid;
}

static struct vector* fbx_get_connection_ids(struct hashmap* index, int64_t id)
{
    hm_ptr* p = hashmap_get(index, id);
    if (p) {
        struct vector* list = (struct vector*)hm_pcast(*p);
        return list;
    }
    return 0;
}

static const char* fbx_get_connection_desc(struct fbx_conns_idx* cidx, int64_t id)
{
    hm_ptr* p = hashmap_get(&cidx->desc_index, id);
    if (p) {
        const char* desc = hm_pcast(*p);
        return desc;
    }
    return 0;
}

/*-----------------------------------------------------------------
 * Objects index
 *-----------------------------------------------------------------*/
struct fbx_objs_idx { struct hashmap index; };

static void fbx_build_objs_index(struct fbx_record* objs, struct fbx_objs_idx* objs_idx)
{
    /* Allocate internal hashmap resources */
    hashmap_init(&objs_idx->index, id_hash, id_eql);
    /* Iterate through full objects list */
    struct fbx_record* o = objs->subrecords;
    while (o) {
        int64_t obj_id = o->properties[0].data.l;
        /* Put current pair */
        hashmap_put(&objs_idx->index, obj_id, hm_cast(o));
        /* Next */
        o = o->next;
    }
}

static void fbx_destroy_objs_index(struct fbx_objs_idx* objs_idx)
{
    hashmap_destroy(&objs_idx->index);
}

static struct fbx_record* fbx_find_object_with_id(struct fbx_objs_idx* objs_idx, int64_t id)
{
    hm_ptr* p = hashmap_get(&objs_idx->index, id);
    if (p) {
        struct fbx_record* r = (struct fbx_record*)hm_pcast(*p);
        return r;
    }
    return 0;
}

static struct fbx_record* fbx_find_object_type_with_id(struct fbx_objs_idx* objs_idx, const char* type, int64_t id)
{
    struct fbx_record* r = fbx_find_object_with_id(objs_idx, id);
    if (r && strcmp(type, r->name) == 0)
        return r;
    return 0;
}

/*-----------------------------------------------------------------
 * Indexes bundle
 *-----------------------------------------------------------------*/
struct fbx_indexes {
    struct fbx_conns_idx cidx;
    struct fbx_objs_idx objs_idx;
};

static void fbx_build_indexes(struct fbx_indexes* indexes, struct fbx_record* conns, struct fbx_record* objs)
{
    /* Build connections index */
    fbx_build_connections_index(conns, &indexes->cidx);
    /* Build objects index */
    fbx_build_objs_index(objs, &indexes->objs_idx);
}

static void fbx_destroy_indexes(struct fbx_indexes* indexes)
{
    /* Free objects index */
    fbx_destroy_objs_index(&indexes->objs_idx);
    /* Free connections index */
    fbx_destroy_connections_index(&indexes->cidx);
}

/*-----------------------------------------------------------------
 * Transform post process
 *-----------------------------------------------------------------*/
/* Reads vec3 from Properties70 subrecord */
static void fbx_read_transform_vec(struct fbx_record* r, float* v)
{
    for (int i = 0; i < 3; ++i)
        v[i] = r->properties[4 + i].data.d;
}

/* Returns a positive value if transform data where found and the given matrix was filled */
static int fbx_read_local_transform(struct fbx_record* mdl, float t[3], float r[3], float s[3], int* rot_active, float pre_rot[3])
{
    struct fbx_record* transform_rec = fbx_find_subrecord_with_name(mdl, "Properties70");
    struct fbx_record* p = transform_rec->subrecords;

    int has_transform = 0;

    while (p) {
        size_t pname_len = p->properties[0].length;
        const char* pname = p->properties[0].data.str;

        if (strncmp("Lcl Scaling", pname, pname_len) == 0) {
            fbx_read_transform_vec(p, s);
            has_transform = 1;
        }
        else if (strncmp("Lcl Rotation", pname, pname_len) == 0) {
            fbx_read_transform_vec(p, r);
            has_transform = 1;
        }
        else if (strncmp("Lcl Translation", pname, pname_len) == 0) {
            fbx_read_transform_vec(p, t);
            has_transform = 1;
        }
        else if (strncmp("RotationActive", pname, pname_len) == 0) {
            *rot_active = p->properties[4].data.i;
        }
        else if (strncmp("PreRotation", pname, pname_len) == 0) {
            fbx_read_transform_vec(p, pre_rot);
            has_transform = 1;
        }

        p = p->next;
    }

    return has_transform;
}

/* Composes local transform components into their matrix */
static void fbx_compose_local_transform(mat4* transform, float t[3], float r[3], float s[3], int rot_active, float pre_rot[3])
{
    *transform = mat4_id();
    /* Lcl Translation */
    *transform = mat4_mul_mat4(*transform, mat4_translation(vec3_new(t[0], t[1], t[2])));
    /* Lcl Rotation */
    *transform = mat4_mul_mat4(*transform, mat4_rotation_quat(quat_from_euler(vec3_new(radians(r[1]),
                                                                                       radians(r[0]),
                                                                                       radians(r[2])))));
    /* PreRotation */
    if (rot_active)
        *transform = mat4_mul_mat4(*transform, mat4_rotation_quat(quat_from_euler(vec3_new(radians(pre_rot[1]),
                                                                                           radians(pre_rot[0]),
                                                                                           radians(pre_rot[2])))));
    /* Lcl Scaling */
    *transform = mat4_mul_mat4(*transform, mat4_scale(vec3_new(s[0], s[1], s[2])));
}

static int fbx_read_transform(struct fbx_indexes* indexes, int64_t mdl_id, mat4* out)
{
    const char* model_node_name = "Model";
    /* List with subsequent model node id's until we reach parent */
    int64_t cur_id = mdl_id;
    struct vector chain;
    vector_init(&chain, sizeof(int64_t));
    vector_append(&chain, &cur_id);
    while(cur_id) {
        /* Loop through parents */
        int found_parent_id = 0;
        struct vector* par_list = fbx_get_connection_ids(&indexes->cidx.index, cur_id);
        for (size_t i = 0; i < par_list->size; ++i) {
            /* Check if parent id is a model id */
            int64_t cpid = *(int64_t*)vector_at(par_list, i);
            struct fbx_record* mdl = fbx_find_object_type_with_id(&indexes->objs_idx, model_node_name, cpid);
            if (mdl) {
                vector_append(&chain, &cpid);
                cur_id = cpid;
                found_parent_id = 1;
                break;
            }
        }
        if (!found_parent_id)
            break;
    }

    /* Construct transform */
    *out = mat4_id();
    int has_transform = 0;
    for (size_t i = 0; i < chain.size; ++i) {
        int64_t id = *(int64_t*)vector_at(&chain, i);
        struct fbx_record* mdl_node = fbx_find_object_type_with_id(&indexes->objs_idx, model_node_name, id);
        //printf("Chain(%d): %lu -> ", mdl_node ? 1 : 0, id);
        if (mdl_node) {
            /* Local Transforms */
            float s[3] = {1.0f, 1.0f, 1.0f}, r[3] = {0.0f, 0.0f, 0.0f}, t[3] = {0.0f, 0.0f, 0.0f};
            /* Pre/Post rotations */
            int rot_active = 0; float pre_rot[3];
            if (fbx_read_local_transform(mdl_node, t, r, s, &rot_active, pre_rot)) {
                mat4 cur;
                fbx_compose_local_transform(&cur, t, r, s, rot_active, pre_rot);
                *out = mat4_mul_mat4(*out, cur);
                has_transform = 1;
            }
        }
    }
    vector_destroy(&chain);
    //printf("Root!\n");
    return has_transform;
}

/* Parses "d|X", "d|Y", "d|Z" properties of an AnimationCurveNode element into given data array */
static void fbx_read_animation_curve_node(struct fbx_record* acn_node, float data[3])
{
    struct fbx_record* p70 = acn_node->subrecords + 0;
    struct fbx_record* p = p70->subrecords;
    while (p) {
        const char* name = p->properties[0].data.str;
        size_t name_sz = p->properties[0].length;
        if (name_sz >= 3) {
            char component = name[2];
            float value = p->properties[4].data.d;
            switch (component) {
                case 'X':
                    data[0] = value;
                    break;
                case 'Y':
                    data[1] = value;
                    break;
                case 'Z':
                    data[2] = value;
                    break;
            }
        }
        p = p->next;
    }
}

/* Searches for AnimationCurveNode elements assosiated with the given model id,
 * parses them and fills given data array. Returns bitflag of the components filled */
static int fbx_read_acn_transform(struct fbx_indexes* indexes, int64_t mdl_id, float t[3], float r[3], float s[3])
{
    const char* acn_node_name = "AnimationCurveNode";
    /* Get child ids for current model node */
    struct vector* acn_chld_node_ids = fbx_get_connection_ids(&indexes->cidx.rev_index, mdl_id);
    /* Search which of them are the animation curve nodes we want */
    int filled = 0;
    for (size_t i = 0; i < acn_chld_node_ids->size; ++i) {
        int64_t chld_id = *(int64_t*)vector_at(acn_chld_node_ids, i);
        struct fbx_record* rec = fbx_find_object_type_with_id(&indexes->objs_idx, acn_node_name, chld_id);
        if (!rec)
            continue;
        /* */
        const char* type = rec->properties[1].data.str;
        size_t type_sz = rec->properties[1].length;
        if (strncmp("T", type, type_sz) == 0) {
            fbx_read_animation_curve_node(rec, t);
            filled |= 1 << 1;
        }
        else if (strncmp("R", type, type_sz) == 0) {
            fbx_read_animation_curve_node(rec, r);
            filled |= 1 << 2;
        }
        else if (strncmp("S", type, type_sz) == 0) {
            fbx_read_animation_curve_node(rec, s);
            filled |= 1 << 3;
        }
    }
    return filled;
}

static void fbx_transform_vertices(struct mesh* m, mat4 transform)
{
    for (int i = 0; i < m->num_verts; ++i) {
        /* Transform positions */
        float* pos = m->vertices[i].position;
        vec3 npos = mat4_mul_vec3(transform, vec3_new(pos[0], pos[1], pos[2]));
        pos[0] = npos.x;
        pos[1] = npos.y;
        pos[2] = npos.z;
    }
}

/*-----------------------------------------------------------------
 * Vertex Weights
 *-----------------------------------------------------------------*/
/* Creates an index assosiating vertex indexes with lists of weight data for a given Geometry node */
static void fbx_build_vertex_weights_index(struct fbx_record* geom, struct fbx_record* objs, struct fbx_indexes* indexes, struct hashmap** weight_index)
{
    const char* deformer_node_name = "Deformer";
    /* Search Deformer child tagged with "Skin" */
    struct vector* geom_chld_ids = fbx_get_connection_ids(&indexes->cidx.rev_index, geom->properties[0].data.l);
    if (!geom_chld_ids)
        return;
    /* Search for skin node id */
    int64_t geom_chld_id = -1;
    for (size_t i = 0; i < geom_chld_ids->size; ++i) {
        int64_t chld_id = *(int64_t*)vector_at(geom_chld_ids, i);
        struct fbx_record* skin_node = fbx_find_object_type_with_id(&indexes->objs_idx, deformer_node_name, chld_id);
        if (skin_node && strncmp("Skin", skin_node->properties[2].data.str, 4) == 0) {
            geom_chld_id = chld_id;
            break;
        }
    }
    if (geom_chld_id == -1)
        return;
    /* Initialize index data structure */
    *weight_index = malloc(sizeof(struct hashmap));
    hashmap_init(*weight_index, id_hash, id_eql);
    /* Get Deformer::Skin's childs */
    struct vector* skin_chld_ids = fbx_get_connection_ids(&indexes->cidx.rev_index, geom_chld_id);
    for (size_t i = 0; i < skin_chld_ids->size; ++i) {
        int64_t skin_chld_id = *(int64_t*)vector_at(skin_chld_ids, i);
        struct fbx_record* cluster_node = fbx_find_object_type_with_id(&indexes->objs_idx, deformer_node_name, skin_chld_id);
        /* Check if current Deformer node is tagged with "Cluster" */
        if (cluster_node && strncmp("Cluster", cluster_node->properties[2].data.str, 7) == 0) {
            /* Get refering node */
            struct vector* cluster_chld_ids = fbx_get_connection_ids(&indexes->cidx.rev_index, skin_chld_id);
            struct fbx_record* ref_bone = 0;
            for (size_t i = 0; i < cluster_chld_ids->size; ++i) {
                int64_t clust_chld_id = *(int64_t*)vector_at(cluster_chld_ids, i);
                struct fbx_record* model_node = fbx_find_object_type_with_id(&indexes->objs_idx, "Model", clust_chld_id);
                if (model_node) {
                    ref_bone = model_node;
                    break;
                }
            }
            /* If given deformer cluster is assosiated with a given bone */
            if (ref_bone) {
                /* Get referring bone index */
                int joint_index = fbx_joint_index(objs, ref_bone->properties[0].data.l);
                /* Search for weight and index lists */
                struct fbx_record* r = cluster_node->subrecords;
                struct fbx_property* weights = 0, *indexes = 0;
                while (r) {
                    if (strncmp("Weights", r->name, 7) == 0) {
                        weights = r->properties + 0;
                    } else if (strncmp("Indexes", r->name, 7) == 0) {
                        indexes = r->properties + 0;
                    }
                    r = r->next;
                }
                /* If both exist fill vertex weight hashmap with data */
                if (weights && indexes) {
                    for (unsigned int i = 0; i < indexes->length / fbx_pt_unit_size(indexes->type); ++i) {
                        int64_t idx = indexes->data.ip[i];
                        double w = weights->data.dp[i];
                        /* Check if weight list exists, if not create a new one */
                        struct vector** wt_list = (struct vector**)hashmap_get(*weight_index, idx);
                        if (!wt_list) {
                            struct vector* wlist = malloc(sizeof(struct vector));
                            vector_init(wlist, sizeof(struct fbx_vertex_weight));
                            hashmap_put(*weight_index, idx, hm_cast(wlist));
                            wt_list = &wlist;
                        }
                        /* Put current value */
                        struct fbx_vertex_weight vw;
                        vw.bone_index = joint_index;
                        vw.bone_weight = w;
                        vector_append(*wt_list, &vw);
                    }
                }
            }
        }
    }
}

static void weight_index_free_iter(hm_ptr key, hm_ptr value)
{
    (void)key;
    struct vector* v = (struct vector*)hm_pcast(value);
    vector_destroy(v);
    free(v);
}

static void fbx_destroy_weight_index(struct hashmap* weight_index)
{
    hashmap_iter(weight_index, weight_index_free_iter);
    hashmap_destroy(weight_index);
    free(weight_index);
}

/*-----------------------------------------------------------------
 * Materials
 *-----------------------------------------------------------------*/
/* Searches for materials ids for model node with the given id */
static void fbx_find_materials_for_model(struct fbx_record* objs, struct fbx_conns_idx* cidx, int64_t mdl_id, struct vector* mat_ids)
{
    const char* mat_node_name = "Material";
    /* Iterate through all material nodes */
    struct fbx_record* mat = fbx_find_subrecord_with_name(objs, mat_node_name);
    while (mat) {
        int64_t mat_id = mat->properties[0].data.l;
        /* Check if given model uses current material */
        struct vector* par_list = fbx_get_connection_ids(&cidx->index, mat_id);
        if (par_list) {
            /* Search model id in material's parent list */
            for (size_t i = 0; i < par_list->size; ++i) {
                int64_t pid = *(int64_t*)vector_at(par_list, i);
                if (pid == mdl_id) {
                    vector_append(mat_ids, &mat_id);
                    break;
                }
            }
        }
        /* Process next material node */
        mat = fbx_find_sibling_with_name(mat, mat_node_name);
    }
}

static size_t mat_id_hash(hm_ptr key) { return (size_t)key; }
static int mat_id_eql(hm_ptr k1, hm_ptr k2) { return k1 == k2; }

/*-----------------------------------------------------------------
 * Model
 *-----------------------------------------------------------------*/
static struct model* fbx_read_model(struct fbx_record* obj, struct fbx_indexes* indexes)
{
    /* Map that maps internal material ids to ours */
    struct hashmap mat_map;
    hashmap_init(&mat_map, mat_id_hash, mat_id_eql);

    /* Gather model data */
    struct model* model = model_new();
    struct fbx_record* geom = fbx_find_subrecord_with_name(obj, "Geometry");
    while (geom) {
        /* Get model node corresponding to current geometry node */
        int64_t model_node_id = fbx_get_first_connection_id(&indexes->cidx, geom->properties[0].data.l);
        struct fbx_record* mdl_node = fbx_find_object_type_with_id(&indexes->objs_idx, "Model", model_node_id);
        /* Create a list with the material ids */
        struct vector mat_ids;
        vector_init(&mat_ids, sizeof(int64_t));
        fbx_find_materials_for_model(obj, &indexes->cidx, model_node_id, &mat_ids);
        /* Create vertex weight index */
        struct hashmap* vw_index = 0;
        fbx_build_vertex_weights_index(geom, obj, indexes, &vw_index);

        /* A single geometry node can be multiple meshes, due to non uniform materials.
         * Param indice_offset is filled with -1 if there are no more data to process
         * in current geom node, or with a value that must be passed to subsequent
         * calls of the fbx_read_mesh function to gather next meshes. */
        int indice_offset = 0;
        int mat_idx = 0;
        do {
            struct mesh* nm = fbx_read_mesh(geom, &indice_offset, &mat_idx, vw_index);
            if (nm) {
                /* Append new mesh */
                model->num_meshes++;
                model->meshes = realloc(model->meshes, model->num_meshes * sizeof(struct mesh*));
                model->meshes[model->num_meshes - 1] = nm;
                /* Check if a transform matrix is available and transform if appropriate */
                if (mdl_node) {
                    mat4 transform;
                    int has_transform = fbx_read_transform(indexes, model_node_id, &transform);
                    if (has_transform) {
                        fbx_transform_vertices(nm, transform);
                    }
                }
                /* Set material */
                if (mat_ids.size > 0) {
                    int64_t fbx_mat_id = *(int64_t*)vector_at(&mat_ids, mat_idx);
                    /* Try to find fbx material id in materials map,
                     * if not add it as a new pair with next available id */
                    hm_ptr* p = hashmap_get(&mat_map, hm_cast(fbx_mat_id));
                    if (!p) {
                        nm->mat_index = mat_map.size;
                        hashmap_put(&mat_map, hm_cast(fbx_mat_id), hm_cast(nm->mat_index));
                    } else {
                        nm->mat_index = *p;
                    }
                }
            }
        } while (indice_offset != -1);

        /* Free vertex weights index */
        if (vw_index)
            fbx_destroy_weight_index(vw_index);
        /* Free materials list */
        vector_destroy(&mat_ids);
        /* Process next mesh */
        geom = fbx_find_sibling_with_name(geom, "Geometry");
    }

    /* Free materials map */
    hashmap_destroy(&mat_map);
    return model;
}

/*-----------------------------------------------------------------
 * Skeleton
 *-----------------------------------------------------------------*/
static int fbx_is_joint_type(const char* type)
{
    return strncmp("LimbNode", type, 8) == 0
        || strncmp("Null", type, 4) == 0;
}

static int fbx_joint_count(struct fbx_record* objs)
{
    const char* mdl_node_name = "Model";
    struct fbx_record* mdl = fbx_find_subrecord_with_name(objs, mdl_node_name);
    int count = 0;
    while (mdl) {
        const char* type = mdl->properties[2].data.str;
        if (fbx_is_joint_type(type))
            ++count;
        /* Process next model node */
        mdl = fbx_find_sibling_with_name(mdl, mdl_node_name);
    }
    return count;
}

static int fbx_joint_index(struct fbx_record* objs, int64_t jnt_id)
{
    /* Get offset of the joint model node */
    const char* mdl_node_name = "Model";
    int ofs = 0;
    struct fbx_record* mdl = fbx_find_subrecord_with_name(objs, mdl_node_name);
    while (mdl) {
        int64_t mid = mdl->properties[0].data.l;
        if (mid == jnt_id)
            return ofs;
        const char* type = mdl->properties[2].data.str;
        if (fbx_is_joint_type(type))
            ++ofs;
        /* Process next model node */
        mdl = fbx_find_sibling_with_name(mdl, mdl_node_name);
    }
    return -1;
}

static int fbx_joint_parent_index(struct fbx_record* objs, struct fbx_indexes* indexes, int64_t child_id)
{
    /* Get parent connection id */
    struct vector* par_list = fbx_get_connection_ids(&indexes->cidx.index, child_id);
    int64_t par_id = -1;
    int par_ofs = -1;
    if (par_list) {
        for (size_t i = 0; i < par_list->size; ++i) {
            /* Check if current parent id is a joint */
            int64_t cpid = *(int64_t*)vector_at(par_list, i);
            struct fbx_record* mdl = fbx_find_object_type_with_id(&indexes->objs_idx, "Model", cpid);
            if (mdl) {
                const char* type = mdl->properties[2].data.str;
                if (fbx_is_joint_type(type)) {
                    par_id = cpid;
                    break;
                }
            }
        }
    }

    if (par_id != -1) {
        par_ofs = fbx_joint_index(objs, par_id);
    }

    return par_ofs;
}

static struct skeleton* fbx_read_skeleton(struct fbx_record* objs, struct fbx_indexes* indexes)
{
    /* Allocate skeleton */
    int jcount = fbx_joint_count(objs);
    if (jcount == 0)
        return 0;
    /* Joints */
    struct skeleton* skel = skeleton_new();
    skel->rest_pose->num_joints = jcount;
    skel->rest_pose->joints = realloc(skel->rest_pose->joints, skel->rest_pose->num_joints * sizeof(struct joint));
    memset(skel->rest_pose->joints, 0, skel->rest_pose->num_joints * sizeof(struct joint));
    /* Joint names */
    skel->joint_names = realloc(skel->joint_names, skel->rest_pose->num_joints * sizeof(char*));
    memset(skel->joint_names, 0, skel->rest_pose->num_joints * sizeof(char*));
    printf("Num joints: %u\n", jcount);

    /* Iterate joints type Model nodes */
    const char* mdl_node_name = "Model";
    struct fbx_record* mdl = fbx_find_subrecord_with_name(objs, mdl_node_name);
    int cur_joint_idx = 0;
    while (mdl) {
        int64_t mdl_id = mdl->properties[0].data.l;
        const char* type = mdl->properties[2].data.str;
        if (fbx_is_joint_type(type)) {
            /* Copy joint name */
            const char* name = mdl->properties[1].data.str;
            size_t name_sz = strlen(name) * sizeof(char);
            skel->joint_names[cur_joint_idx] = malloc(name_sz + 1);
            memcpy(skel->joint_names[cur_joint_idx], name, name_sz);
            *(skel->joint_names[cur_joint_idx] + name_sz) = 0;
            /* Set joint parent */
            struct joint* j = skel->rest_pose->joints + cur_joint_idx;
            int par_idx = fbx_joint_parent_index(objs, indexes, mdl_id);
            j->parent = par_idx == -1 ? 0 : skel->rest_pose->joints + par_idx;
            /* Local Transforms */
            float s[3] = {1.0f, 1.0f, 1.0f}, r[3] = {0.0f, 0.0f, 0.0f}, t[3] = {0.0f, 0.0f, 0.0f};
            /* Pre/Post rotations */
            int rot_active = 0; float pre_rot[3] = {0.0f, 0.0f, 0.0f};
            fbx_read_local_transform(mdl, t, r, s, &rot_active, pre_rot);
            /* AnimationCurveNode transform */
            float acn_s[3] = {1.0f, 1.0f, 1.0f}, acn_r[3] = {0.0f, 0.0f, 0.0f}, acn_t[3] = {0.0f, 0.0f, 0.0f};
            fbx_read_acn_transform(indexes, mdl_id, acn_t, acn_r, acn_s);

            /* Note: quat_from_euler param order is y,x,z */
            quat rq = quat_from_euler(vec3_new(radians(r[1]),
                                               radians(r[0]),
                                               radians(r[2])));
            if (rot_active) {
                quat prq = quat_from_euler(vec3_new(radians(pre_rot[1]),
                                                    radians(pre_rot[0]),
                                                    radians(pre_rot[2])));
                rq = quat_mul_quat(prq, rq);
            }

            /* Copy joint data */
            memcpy(j->position, t, 3 * sizeof(float));
            memcpy(j->rotation, &rq, 4 * sizeof(float));
            memcpy(j->scaling, s, 3 * sizeof(float));
            ++cur_joint_idx;
        }
        /* Process next model node */
        mdl = fbx_find_sibling_with_name(mdl, mdl_node_name);
    }

    return skel;
}

/*-----------------------------------------------------------------
 * Global Settings
 *-----------------------------------------------------------------*/
enum frame_rate {
    frame_rate_default         = 0,
    frame_rate_120             = 1,
    frame_rate_100             = 2,
    frame_rate_60              = 3,
    frame_rate_50              = 4,
    frame_rate_48              = 5,
    frame_rate_30              = 6,
    frame_rate_30_drop         = 7,
    frame_rate_ntsc_drop_frame = 8,
    frame_rate_ntsc_full_frame = 9,
    frame_rate_pal             = 10,
    frame_rate_cinema          = 11,
    frame_rate_1000            = 12,
    frame_rate_cinema_nd       = 13,
    frame_rate_custom          = 14
};

static float fbx_framerate(struct fbx_record* gsettings)
{
    struct fbx_record* p70_rec = fbx_find_subrecord_with_name(gsettings, "Properties70");
    struct fbx_record* p = p70_rec->subrecords;

    enum frame_rate fr_rate = frame_rate_default;
    float cust_fr_rate = 0.0f;
    /* Read frame rate properties */
    while (p) {
        size_t pname_len = p->properties[0].length;
        const char* pname = p->properties[0].data.str;
        if (strncmp("TimeMode", pname, pname_len) == 0) {
            fr_rate = p->properties[4].data.i;
        } else if (strncmp("CustomFrameRate", pname, pname_len) == 0) {
            cust_fr_rate = p->properties[4].data.i;
        }
        p = p->next;
    }
    /* Get frame rate value */
    float fr = 0.0f;
    switch (fr_rate) {
        case frame_rate_default:
            fr = 1.0f;
            break;
        case frame_rate_120:
            fr = 120.0f;
            break;
        case frame_rate_100:
            fr = 100.0f;
            break;
        case frame_rate_60:
            fr = 60.0f;
            break;
        case frame_rate_50:
            fr = 50.0f;
            break;
        case frame_rate_48:
            fr = 48.0f;
            break;
        case frame_rate_30:
        case frame_rate_30_drop:
            fr = 30.0f;
            break;
        case frame_rate_ntsc_drop_frame:
        case frame_rate_ntsc_full_frame:
            fr = 29.9700262f;
            break;
        case frame_rate_pal:
            fr = 25.0f;
            break;
        case frame_rate_cinema:
            fr = 24.0f;
            break;
        case frame_rate_1000:
            fr = 1000.0f;
            break;
        case frame_rate_cinema_nd:
            fr = 23.976f;
            break;
        case frame_rate_custom:
            fr = cust_fr_rate;
            break;
        default:
            fr = -1.0f;
            break;
    }
    return fr;
}

struct fbx_transform_orientation {
    size_t indexes[3];
    float signs[3];
};

static void fbx_global_orientation(struct fbx_record* gsettings, float sgn[3], size_t val[3])
{
    struct fbx_record* p70_rec = fbx_find_subrecord_with_name(gsettings, "Properties70");
    struct fbx_record* p = p70_rec->subrecords;

    /* Read frame rate properties */
    float signs[3] = { 1.0f, 1.0f, 1.0f };
    size_t values[3] = { 0.0f, 1.0f, 2.0f };
    while (p) {
        const char* pname = p->properties[0].data.str;
        if (strncmp("CoordAxisSign", pname, 13) == 0) {
            signs[0] = p->properties[4].data.i;
        } else if (strncmp("CoordAxis", pname, 9) == 0) { /* X axis */
            values[0] = p->properties[4].data.i;
        } else if (strncmp("UpAxisSign", pname, 10) == 0) {
            signs[1] = p->properties[4].data.i;
        } else if (strncmp("UpAxis", pname, 6) == 0) { /* Y axis */
            values[1] = p->properties[4].data.i;
        } else if (strncmp("FrontAxisSign", pname, 13) == 0) {
            signs[2] = p->properties[4].data.i;
        } else if (strncmp("FrontAxis", pname, 9) == 0) { /* Z axis */
            values[2] = p->properties[4].data.i;
        }
        p = p->next;
    }
    memcpy(sgn, signs, 3 * sizeof(float));
    memcpy(val, values, 3 * sizeof(size_t));
}

#define convert_fbx_time(time) (((float)time) / 46186158000L)

static int fbx_find_num_frames(struct fbx_record* objs)
{
    const char* anim_curve_node_name = "AnimationCurve";
    int max_num_frames = 0;
    /* Iterate through all AnimationCurve object nodes */
    struct fbx_record* anim_curve = fbx_find_subrecord_with_name(objs, anim_curve_node_name);
    while (anim_curve) {
        /* Iterate through subproperties */
        struct fbx_record* p = anim_curve->subrecords;
        while (p) {
            if (strcmp(p->name, "KeyTime") == 0) {
                struct fbx_property* prop = p->properties + 0;
                int num_frames = prop->length / fbx_pt_unit_size(prop->type);
                /* Set new max */
                if (num_frames > max_num_frames)
                    max_num_frames = num_frames;
                break;
            }
            p = p->next;
        }
        /* Process next anim curve node */
        anim_curve = fbx_find_sibling_with_name(anim_curve, anim_curve_node_name);
    }
    return max_num_frames;
}

static float fbx_calc_anim_curv_value(struct fbx_record* anim_curv_node, int cur_frame, int max_frames, float framerate)
{
    struct fbx_property* key_value = 0;
    struct fbx_property* key_time = 0;
    /* Gather the two relevant properties */
    struct fbx_record* p = anim_curv_node->subrecords;
    while (p) {
        if (strcmp("KeyValueFloat", p->name) == 0) {
            key_value = p->properties + 0;
        } else if (strcmp("KeyTime", p->name) == 0) {
            key_time = p->properties + 0;
        }
        p = p->next;
    }

    /* Calc the corresponding value */
    float frame_time = 1.0f / framerate;
    size_t key_time_arr_sz = key_time->length / fbx_pt_unit_size(key_time->type);
    float cur_frame_perc = ((float)cur_frame / max_frames);
    float cur_ideal_time = cur_frame * frame_time;
    /* Initial value by lerp */
    size_t key_time_idx = (key_time_arr_sz - 1) * cur_frame_perc;
    int found = 0;
    do {
        /* Reached one of the boundaries */
        if (key_time_idx == 0 || key_time_idx == key_time_arr_sz - 1)
            break;
        /* Calc distances */
        found = 1;
        float cur_distance = fabs(convert_fbx_time(key_time->data.lp[key_time_idx]) - cur_ideal_time);
        float next_distance = fabs(convert_fbx_time(key_time->data.lp[key_time_idx + 1]) - cur_ideal_time);
        float prev_distance = fabs(convert_fbx_time(key_time->data.lp[key_time_idx - 1]) - cur_ideal_time);
        /* Check immediate right and immediate left distances */
        if (next_distance < cur_distance) {
            ++key_time_idx;
            found = 0;
        } else if (prev_distance < cur_distance) {
            --key_time_idx;
            found = 0;
        }
    } while (!found);

    /* Fetch value for given index */
    float value = key_value->data.fp[key_time_idx];
    return value;
}

/* Returns bitflag of components filled */
static int fbx_read_frame_transform(struct fbx_indexes* indexes, int mdl_id, int cur_frame, int max_frames, float framerate, float t[3], float r[3], float s[3])
{
    /* Result bitflag */
    int components_filled = 0;
    /* Get AnimationCurveNode childs of model node */
    struct vector* mdl_chld_ids = fbx_get_connection_ids(&indexes->cidx.rev_index, mdl_id);
    for (size_t i = 0; i < mdl_chld_ids->size; ++i) {
        int64_t mdl_chld_id = *(int64_t*)vector_at(mdl_chld_ids, i);
        struct fbx_record* rec = fbx_find_object_type_with_id(&indexes->objs_idx, "AnimationCurveNode", mdl_chld_id);
        if (rec) {
            /* Select the transform component to fill */
            const char* component_type = rec->properties[1].data.str;
            size_t component_type_sz = rec->properties[1].length;
            float* component_target = 0;
            if (strncmp("T", component_type, component_type_sz) == 0) {
                component_target = t;
                components_filled |= (1 << 1);
            }
            else if (strncmp("R", component_type, component_type_sz) == 0) {
                component_target = r;
                components_filled |= (1 << 2);
            }
            else if (strncmp("S", component_type, component_type_sz) == 0) {
                component_target = s;
                components_filled |= (1 << 3);
            }

            /* Find AnimationCurveNode's AnimationCurve childs */
            struct vector* acn_chld_ids = fbx_get_connection_ids(&indexes->cidx.rev_index, mdl_chld_id);
            for (size_t j = 0; j < acn_chld_ids->size; ++j) {
                int64_t acn_chld_id = *(int64_t*)vector_at(acn_chld_ids, j);
                /* Get child description */
                const char* desc = fbx_get_connection_desc(&indexes->cidx, acn_chld_id);
                int tidx = -1;
                if (strncmp("d|X", desc, 3) == 0)
                    tidx = 0;
                else if (strncmp("d|Y", desc, 3) == 0)
                    tidx = 1;
                else if (strncmp("d|Z", desc, 3) == 0)
                    tidx = 2;

                if (tidx != -1) {
                    struct fbx_record* anim_curve_node = fbx_find_object_with_id(&indexes->objs_idx, acn_chld_id);
                    component_target[tidx] = fbx_calc_anim_curv_value(anim_curve_node, cur_frame, max_frames, framerate);
                }
            }
        }
    }
    return components_filled;
}

static struct frameset* fbx_read_frames(struct fbx_record* objs, struct fbx_indexes* indexes, float framerate)
{
    /* Create empty frameset */
    struct frameset* fset = frameset_new();
    fset->num_frames = fbx_find_num_frames(objs);
    fset->frames = malloc(fset->num_frames * sizeof(struct frame*));
    memset(fset->frames, 0, fset->num_frames * sizeof(struct frame*));

    /* Prepopulate with empty frames */
    int jcount = fbx_joint_count(objs);
    for (uint32_t i = 0; i < fset->num_frames; ++i) {
        struct frame* fr = frame_new();
        fr->num_joints = jcount;
        fr->joints = realloc(fr->joints, fr->num_joints * sizeof(struct joint));
        memset(fr->joints, 0, fr->num_joints * sizeof(struct joint));
        fset->frames[i] = fr;
    }

    /* Iterate joints type Model nodes */
    const char* mdl_node_name = "Model";
    struct fbx_record* mdl = fbx_find_subrecord_with_name(objs, mdl_node_name);
    int cur_joint_idx = 0;
    while (mdl) {
        int64_t mdl_id = mdl->properties[0].data.l;
        const char* type = mdl->properties[2].data.str;
        if (fbx_is_joint_type(type)) {
            /* Local Transforms */
            float s[3] = {1.0f, 1.0f, 1.0f}, r[3] = {0.0f, 0.0f, 0.0f}, t[3] = {0.0f, 0.0f, 0.0f};
            int rot_active = 0; float pre_rot[3] = {0.0f, 0.0f, 0.0f};
            fbx_read_local_transform(mdl, t, r, s, &rot_active, pre_rot);
            /* AnimationCurveNode transforms */
            /*
            float acn_s[3] = {1.0f, 1.0f, 1.0f}, acn_r[3] = {0.0f, 0.0f, 0.0f}, acn_t[3] = {0.0f, 0.0f, 0.0f};
            fbx_read_acn_transform(objs_idx, cidx, mdl_id, acn_t, acn_r, acn_s);
            */
            /* Get parent index */
            int par_idx = fbx_joint_parent_index(objs, indexes, mdl_id);
            /* Iterate through each frame */
            for (uint32_t i = 0; i < fset->num_frames; ++i) {
                struct joint* j = fset->frames[i]->joints + cur_joint_idx;
                float fs[3] = {1.0f, 1.0f, 1.0f}, fr[3] = {0.0f, 0.0f, 0.0f}, ft[3] = {0.0f, 0.0f, 0.0f};
                /* Fallback to local transform if a component had no frame transform */
                int components_filled = fbx_read_frame_transform(indexes, mdl_id, i, fset->num_frames, framerate, ft, fr, fs);
                float* ss = s; float* rr = r; float* tt = t;
                if (components_filled & (1 << 1)) tt = ft;
                if (components_filled & (1 << 2)) rr = fr;
                if (components_filled & (1 << 3)) ss = fs;

                quat rq = quat_from_euler(vec3_new(radians(rr[1]),
                                                   radians(rr[0]),
                                                   radians(rr[2])));
                if (rot_active) {
                    quat prq = quat_from_euler(vec3_new(radians(pre_rot[1]),
                                                        radians(pre_rot[0]),
                                                        radians(pre_rot[2])));
                    rq = quat_mul_quat(prq, rq);
                }
                memcpy(j->position, tt, 3 * sizeof(float));
                memcpy(j->rotation, &rq, 4 * sizeof(float));
                memcpy(j->scaling, ss, 3 * sizeof(float));
                j->parent = par_idx == -1 ? 0 : fset->frames[i]->joints + par_idx;
            }
            ++cur_joint_idx;
        }
        /* Process next model node */
        mdl = fbx_find_sibling_with_name(mdl, mdl_node_name);
    }

    return fset;
}

/*-----------------------------------------------------------------
 * Constructor
 *-----------------------------------------------------------------*/
struct model* model_from_fbx(const unsigned char* data, size_t sz)
{
    /* Initialize parser state */
    struct parser_state ps;
    memset(&ps, 0, sizeof(struct parser_state));
    ps.data = (unsigned char*) data;
    ps.cur = (unsigned char*) data;
    ps.bufend = (unsigned char*) data + sz;

    /* Initialize fbx file */
    struct fbx_file fbx;
    memset(&fbx, 0, sizeof(struct fbx_file));

    /* Read header */
    if (!fbx_read_header(&ps, &fbx)) {
        fprintf(stderr, "Not a fbx file!\n");
        return 0;
    }
    printf("Version: %d\n", fbx.version);

    /* Parse fbx file */
    struct fbx_record* r = fbx_read_root_record(&ps);
    fbx.root = r;

    /* Build indexes */
    struct fbx_record* conns = fbx_find_subrecord_with_name(fbx.root, "Connections");
    struct fbx_record* objs = fbx_find_subrecord_with_name(fbx.root, "Objects");
    struct fbx_indexes indexes;
    fbx_build_indexes(&indexes, conns, objs);

    /* Parse orientation settings */
    struct fbx_record* gsettings = fbx_find_subrecord_with_name(fbx.root, "GlobalSettings");
    struct fbx_transform_orientation gorient;
    fbx_global_orientation(gsettings, gorient.signs, gorient.indexes);
    printf("Orientation: Sign(%.1f, %.1f, %.1f) Idx(%lu, %lu, %lu)\n",
            gorient.signs[0], gorient.signs[1], gorient.signs[2],
            gorient.indexes[0], gorient.indexes[1], gorient.indexes[2]);

    /* Gather model data from parsed tree  */
    struct model* m = fbx_read_model(objs, &indexes);

    /* Gather skeleton data */
    m->skeleton = fbx_read_skeleton(objs, &indexes);

    /* Retrieve animation framerate */
    float fr = fbx_framerate(gsettings);
    printf("Framerate: %f\n", fr);

    /* Read frameset */
    if (m->skeleton)
        m->frameset = fbx_read_frames(objs, &indexes, fr);

    /* Free indexes */
    fbx_destroy_indexes(&indexes);
    /* Free tree */
    fbx_record_destroy(r);
    return m;
}
