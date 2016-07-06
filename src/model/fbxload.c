#include "assets/model/model.h"
#define _DEBUG
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

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

static size_t fbx_pt_size(enum fbx_pt pt, uint32_t arr_len)
{
    switch(pt) {
        case fbx_pt_short:      return sizeof(int16_t);
        case fbx_pt_bool:       return sizeof(uint8_t);
        case fbx_pt_int:        return sizeof(int32_t);
        case fbx_pt_float:      return sizeof(float);
        case fbx_pt_double:     return sizeof(double);
        case fbx_pt_long:       return sizeof(int64_t);
        case fbx_pt_float_arr:  return sizeof(float) * arr_len;
        case fbx_pt_double_arr: return sizeof(double) * arr_len;
        case fbx_pt_long_arr:   return sizeof(int64_t) * arr_len;
        case fbx_pt_int_arr:    return sizeof(int32_t) * arr_len;
        case fbx_pt_bool_arr:   return sizeof(uint8_t) * arr_len;
        case fbx_pt_string:     return sizeof(char) * arr_len;
        case fbx_pt_raw:        return arr_len;
        default: return 0;
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
                prop.data.p = 0; // TODO
                prop.length = fbx_pt_size(pt, arr_len);
            } else {
                prop.data.p = 0; // TODO
                prop.length = clen;
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

// Path to geometries: Objects/???

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

    struct fbx_record* r = fbx_read_root_record(&ps);
    fbx.root = r;
    //fbx_record_print(r, 0);
    fbx_record_destroy(r);

    return 0;
}
