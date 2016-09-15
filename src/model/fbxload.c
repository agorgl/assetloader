#include "assets/model/model.h"
#include "fbxfile.h"
#define _DEBUG
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <hashmap.h>
#include <vector.h>
#include <linalgb.h>

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

static struct mesh* fbx_read_mesh(struct fbx_record* geom, int* indice_offset, int* mat_id)
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
struct fbx_conns_idx { struct hashmap index; };

static size_t id_hash(hm_ptr key) { return (size_t)key; }
static int id_eql(hm_ptr k1, hm_ptr k2) { return k1 == k2; }

static void fbx_build_connections_index(struct fbx_record* connections, struct fbx_conns_idx* cidx)
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

static void id_free_iter_fn(hm_ptr key, hm_ptr value)
{
    (void)key;
    struct vector* v = (struct vector*)hm_pcast(value);
    vector_destroy(v);
    free(v);
}

static void fbx_destroy_connections_index(struct fbx_conns_idx* cidx)
{
    hashmap_iter(&cidx->index, id_free_iter_fn);
    hashmap_destroy(&cidx->index);
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
    /* PreRotation */
    if (rot_active)
        *transform = mat4_mul_mat4(*transform, mat4_rotation_euler(-radians(pre_rot[0]),
                                                                   -radians(pre_rot[1]),
                                                                   -radians(pre_rot[2])));
    /* Lcl Rotation */
    *transform = mat4_mul_mat4(*transform, mat4_rotation_euler(radians(r[0]), radians(r[1]), radians(r[2])));
    /* Lcl Scaling */
    *transform = mat4_mul_mat4(*transform, mat4_scale(vec3_new(s[0], s[1], s[2])));
}

/* Searches for a Model node with the given id */
static struct fbx_record* fbx_find_model_node(struct fbx_record* objs, int64_t id)
{
    const char* mdl_node_name = "Model";
    struct fbx_record* mdl = fbx_find_subrecord_with_name(objs, mdl_node_name);
    while (mdl) {
        if (mdl->properties[0].data.l == id)
            return mdl;
        /* Process next model node */
        mdl = fbx_find_sibling_with_name(mdl, mdl_node_name);
    }
    return mdl;
}

static int fbx_read_transform(struct fbx_record* objs, struct fbx_conns_idx* cidx, int64_t mdl_id, mat4* out)
{
    /* List with subsequent model node id's until we reach parent */
    int64_t cur_id = mdl_id;
    struct vector chain;
    vector_init(&chain, sizeof(int64_t));
    vector_append(&chain, &cur_id);
    while(cur_id) {
        /* Loop through parents */
        int found_parent_id = 0;
        struct vector* par_list = hm_pcast(*hashmap_get(&cidx->index, cur_id));
        for (size_t i = 0; i < par_list->size; ++i) {
            /* Check if parent id is a model id */
            int64_t cpid = *(int64_t*)vector_at(par_list, i);
            struct fbx_record* mdl = fbx_find_model_node(objs, cpid);
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
        struct fbx_record* mdl_node = fbx_find_model_node(objs, id);
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
        hm_ptr* r = hashmap_get(&cidx->index, mat_id);
        if (r) {
            struct vector* par_list = (struct vector*)hm_pcast(*r);
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
static struct model* fbx_read_model(struct fbx_record* obj, struct fbx_conns_idx* cidx)
{
    /* Map that maps internal material ids to ours */
    struct hashmap mat_map;
    hashmap_init(&mat_map, mat_id_hash, mat_id_eql);

    /* Gather model data */
    struct model* model = model_new();
    struct fbx_record* geom = fbx_find_subrecord_with_name(obj, "Geometry");
    while (geom) {
        /* Get model node corresponding to current geometry node */
        int64_t model_node_id = fbx_get_first_connection_id(cidx, geom->properties[0].data.l);
        struct fbx_record* mdl_node = fbx_find_model_node(obj, model_node_id);
        /* Create a list with the material ids */
        struct vector mat_ids;
        vector_init(&mat_ids, sizeof(int64_t));
        fbx_find_materials_for_model(obj, cidx, model_node_id, &mat_ids);

        /* A single geometry node can be multiple meshes, due to non uniform materials.
         * Param indice_offset is filled with -1 if there are no more data to process
         * in current geom node, or with a value that must be passed to subsequent
         * calls of the fbx_read_mesh function to gather next meshes. */
        int indice_offset = 0;
        int mat_idx = 0;
        do {
            struct mesh* nm = fbx_read_mesh(geom, &indice_offset, &mat_idx);
            if (nm) {
                /* Append new mesh */
                model->num_meshes++;
                model->meshes = realloc(model->meshes, model->num_meshes * sizeof(struct mesh*));
                model->meshes[model->num_meshes - 1] = nm;
                /* Check if a transform matrix is available and transform if appropriate */
                if (mdl_node) {
                    mat4 transform;
                    int has_transform = fbx_read_transform(obj, cidx, model_node_id, &transform);
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
static int fbx_joint_count(struct fbx_record* objs)
{
    const char* mdl_node_name = "Model";
    struct fbx_record* mdl = fbx_find_subrecord_with_name(objs, mdl_node_name);
    int count = 0;
    while (mdl) {
        const char* type = mdl->properties[2].data.str;
        if (strncmp("LimbNode", type, 8) == 0)
            ++count;
        /* Process next model node */
        mdl = fbx_find_sibling_with_name(mdl, mdl_node_name);
    }
    return count;
}

static int fbx_joint_parent_index(struct fbx_record* objs, struct fbx_conns_idx* cidx, int64_t child_id)
{
    /* Get parent connection id */
    hm_ptr* p = hashmap_get(&cidx->index, child_id);
    int64_t par_id = -1;
    int par_ofs = -1;
    if (p) {
        struct vector* par_list = (struct vector*)hm_pcast(*p);
        for (size_t i = 0; i < par_list->size; ++i) {
            /* Check if current parent id is a joint */
            int64_t cpid = *(int64_t*)vector_at(par_list, i);
            struct fbx_record* mdl = fbx_find_model_node(objs, cpid);
            if (mdl) {
                const char* type = mdl->properties[2].data.str;
                if (strncmp("LimbNode", type, 8) == 0) {
                    par_id = cpid;
                    break;
                }
            }
        }
    }

    if (par_id != -1) {
        /* Get offset of the parent model node */
        par_ofs = 0;
        const char* mdl_node_name = "Model";
        struct fbx_record* mdl = fbx_find_subrecord_with_name(objs, mdl_node_name);
        while (mdl) {
            int64_t cid = mdl->properties[0].data.l;
            if (cid == par_id)
                break;
            const char* type = mdl->properties[2].data.str;
            if (strncmp("LimbNode", type, 8) == 0)
                ++par_ofs;
            /* Process next model node */
            mdl = fbx_find_sibling_with_name(mdl, mdl_node_name);
        }
    }

    return par_ofs;
}

static struct skeleton* fbx_read_skeleton(struct fbx_record* objs, struct fbx_conns_idx* cidx)
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

    /* Iterate "LimbNode" marked Model nodes */
    const char* mdl_node_name = "Model";
    struct fbx_record* mdl = fbx_find_subrecord_with_name(objs, mdl_node_name);
    int cur_joint_idx = 0;
    while (mdl) {
        const char* type = mdl->properties[2].data.str;
        if (strncmp("LimbNode", type, 8) == 0) {
            /* Copy joint name */
            const char* name = mdl->properties[1].data.str;
            size_t name_sz = strlen(name) * sizeof(char);
            skel->joint_names[cur_joint_idx] = malloc(name_sz + 1);
            memcpy(skel->joint_names[cur_joint_idx], name, name_sz);
            *(skel->joint_names[cur_joint_idx] + name_sz) = 0;
            /* Local Transforms */
            float s[3] = {1.0f, 1.0f, 1.0f}, r[3] = {0.0f, 0.0f, 0.0f}, t[3] = {0.0f, 0.0f, 0.0f};
            /* Pre/Post rotations */
            int rot_active = 0; float pre_rot[3] = {0.0f, 0.0f, 0.0f};
            fbx_read_local_transform(mdl, t, r, s, &rot_active, pre_rot);
            /* Copy joint data */
            struct joint* j = skel->rest_pose->joints + cur_joint_idx;
            memcpy(j->position, t, 3 * sizeof(float));
            /* TODO: Find anorthodox rotation composition */
            float fr[3] = { 0.0f, 0.0f, 0.0f };
            fr[0] += r[1];
            fr[1] += r[0];
            fr[2] += r[2];
            if (rot_active) {
                fr[0] += pre_rot[0];
                fr[1] += pre_rot[1];
                fr[2] += pre_rot[2];
            }
            quat q = quat_from_euler(vec3_new(radians(fr[0]),
                                              radians(fr[1]),
                                              radians(fr[2])));
            memcpy(j->rotation, &q, 4 * sizeof(float));
            memcpy(j->scaling, &s, 3 * sizeof(float));
            /* Set joint parent */
            int par_idx = fbx_joint_parent_index(objs, cidx, mdl->properties[0].data.l);
            j->parent = par_idx == -1 ? 0 : skel->rest_pose->joints + par_idx;

            /*
            printf("Found bone: %s [S(%.2f,%.2f,%.2f),R(%.2f,%.2f,%.2f),PR(%.2f,%.2f,%.2f)]\n",
                    name, s[0], s[1], s[2], r[0], r[1], r[2], pre_rot[0], pre_rot[1], pre_rot[2]);
             */
            ++cur_joint_idx;
        }
        /* Process next model node */
        mdl = fbx_find_sibling_with_name(mdl, mdl_node_name);
    }

    return skel;
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

    /* Build connections index */
    struct fbx_record* conns = fbx_find_subrecord_with_name(fbx.root, "Connections");
    struct fbx_conns_idx cidx;
    fbx_build_connections_index(conns, &cidx);

    /* Gather model data from parsed tree  */
    struct fbx_record* objs = fbx_find_subrecord_with_name(fbx.root, "Objects");
    struct model* m = fbx_read_model(objs, &cidx);

    /* Gather skeleton data */
    m->skeleton = fbx_read_skeleton(objs, &cidx);

    /* Free connections index */
    fbx_destroy_connections_index(&cidx);
    /* Free tree */
    fbx_record_destroy(r);
    return m;
}
