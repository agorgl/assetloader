#include "assets/model/model.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <assets/model/postprocess.h>

/*-----------------------------------------------------------------
 * Data iterator
 *-----------------------------------------------------------------*/
struct data_iterator {
    const unsigned char* cur;
    const unsigned char* lim;
};

/* Initialize iterator from base for sz bytes */
#define it_init(it, data, sz) \
    do { it->cur = data; it->lim = data + sz; } while(0)
/* Remaining sz bytes exist */
#define it_rem(it, sz) \
    (it->cur + sz < it->lim)

#ifndef PARSE_SAFE
/* Forward sz bytes */
#define it_fw(it, sz) \
    do { it->cur += sz; } while (0)
/* Forward byte */
#define it_fwb(it) \
    ++(it->cur)
#else
#define it_fw(it, sz) \
    do { assert(it_rem(it, sz)); it_fw(it, sz); } while (0)
#define it_fwb(it) \
    do { assert(it_rem(it, 1)); it_fwb(it); } while (0)
#endif

/* Forward line */
#define it_fwl(it) \
    do { while (*it->cur != '\n') { it_fwb(it); }; it_fwb(it); } while (0)
#define is_wordspace(c) \
    ((c) == ' ' || (c) == '\t' || (c) == '\n')
/* Forward space */
#define it_fws(it) \
    do { while (is_wordspace(*it->cur)) it_fwb(it); } while (0)
/* Forward word */
#define it_fww(it) \
    do { while (!is_wordspace(*it->cur)) it_fwb(it); } while (0)
/* Forward next word */
#define it_fwnw(it) \
    do { it_fww(it); it_fws(it); } while (0)
/* Forward till char */
#define it_fwtc(it, c) \
    do { while (it->cur != c) it_fwb(it) it_fwb(it); } while (0)

/* Count word */
static int it_cntw(struct data_iterator* it)
{
    int cnt = 0;
    while (!is_wordspace(*(it->cur + cnt)))
        ++cnt;
    return cnt;
}

/* Compare word */
#define it_cmpw(it, w) \
    (strncmp(w, (const char*)it->cur, strlen(w)) == 0)

/*-----------------------------------------------------------------
 * Ply
 *-----------------------------------------------------------------*/
enum ply_prop_type {
    /* Name          Type        Number of bytes
     * ----------------------------------------- */
    PLY_CHAR = 0, /* character                 1 */
    PLY_UCHAR,    /* unsigned character        1 */
    PLY_SHORT,    /* short integer             2 */
    PLY_USHORT,   /* unsigned short integer    2 */
    PLY_INT,      /* integer                   4 */
    PLY_UINT,     /* unsigned integer          4 */
    PLY_FLOAT,    /* single-precision float    4 */
    PLY_DOUBLE,   /* double-precision float    8 */
    PLY_UNDEFINED
};

static const char* ply_prop_type_names[] = {
    "char", "uchar", "short", "ushort", "int", "uint", "float", "double"
};

static const size_t ply_prop_type_sizes[] = {
    1, 1, 2, 2, 4, 4, 4, 8
};

struct ply_property {
    const char* name;
    enum ply_prop_type dtype;
    unsigned int is_list;
    enum ply_prop_type lsz_type;
};

struct ply_element {
    const char* name;
    unsigned long nentries;
    struct ply_property* props;
    unsigned long nprops;
};

struct ply_header {
    enum {
        PLY_ASCII,
        PLY_BINARY_LE,
        PLY_BINARY_BE
    } format;
    struct { int maj, min; } ver;
    struct ply_element* elems;
    unsigned long nelems;
};

struct ply_data {
    void** elem_chunks;
    unsigned long nelems;
};

static enum ply_prop_type ply_prop_dtype_read(struct data_iterator* it)
{
    int i = 0;
    for (i = 0; i < PLY_UNDEFINED; ++i) {
        const char* tname = ply_prop_type_names[i];
        size_t tname_sz = strlen(tname);
        if (strncmp(tname, (const char*)it->cur, tname_sz) == 0)
            return i;
    }
    return i;
}

static int ply_prop_read(struct ply_property* pp, struct data_iterator* it)
{
    /* Skip "property" word */
    it_fwnw(it);

    /* Check if list type */
    if (it_cmpw(it, "list")) {
        pp->is_list = 1;
        it_fwnw(it);
    }

    /* Read data type */
    pp->dtype = ply_prop_dtype_read(it);
    it_fwnw(it);

    /* List length dtype */
    if (pp->is_list) {
        pp->lsz_type = ply_prop_dtype_read(it);
        it_fwnw(it);
    } else
        pp->lsz_type = PLY_UNDEFINED;

    /* Read property name */
    int wsz = it_cntw(it);
    pp->name = calloc(1, wsz + 1);
    strncpy((char*)pp->name, (const char*)it->cur, wsz);
    it_fwl(it);
    return 0;
}

static int ply_elem_read(struct ply_element* pe, struct data_iterator* it)
{
    /* Skip "element" word */
    it_fwnw(it);

    /* Read element name */
    int wsz = it_cntw(it);
    pe->name = calloc(1, wsz + 1);
    strncpy((char*)pe->name, (const char*)it->cur, wsz);
    it_fwnw(it);

    /* Read element count */
    pe->nentries = atol((const char*)it->cur);
    it_fwl(it);

    /* Read properties */
    while (it_cmpw(it, "property")) {
        /* Allocate and read property */
        pe->nprops++;
        pe->props = realloc(pe->props, pe->nprops * sizeof(*pe->props));
        struct ply_property* pp = pe->props + (pe->nprops - 1);
        memset(pp, 0, sizeof(*pp));
        ply_prop_read(pp, it);
    }
    return 0;
}

static int ply_header_read(struct ply_header* ph, struct data_iterator* it)
{
    /* Zero out */
    memset(ph, 0, sizeof(*ph));

    /* Read format */
    if (!it_cmpw(it, "format"))
        return 1;
    it_fwnw(it);

    if (it_cmpw(it, "ascii"))
        ph->format = PLY_ASCII;
    else if (it_cmpw(it, "binary_little_endian"))
        ph->format = PLY_BINARY_LE;
    else if (it_cmpw(it, "binary_big_endian"))
        ph->format = PLY_BINARY_BE;
    else
        return 1;
    it_fwnw(it);

    /* Read version */
    ph->ver.maj = atoi((const char*)it->cur);
    it_fwb(it);
    ph->ver.min = atoi((const char*)it->cur);
    it_fwl(it);

    /* Skip comments */
    while (it_cmpw(it, "comment"))
        it_fwl(it);

    /* Read elements */
    while (!it_cmpw(it, "end_header")) {
        if (!it_cmpw(it, "element"))
            return 1;
        /* Allocate and read element */
        ph->nelems++;
        ph->elems = realloc(ph->elems, ph->nelems * sizeof(*ph->elems));
        struct ply_element* pe = ph->elems + (ph->nelems - 1);
        memset(pe, 0, sizeof(*pe));
        ply_elem_read(pe, it);
    }
    it_fwl(it);
    return 0;
}

static int ply_element_entries_are_variable_size(struct ply_element* pe)
{
    for (unsigned long i = 0; i < pe->nprops; ++i)
        if (pe->props[i].is_list)
            return 1;
    return 0;
}

static size_t ply_element_entries_size(struct ply_element* pe)
{
    size_t sz = 0;
    for (unsigned long i = 0; i < pe->nprops; ++i)
        sz += ply_prop_type_sizes[pe->props[i].dtype];
    return sz * pe->nentries;
}

static unsigned long ply_read_list_size(enum ply_prop_type pt, void* data)
{
    unsigned long sz = 0;
    switch (pt) {
        case PLY_CHAR:
        case PLY_UCHAR:
            sz = *(uint8_t*)data;
            break;
        case PLY_SHORT:
        case PLY_USHORT:
            sz = *(uint16_t*)data;
            break;
        case PLY_INT:
        case PLY_UINT:
            sz = *(uint32_t*)data;
            break;
        case PLY_FLOAT:
            sz = *(float*)data;
            break;
        case PLY_DOUBLE:
            sz = *(double*)data;
            break;
        default:
            break;
    }
    return sz;
}

static int ply_data_read(struct ply_data* pd, struct ply_header* ph, struct data_iterator* it)
{
    pd->nelems = ph->nelems;
    pd->elem_chunks = calloc(pd->nelems, sizeof(void*));
    for (unsigned long i = 0; i < ph->nelems; ++i) {
        struct ply_element* pe = ph->elems + i;
        if (ph->format != PLY_ASCII) {
            /* TODO: Take care of endianess */
            pd->elem_chunks[i] = (void*) it->cur;
            if (!ply_element_entries_are_variable_size(pe))
                it_fw(it, ply_element_entries_size(pe));
            else {
                /* Step through entries */
                for (unsigned long j = 0; j < pe->nentries; ++j) {
                    for (unsigned long k = 0; k < pe->nprops; ++k) {
                        struct ply_property* pp = pe->props + k;
                        if (!pp->is_list)
                            it_fw(it, ply_prop_type_sizes[pp->dtype]);
                        else {
                            /* Get variable list size */
                            unsigned long sz = ply_read_list_size(pp->lsz_type, (void*)it->cur);
                            it_fw(it, ply_prop_type_sizes[pp->lsz_type]);
                            /* Forward by the number of list properties we got */
                            it_fw(it, sz * ply_prop_type_sizes[pp->dtype]);
                        }
                    }
                }
            }
        } else {
            assert(0 && "Unimplemented");
        }
    }
    return 0;
}

static void ply_header_free(struct ply_header* ph)
{
    for (unsigned long i = 0; i < ph->nelems; ++i) {
        struct ply_element* pe = ph->elems + i;
        for (unsigned long j = 0; j < pe->nprops; ++j)
            free((void*)pe->props[j].name);
        free((void*)pe->name);
        free(pe->props);
    }
    free(ph->elems);
}

static void ply_data_free(struct ply_data* pd, int from_ascii)
{
    if (from_ascii) {
        for (unsigned long i = 0; i < pd->nelems; ++i) {
            free(pd->elem_chunks[i]);
        }
    }
    free(pd->elem_chunks);
}

static struct mesh* ply_read_mesh(struct ply_header* ph, struct ply_data* pd)
{
    struct mesh* mesh = mesh_new();
    for (unsigned long i = 0; i < ph->nelems; ++i) {
        struct ply_element* pe = ph->elems + i;
        void* elem_chunk = pd->elem_chunks[i];
        if (strcmp(pe->name, "vertex") == 0) {
            /* Vertices */
            mesh->num_verts = pe->nentries;
            mesh->vertices = realloc(mesh->vertices, mesh->num_verts * sizeof(struct vertex));
            memset(mesh->vertices, 0, mesh->num_verts * sizeof(struct vertex));
            /* Speedup */
            int entrysz_varies = ply_element_entries_are_variable_size(pe);
            if (!entrysz_varies) {
                size_t entry_sz = ply_element_entries_size(pe) / pe->nentries;
                size_t xyz_ofs[3] = {0, 0, 0};
                size_t cur_prop_ofs = 0;
                for (unsigned long j = 0; j < pe->nprops; ++j) {
                    struct ply_property* pp = pe->props + j;
                    if (strcmp(pp->name, "x") == 0)
                        xyz_ofs[0] = cur_prop_ofs;
                    else if (strcmp(pp->name, "y") == 0)
                        xyz_ofs[1] = cur_prop_ofs;
                    else if (strcmp(pp->name, "z") == 0)
                        xyz_ofs[2] = cur_prop_ofs;
                    cur_prop_ofs += ply_prop_type_sizes[pp->dtype];
                }
                for (unsigned long j = 0; j < pe->nentries; ++j) {
                    void* entryd = elem_chunk + j * entry_sz;
                    struct vertex* v = mesh->vertices + j;
                    v->position[0] = *(float*)(entryd + xyz_ofs[0]);
                    v->position[1] = *(float*)(entryd + xyz_ofs[1]);
                    v->position[2] = *(float*)(entryd + xyz_ofs[2]);
                }
            } else {

            }
        } else if (strcmp(pe->name, "tristrips") == 0) {
            /* Triangle Strips */
            mesh->num_indices = 0;
            mesh->indices = realloc(mesh->indices, mesh->num_indices * sizeof(uint32_t));
            struct ply_property* ve_prop = 0;
            for (unsigned long j = 0; j < pe->nprops; ++j) {
                struct ply_property* pp = pe->props + j;
                if (strcmp(pp->name, "vertex_indices") == 0)
                    ve_prop = pp;
            }
            void* curd = elem_chunk;
            for (unsigned long j = 0; j < pe->nentries; ++j) {
                /* Entry's list size */
                unsigned long list_sz = ply_read_list_size(ve_prop->lsz_type, curd);
                curd += ply_prop_type_sizes[ve_prop->lsz_type];
                /* List item size */
                unsigned long dsz = ply_prop_type_sizes[ve_prop->dtype];
                /* Grow indice array */
                mesh->indices = realloc(mesh->indices, (3 * (mesh->num_indices + list_sz)) * sizeof(uint32_t));
                /* Store indices */
                int prev[2] = {-1, -1};
                for (unsigned long k = 0; k < list_sz; ++k) {
                    int32_t indice = *(int32_t*)(curd + k * dsz);
                    if (indice == -1) {
                        prev[0] = prev[1] = -1;
                        continue;
                    }
                    if (prev[0] == -1) {
                        prev[0] = indice;
                        continue;
                    }
                    if (prev[1] == -1) {
                        prev[1] = indice;
                        continue;
                    }
                    mesh->indices[mesh->num_indices + 0] = prev[0];
                    mesh->indices[mesh->num_indices + 1] = prev[1];
                    mesh->indices[mesh->num_indices + 2] = indice;
                    mesh->num_indices += 3;
                    prev[0] = prev[1];
                    prev[1] = indice;
                }
                curd += list_sz * dsz;
            }
        } else if (strcmp(pe->name, "face") == 0) {
            /* Faces */
            mesh->num_indices = 0;
            mesh->indices = realloc(mesh->indices, mesh->num_indices * sizeof(uint32_t));
            struct ply_property* ve_prop = 0;
            for (unsigned long j = 0; j < pe->nprops; ++j) {
                struct ply_property* pp = pe->props + j;
                if (strcmp(pp->name, "vertex_indices") == 0)
                    ve_prop = pp;
            }
            void* curd = elem_chunk;
            for (unsigned long j = 0; j < pe->nentries; ++j) {
                /* Entry's list size */
                unsigned long list_sz = ply_read_list_size(ve_prop->lsz_type, curd);
                curd += ply_prop_type_sizes[ve_prop->lsz_type];
                /* List item size */
                unsigned long dsz = ply_prop_type_sizes[ve_prop->dtype];
                /* Grow indice array */
                mesh->indices = realloc(mesh->indices, (mesh->num_indices + list_sz) * sizeof(uint32_t));
                /* Store indices */
                for (unsigned long k = 0; k < list_sz; ++k) {
                    int32_t indice = *(int32_t*)(curd + k * dsz);
                    mesh->indices[mesh->num_indices++] = indice;
                }
                curd += list_sz * dsz;
            }
        }
    }
    return mesh;
}

struct model* model_from_ply(const unsigned char* data, size_t sz)
{
    /* Data iterator */
    struct data_iterator it;
    it_init((&it), data, sz);

    /* Check magic */
    if (it_rem((&it), 4) && strncmp((const char*)data, "ply\n", 4) != 0) {
        printf("Invalid ply header!\n");
        return 0;
    }
    it_fwl((&it));

    /* Read header and data */
    struct ply_header ply_header;
    ply_header_read(&ply_header, &it);
    struct ply_data ply_data;
    ply_data_read(&ply_data, &ply_header, &it);

    /* Read mesh */
    struct mesh* mesh = ply_read_mesh(&ply_header, &ply_data);
    mesh_generate_normals(mesh);
    mesh->mgroup_idx = 0;

    /* Setup model struct */
    struct model* m = model_new();
    m->num_meshes++;
    m->meshes = realloc(m->meshes, m->num_meshes * sizeof(struct mesh*));
    m->meshes[m->num_meshes - 1] = mesh;
    m->num_materials = 1;

    /* Create and append the root mesh group */
    struct mesh_group* mgroup = mesh_group_new();
    mgroup->name = strdup("root_group");
    m->num_mesh_groups++;
    m->mesh_groups = realloc(m->mesh_groups, m->num_mesh_groups * sizeof(struct mesh_group*));
    m->mesh_groups[m->num_mesh_groups - 1] = mgroup;

    /* Free header and data */
    ply_data_free(&ply_data, ply_header.format == PLY_ASCII);
    ply_header_free(&ply_header);

    return m;
}
