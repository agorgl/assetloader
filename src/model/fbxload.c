#include "assets/model/model.h"
#define _DEBUG
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <zlib.h>
#include <hashmap.h>

/*------------------------------------------*
 * Node Record Format                       |
 *------------------------------------------*
 * 4 bytes           | End Offset           |
 * 4 bytes           | Num Properties       |
 * 4 bytes           | Property List Length |
 * 1 bytes           | Name Length          |
 * Name Length bytes | Name                 |
 * Variable bytes    | Property Data        |
 * Variable bytes    | Nested Records Data  |
 * 13 bytes          | Padding Block        |
 *------------------------------------------*/

/*----------------------------*
 * Property Record Format     |
 *----------------------------*
 * 1 byte         | Type code |
 * Variable bytes | Data      |
 *----------------------------*/

/*------------------------------------*
 * Array Data Format                  |
 *------------------------------------*
 * 4 bytes        | Array Length      |
 * 4 bytes        | Encoding          |
 * 4 bytes        | Compressed Length |
 * Variable bytes | Contents          |
 *------------------------------------*/

/* If Encoding is 0, the Contents is just ArrayLength times the array data type.
 * If Encoding is 1, the Contents is a deflate/zip-compressed buffer of length
 * CompressedLength bytes. The buffer can for example be decoded using zlib. */

/* For String and Raw data types we have 4 bytes of length following
 * with length bytes of their data */

/*-----------------------------------------------------------------
 * Types
 *-----------------------------------------------------------------*/
struct parser_state {
    unsigned char* data;
    unsigned char* cur;
    unsigned char* bufend;
};

/* FBX Property type */
enum fbx_pt {
    /* Primitive types */
    fbx_pt_short,  /* 2 byte signed integer */
    fbx_pt_bool,   /* 1 byte boolean */
    fbx_pt_int,    /* 4 byte signed integer */
    fbx_pt_float,  /* 4 byte single precision IEEE 754 number */
    fbx_pt_double, /* 8 byte double precision IEEE 754 number */
    fbx_pt_long,   /* 8 byte signed integer */
    /* Array types */
    fbx_pt_float_arr,
    fbx_pt_double_arr,
    fbx_pt_long_arr,
    fbx_pt_int_arr,
    fbx_pt_bool_arr,
    /* Other */
    fbx_pt_string, /* Not null terminated, may contain nulls */
    fbx_pt_raw,
    fbx_pt_invalid
};

/* FBX Property */
struct fbx_property {
    char code;
    enum fbx_pt type;
    union {
        uint8_t b;
        int16_t s;
        int32_t i;
        int64_t l;
        float f;
        double d;
        void* p;
        uint8_t* bp;
        int16_t* sp;
        int32_t* ip;
        int64_t* lp;
        float* fp;
        double* dp;
        const char* str;
        unsigned char* raw;
    } data;
    uint32_t length;
};

/* FBX Record */
struct fbx_record {
    char* name;
    struct fbx_property* properties; /* Array of properties */
    uint32_t num_props;
    struct fbx_record* subrecords;   /* Linked list of subrecords */
    struct fbx_record* next;         /* Next record in list */
};

/* FBX File */
struct fbx_file {
    unsigned int version;
    struct fbx_record* root;
};

/*-----------------------------------------------------------------
 * Helpers
 *-----------------------------------------------------------------*/
static enum fbx_pt fbx_code_to_pt(char c)
{
    switch(c) {
        case 'Y': return fbx_pt_short;
        case 'C': return fbx_pt_bool;
        case 'I': return fbx_pt_int;
        case 'F': return fbx_pt_float;
        case 'D': return fbx_pt_double;
        case 'L': return fbx_pt_long;
        case 'f': return fbx_pt_float_arr;
        case 'd': return fbx_pt_double_arr;
        case 'l': return fbx_pt_long_arr;
        case 'i': return fbx_pt_int_arr;
        case 'b': return fbx_pt_bool_arr;
        case 'S': return fbx_pt_string;
        case 'R': return fbx_pt_raw;
        default:  return fbx_pt_invalid;
    }
}

static size_t fbx_pt_unit_size(enum fbx_pt pt)
{
    switch(pt) {
        case fbx_pt_short:      return sizeof(int16_t);
        case fbx_pt_bool:       return sizeof(uint8_t);
        case fbx_pt_int:        return sizeof(int32_t);
        case fbx_pt_float:      return sizeof(float);
        case fbx_pt_double:     return sizeof(double);
        case fbx_pt_long:       return sizeof(int64_t);
        case fbx_pt_float_arr:  return sizeof(float);
        case fbx_pt_double_arr: return sizeof(double);
        case fbx_pt_long_arr:   return sizeof(int64_t);
        case fbx_pt_int_arr:    return sizeof(int32_t);
        case fbx_pt_bool_arr:   return sizeof(uint8_t);
        case fbx_pt_string:     return sizeof(char);
        case fbx_pt_raw:        return 1;
        default: return 0;
    }
}

static size_t fbx_pt_size(enum fbx_pt pt, uint32_t arr_len)
{
    switch(pt) {
        case fbx_pt_float_arr:
        case fbx_pt_double_arr:
        case fbx_pt_long_arr:
        case fbx_pt_int_arr:
        case fbx_pt_bool_arr:
        case fbx_pt_string:
        case fbx_pt_raw:
            return fbx_pt_unit_size(pt) * arr_len;
        default:
            return fbx_pt_unit_size(pt);
    }
}

static const char* fbx_pt_desc(enum fbx_pt pt)
{
    switch(pt) {
        case fbx_pt_short:      return "Int16";
        case fbx_pt_bool:       return "Bool";
        case fbx_pt_int:        return "Int32";
        case fbx_pt_float:      return "Float";
        case fbx_pt_double:     return "Double";
        case fbx_pt_long:       return "Int64";
        case fbx_pt_float_arr:  return "Float Array";
        case fbx_pt_double_arr: return "Double Array";
        case fbx_pt_long_arr:   return "Int64 Array";
        case fbx_pt_int_arr:    return "Int32 Array";
        case fbx_pt_bool_arr:   return "Bool Array";
        case fbx_pt_string:     return "String";
        case fbx_pt_raw:        return "Raw";
        default: return "???";
    }
}

static void fbx_property_print(struct fbx_property prop)
{
    switch(prop.type) {
        case fbx_pt_short:
            printf("%d", prop.data.s);
            break;
        case fbx_pt_bool:
            printf("%s", prop.data.b != 0 ? "true": "false");
            break;
        case fbx_pt_int:
            printf("%d", prop.data.i);
            break;
        case fbx_pt_float:
            printf("%f", prop.data.f);
            break;
        case fbx_pt_double:
            printf("%f", prop.data.d);
            break;
        case fbx_pt_long:
            printf("%d", prop.data.i);
            break;
        case fbx_pt_string:
            printf("%.*s", prop.length, prop.data.str);
            break;
        case fbx_pt_float_arr:
        case fbx_pt_double_arr:
        case fbx_pt_long_arr:
        case fbx_pt_int_arr:
        case fbx_pt_bool_arr:
        case fbx_pt_raw:
            printf("%p", prop.data.p);
            break;
        default:
            printf("???");
            break;
    }
}

static void fbx_record_print(struct fbx_record* rec, int depth)
{
    /* Print record name */
    printf("%*sRecord name: %s\n", depth, " ", rec->name);
    /* Print record properties */
    for (unsigned int i = 0; i < rec->num_props; ++i) {
        struct fbx_property prop = rec->properties[i];
        printf("%*sProperty type: %s (%c) %u bytes: ",
                depth, " ", fbx_pt_desc(prop.type), prop.code, prop.length);
        fbx_property_print(prop);
        printf("\n");
    }
    /* Recurse */
    struct fbx_record* r = rec->subrecords;
    while (r) {
        fbx_record_print(r, depth + 1);
        r = r->next;
    }
}

static struct fbx_record* fbx_find_subrecord_with_name(struct fbx_record* rec, const char* name)
{
    struct fbx_record* r = rec->subrecords;
    while (r) {
        if (strcmp(r->name, name) == 0) {
            return r;
        }
        r = r->next;
    }
    return 0;
}

static struct fbx_record* fbx_find_sibling_with_name(struct fbx_record* rec, const char* name)
{
    struct fbx_record* r = rec->next;
    while (r) {
        if (strcmp(r->name, name) == 0) {
            return r;
        }
        r = r->next;
    }
    return 0;
}

/*-----------------------------------------------------------------
 * Compression
 *-----------------------------------------------------------------*/
static int fbx_array_decompress(const void* src, int srclen, void* dst, int dstlen)
{
    int err = -1, ret = -1;
    z_stream strm  = {};
    strm.total_in  = strm.avail_in  = srclen;
    strm.total_out = strm.avail_out = dstlen;
    strm.next_in   = (Bytef*) src;
    strm.next_out  = (Bytef*) dst;

    strm.zalloc = Z_NULL;
    strm.zfree  = Z_NULL;
    strm.opaque = Z_NULL;

    err = inflateInit(&strm);
    if (err == Z_OK) {
        err = inflate(&strm, Z_FINISH);
        assert(err != Z_STREAM_ERROR);
        if (err == Z_STREAM_END)
            ret = strm.total_out;
    }

    inflateEnd(&strm);
    return ret; /* -1 or len of input */
}

/*-----------------------------------------------------------------
 * Parsing
 *-----------------------------------------------------------------*/
#ifdef _DEBUG
#define iterfw(bytes) assert((ps->cur += bytes) < ps->bufend)
#else
#define iterfw(bytes) ps->cur += bytes
#endif

static const char* fbx_header = "Kaydara FBX Binary  \x00\x1A\x00";
static int fbx_read_header(struct parser_state* ps, struct fbx_file* fbx)
{
    if (memcmp(fbx_header, ps->cur, 23) == 0) {
        iterfw(23);
        fbx->version = *((uint32_t*)ps->cur);
        iterfw(sizeof(uint32_t));
        return 1;
    }
    return 0;
}

static struct fbx_property fbx_read_property(struct parser_state* ps)
{
    /* Read type */
    char tcode = *((char*)ps->cur);
    iterfw(sizeof(char));

    /* Read data */
    enum fbx_pt pt = fbx_code_to_pt(tcode);
    struct fbx_property prop;
    memset(&prop, 0, sizeof(struct fbx_property));
    prop.type = pt;
    prop.code = tcode;

    switch(pt)
    {
        case fbx_pt_short: {
            prop.data.s = *((uint16_t*)ps->cur);
            prop.length = fbx_pt_size(pt, 0);
            break;
        }
        case fbx_pt_bool: {
            prop.data.b = *((uint8_t*)ps->cur);
            prop.length = fbx_pt_size(pt, 0);
            break;
        }
        case fbx_pt_int: {
            prop.data.i = *((uint32_t*)ps->cur);
            prop.length = fbx_pt_size(pt, 0);
            break;
        }
        case fbx_pt_float: {
            prop.data.f = *((float*)ps->cur);
            prop.length = fbx_pt_size(pt, 0);
            break;
        }
        case fbx_pt_double: {
            prop.data.d = *((double*)ps->cur);
            prop.length = fbx_pt_size(pt, 0);
            break;
        }
        case fbx_pt_long: {
            prop.data.l = *((uint64_t*)ps->cur);
            prop.length = fbx_pt_size(pt, 0);
            break;
        }
        case fbx_pt_float_arr:
        case fbx_pt_double_arr:
        case fbx_pt_long_arr:
        case fbx_pt_int_arr:
        case fbx_pt_bool_arr: {
            uint32_t arr_len = *((uint32_t*)ps->cur);
            iterfw(sizeof(uint32_t));
            uint32_t enc = *((uint32_t*)ps->cur);
            iterfw(sizeof(uint32_t));
            uint32_t clen = *((uint32_t*)ps->cur);
            iterfw(sizeof(uint32_t));
            if (!enc) {
                prop.data.p = ps->cur;
                prop.length = fbx_pt_size(pt, arr_len);
            } else {
                prop.length = fbx_pt_size(pt, arr_len);;
                prop.data.p = malloc(prop.length); // TODO: Free below
                fbx_array_decompress(ps->cur, clen, prop.data.p, prop.length);
                /* Early return (different iterfw size) */
                iterfw(clen);
                return prop;
            }
            break;
        }
        case fbx_pt_string:
        case fbx_pt_raw: {
            uint32_t len = *((uint32_t*)ps->cur);
            iterfw(sizeof(uint32_t));
            prop.length = fbx_pt_size(pt, len);
            prop.data.p = (char*)ps->cur;
            break;
        }
        default:
            assert(0 && "Invalid property type");
            break;
    }
    /* Forward iterator by the length of the parsed property */
    iterfw(prop.length);
    return prop;
}

static void fbx_record_init(struct fbx_record* fbxr)
{
    memset(fbxr, 0, sizeof(struct fbx_record));
}

static void fbx_record_destroy(struct fbx_record* fbxr)
{
    struct fbx_record* n = fbxr->subrecords;
    while (n) {
        struct fbx_record* next = n->next;
        fbx_record_destroy(n);
        n = next;
    }
    free(fbxr->properties);
    free(fbxr->name);
    free(fbxr);
}

/* Padding block at end of each record */
static unsigned char fbx_record_padding_block[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static struct fbx_record* fbx_read_record(struct parser_state* ps)
{
    /* End Offset */
    uint32_t end_off = *((uint32_t*)ps->cur);
    iterfw(sizeof(uint32_t));
    if (end_off == 0)
        return 0;

    /* Allocate record */
    struct fbx_record* rec = malloc(sizeof(struct fbx_record));
    fbx_record_init(rec);

    /* Properties */
    uint32_t num_props = *((uint32_t*)ps->cur); /* Num Properties */
    iterfw(sizeof(uint32_t));
    uint32_t prop_list_len = *((uint32_t*)ps->cur); /* Property list length */
    (void) prop_list_len;
    iterfw(sizeof(uint32_t));

    /* Name */
    uint8_t name_len = *((uint8_t*)ps->cur);
    iterfw(sizeof(uint8_t));
    char* name = (char*) ps->cur;
    iterfw(name_len);

    rec->name = malloc(name_len * sizeof(char) + 1);
    memcpy(rec->name, name, name_len);
    rec->name[name_len] = 0;

    /* Read properties */
    rec->num_props = num_props;
    rec->properties = malloc(num_props * sizeof(struct fbx_property));
    for (unsigned int i = 0; i < num_props; ++i) {
        struct fbx_property prop = fbx_read_property(ps);
        rec->properties[i] = prop;
    }

    /* If space remains till next entry it probably is a nested record */
    if (ps->cur < ps->data + end_off) {
        while (ps->cur < ps->data + end_off - 13) {
            struct fbx_record* sr = fbx_read_record(ps);
            sr->next = rec->subrecords;
            rec->subrecords = sr;
        }
        assert(memcmp(fbx_record_padding_block, ps->cur, 13) == 0 && "Padding block mismatch");
        ps->cur += 13;
    }

    /* Check for future possible errors */
    assert(ps->cur == ps->data + end_off && "Record end offset not reached");

    /* Fw to next node */
    ps->cur = (unsigned char*) ps->data + end_off;
    return rec;
}

static struct fbx_record* fbx_read_root_record(struct parser_state* ps)
{
    struct fbx_record* r = malloc(sizeof(struct fbx_record));
    fbx_record_init(r);
    r->name = malloc(5 * sizeof(char));
    memcpy(r->name, "Root", 5);
    struct fbx_record* sr = 0;
    do {
        sr = fbx_read_record(ps);
        if (!sr)
            break;
        sr->next = r->subrecords;
        r->subrecords = sr;
    } while (sr);
    return r;
}

/* Copy double to float array */
static void fbx_copy_dtfa(float* dst, double* src, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        dst[i] = src[i];
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

static struct mesh* fbx_read_mesh(struct fbx_record* geom)
{
    /* Check if geometry node contains any vertices */
    struct fbx_record* verts_nod = fbx_find_subrecord_with_name(geom, "Vertices");
    if (!verts_nod)
        return 0;
    struct fbx_property* verts = verts_nod->properties;

    /* Find data nodes */
    struct fbx_property* indices = fbx_find_subrecord_with_name(
        geom,
        "PolygonVertexIndex"
    )->properties;
    struct fbx_property* norms = fbx_find_subrecord_with_name(
        fbx_find_subrecord_with_name(geom, "LayerElementNormal"),
        "Normals"
    )->properties;
    struct fbx_property* tangents = fbx_find_subrecord_with_name(
        fbx_find_subrecord_with_name(geom, "LayerElementTangent"),
        "Tangents"
    )->properties;
    struct fbx_property* binormals = fbx_find_subrecord_with_name(
        fbx_find_subrecord_with_name(geom, "LayerElementBinormal"),
        "Binormals"
    )->properties;
    struct fbx_property* uvs = fbx_find_subrecord_with_name(
        fbx_find_subrecord_with_name(geom, "LayerElementUV"),
        "UV"
    )->properties;
    struct fbx_property* uv_idxs = fbx_find_subrecord_with_name(
        fbx_find_subrecord_with_name(geom, "LayerElementUV"),
        "UVIndex"
    )->properties;

    /* Create mesh */
    struct mesh* mesh = mesh_new();
    size_t vert_unit_sz = fbx_pt_unit_size(verts->type);
    mesh->num_verts = 0;
    mesh->num_indices = indices->length / fbx_pt_unit_size(indices->type);

    /* Allocate top limit */
    mesh->vertices = realloc(mesh->vertices, mesh->num_indices * sizeof(struct vertex));
    mesh->indices = realloc(mesh->indices, mesh->num_indices * sizeof(uint32_t));
    memset(mesh->vertices, 0, mesh->num_indices * sizeof(struct vertex));

    /* Used to find and reuse indices of already stored vertices */
    struct hashmap stored_vertices;
    hashmap_init(&stored_vertices, vertex_hash, vertex_eql);

    /* Populate mesh */
    for (int i = 0; i < mesh->num_indices; ++i) {
        /* NOTE!
         * Negative array values in the positions' indices array exist
         * to indicate the last index of a polygon.
         * To find the actual indice value we must negate it
         * and substract 1 from that value */
        int32_t pos_ind = indices->data.ip[i];
        if (pos_ind < 0)
            pos_ind = -1 * pos_ind - 1;
        uint32_t uv_ind = uv_idxs->data.ip[i];
        /* Fill temporary vertex */
        struct vertex tv;
        if (vert_unit_sz != sizeof(float)) {
            fbx_copy_dtfa(tv.position, verts->data.dp + pos_ind * 3, 3);
            fbx_copy_dtfa(tv.normal, norms->data.dp + i * 3, 3);
            fbx_copy_dtfa(tv.tangent, tangents->data.dp + i * 3, 3);
            fbx_copy_dtfa(tv.binormal, binormals->data.dp + i * 3, 3);
            fbx_copy_dtfa(tv.uvs, uvs->data.dp + uv_ind * 2, 2);
        }
        else {
            memcpy(tv.position, verts->data.fp + pos_ind * 3, 3 * sizeof(float));
            memcpy(tv.normal, norms->data.fp + i * 3, 3 * sizeof(float));
            memcpy(tv.tangent, tangents->data.fp + i * 3, 3 * sizeof(float));
            memcpy(tv.binormal, binormals->data.fp + i * 3, 3 * sizeof(float));
            memcpy(tv.uvs, uvs->data.fp + uv_ind * 2, 2 * sizeof(float));
        }
        /* Check if current vertex is already stored */
        void* stored_indice = hashmap_get(&stored_vertices, &tv);
        if (stored_indice) {
            mesh->indices[i] = *(uint32_t*)stored_indice;
        } else {
            ++mesh->num_verts;
            /* Store new vertice */
            uint32_t nidx = mesh->num_verts - 1;
            memcpy(mesh->vertices + nidx, &tv, sizeof(struct vertex));
            /* Set indice */
            mesh->indices[i] = nidx;
            /* Store vertex ptr to lookup table */
            hashmap_put(&stored_vertices, mesh->vertices + nidx, (void*)(uintptr_t)nidx);
        }
    }

    return mesh;
}

static struct model* fbx_read_model(struct fbx_record* obj)
{
    struct model* model = model_new();
    struct fbx_record* geom = fbx_find_subrecord_with_name(obj, "Geometry");
    while (geom) {
        struct mesh* nm = fbx_read_mesh(geom);
        if (nm) {
            model->num_meshes++;
            model->meshes = realloc(model->meshes, model->num_meshes * sizeof(struct mesh*));
            model->meshes[model->num_meshes - 1] = nm;
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

    /* Print debug info */
    /*
    fbx_record_print(
        fbx_find_subrecord_with_name(
            fbx_find_subrecord_with_name(fbx.root, "Objects"),
            "Geometry"),
        3
    );
    */

    /* Gather model data from parsed tree  */
    struct fbx_record* objs = fbx_find_subrecord_with_name(fbx.root, "Objects");
    struct model* m = fbx_read_model(objs);

    /* Free tree */
    fbx_record_destroy(r);
    return m;
}
