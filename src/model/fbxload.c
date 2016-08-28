#include "assets/model/model.h"
#include "fbxfile.h"
#define _DEBUG
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <hashmap.h>
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

static size_t vertex_hash(void* key)
{
    struct vertex* v = (struct vertex*) key;
    return (size_t)(v->position[0]
                  * v->position[1]
                  * v->position[2]
                  * v->normal[0]
                  * v->normal[1]
                  * v->normal[2]);
}

static int vertex_eql(void* k1, void* k2)
{
    return memcmp(k1, k2, sizeof(struct vertex)) == 0; /* Compare obj vertex index triplets */
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

static struct mesh* fbx_read_mesh(struct fbx_record* geom)
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

    /* Create mesh */
    struct mesh* mesh = mesh_new();
    size_t vu_sz = fbx_pt_unit_size(verts->type);
    mesh->num_verts = 0;
    mesh->num_indices = 0;
    int stored_indices = indices->length / fbx_pt_unit_size(indices->type);

    /* Allocate top limit */
    mesh->vertices = realloc(mesh->vertices, stored_indices * sizeof(struct vertex));
    mesh->indices = realloc(mesh->indices, stored_indices * 2 * sizeof(uint32_t));
    memset(mesh->vertices, 0, stored_indices * sizeof(struct vertex));

    /* Used to find and reuse indices of already stored vertices */
    struct hashmap stored_vertices;
    hashmap_init(&stored_vertices, vertex_hash, vertex_eql);

    /* Populate mesh */
    int fc = 0; /* Counter of vertices in running face */
    for (int i = 0; i < stored_indices; ++i) {
        /* NOTE!
         * Negative array values in the positions' indices array exist
         * to indicate the last index of a polygon.
         * To find the actual indice value we must negate it
         * and substract 1 from that value */
        int32_t pos_ind = indices->data.ip[i];
        if (pos_ind < 0)
            pos_ind = -1 * pos_ind - 1;
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
        void* stored_indice = hashmap_get(&stored_vertices, &tv);
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
            hashmap_put(&stored_vertices, mesh->vertices + nidx, (void*)(uintptr_t)nidx);
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

    hashmap_destroy(&stored_vertices);
    return mesh;
}

/* Fetches connection id for the given key */
static int64_t fbx_get_connection_id(struct fbx_record* connections, int64_t key)
{
    int64_t id = -1;
    struct fbx_record* c = connections->subrecords;
    while (c) {
        if (c->properties[1].data.l == key) {
            id = c->properties[2].data.l;
            break;
        }
        c = c->next;
    }
    return id;
}

/* Reads vec3 from Properties70 subrecord */
static void fbx_read_transform_vec(struct fbx_record* r, float* v)
{
    for (int i = 0; i < 3; ++i)
        v[i] = r->properties[4 + i].data.d;
}

/* Returns a positive value if transform data where found and the given matrix was filled */
static int fbx_read_transform(struct fbx_record* mdl, mat4* transform)
{
    struct fbx_record* transform_rec = fbx_find_subrecord_with_name(mdl, "Properties70");
    struct fbx_record* p = transform_rec->subrecords;

    int has_transform = 0;
    float s[3] = {1.0f, 1.0f, 1.0f}, r[3] = {0.0f, 0.0f, 0.0f}, t[3] = {0.0f, 0.0f, 0.0f};

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

        p = p->next;
    }

    if (has_transform) {
        /*
        printf("Found transform! \n\\t Scaling: %f %f %f\n\\t Rotation: %f %f %f\n\\t Translation: %f %f %f\n",
            s[0], s[1], s[2], r[0], r[1], r[2], t[0], t[1], t[2]);
        */
        *transform = mat4_inverse(
            mat4_mul_mat4(
                mat4_mul_mat4(
                    mat4_rotation_euler(radians(r[0]), radians(r[1]), radians(r[2])),
                    mat4_scale(vec3_new(s[0], s[1], s[2]))
                ),
                mat4_translation(vec3_new(t[0], t[1], t[2]))
            )
        );
    }
    return has_transform;
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

static struct model* fbx_read_model(struct fbx_record* obj, struct fbx_record* conns)
{
    struct model* model = model_new();
    struct fbx_record* geom = fbx_find_subrecord_with_name(obj, "Geometry");
    while (geom) {
        struct mesh* nm = fbx_read_mesh(geom);
        if (nm) {
            model->num_meshes++;
            model->meshes = realloc(model->meshes, model->num_meshes * sizeof(struct mesh*));
            model->meshes[model->num_meshes - 1] = nm;
            /* Check if a transform matrix is available */
            int64_t model_node_id = fbx_get_connection_id(conns, geom->properties[0].data.l);
            struct fbx_record* mdl_node = fbx_find_model_node(obj, model_node_id);
            if (mdl_node) {
                mat4 transform;
                if (fbx_read_transform(mdl_node, &transform)) {
                    fbx_transform_vertices(nm, transform);
                }
            }
        }
        /* Process next mesh */
        geom = fbx_find_sibling_with_name(geom, "Geometry");
    }
    return model;
}

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

    /* Gather model data from parsed tree  */
    struct fbx_record* objs = fbx_find_subrecord_with_name(fbx.root, "Objects");
    struct fbx_record* conns = fbx_find_subrecord_with_name(fbx.root, "Connections");
    struct model* m = fbx_read_model(objs, conns);

    /* Free tree */
    fbx_record_destroy(r);
    return m;
}
