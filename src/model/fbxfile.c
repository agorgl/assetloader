#include "fbxfile.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <zlib.h>

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

size_t fbx_pt_unit_size(enum fbx_pt pt)
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

void fbx_record_print(struct fbx_record* rec, int depth)
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

void fbx_record_pretty_print(struct fbx_record* rec, int depth)
{
    /* Print record name */
    printf("%*s%s: {\n", depth * 4, " ", rec->name);
    /* Print record properties */
    for (unsigned int i = 0; i < rec->num_props; ++i) {
        struct fbx_property prop = rec->properties[i];
        printf("%*s(%c) %u bytes: ",
                (depth + 1) * 4, " ", prop.code, prop.length);
        fbx_property_print(prop);
        printf(",\n");
    }
    /* Recurse */
    struct fbx_record* r = rec->subrecords;
    while (r) {
        fbx_record_pretty_print(r, depth + 1);
        r = r->next;
    }
    printf("%*s}\n", depth * 4, " ");
}

struct fbx_record* fbx_find_subrecord_with_name(struct fbx_record* rec, const char* name)
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

struct fbx_record* fbx_find_sibling_with_name(struct fbx_record* rec, const char* name)
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
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
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
int fbx_read_header(struct parser_state* ps, struct fbx_file* fbx)
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
                prop.enc_arr = 0;
            } else {
                prop.length = fbx_pt_size(pt, arr_len);;
                prop.data.p = malloc(prop.length);
                fbx_array_decompress(ps->cur, clen, prop.data.p, prop.length);
                prop.enc_arr = 1;
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

static void fbx_property_destroy(struct fbx_property* p)
{
    if ((p->type == fbx_pt_float_arr
         || p->type == fbx_pt_double_arr
         || p->type == fbx_pt_long_arr
         || p->type == fbx_pt_int_arr
         || p->type == fbx_pt_bool_arr) && p->enc_arr) {
        free(p->data.p);
    }
}

void fbx_record_destroy(struct fbx_record* fbxr)
{
    struct fbx_record* n = fbxr->subrecords;
    while (n) {
        struct fbx_record* next = n->next;
        fbx_record_destroy(n);
        n = next;
    }
    for (uint32_t i = 0; i < fbxr->num_props; ++i)
        fbx_property_destroy(fbxr->properties + i);
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

struct fbx_record* fbx_read_root_record(struct parser_state* ps)
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
