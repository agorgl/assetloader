#include "assets/model/model.h"
#include <stdlib.h>
#include <string.h>
#include <vector.h>
#include <hashmap.h>

/* Custom version of standard isspace, to avoid MSVC checks */
static int is_space(const char c)
{
    return c == ' '
        || c == '\t'
        || c == '\n'
        || c == '\v'
        || c == '\f'
        || c == '\r';
}

/* Token is not null terminated with token_sz being the token length */
static float parse_float(const char* token, size_t token_sz)
{
    (void) token_sz; /* TODO: Use functions with bound checking */
    return (float) atof(token);
}

/* Token is not null terminated with token_sz being the token length */
static int32_t parse_int(const char* token, size_t token_sz)
{
    (void) token_sz; /* TODO: Use functions with bound checking */
    return atoi(token);
}

/* Parses: i, i/j/k, i//k, i/j */
static void parse_face_triple(const char* token, size_t token_sz, int32_t* triple)
{
    const char* tok_end = token + token_sz;
    const char* cur = token;
    const char* wend = token;

    for (int32_t i = 0; i < 3; ++i) {
        wend = cur;
        /* Advance word end operator */
        while (wend < tok_end && *wend != '/')
            ++wend;
        /* Parse integer */
        if (wend - cur > 0)
            *(triple + i) = parse_int(cur, wend - cur);
        /* Advance current ptr */
        cur = wend + 1;
    }
}

static void parse_space_sep_entry(const char* token, size_t token_sz, float* arr, size_t count)
{
    const char* cur = token;
    const char* line_end = token + token_sz;

    for (size_t i = 0; i < count; ++i) {
        /* Skip whitespace to next word */
        while (cur < line_end && is_space(*cur))
            ++cur;
        /* Check if eol reached */
        if (cur == line_end)
            break;
        arr[i] = parse_float((const char*)cur, line_end - cur);
        /* Skip parsed word */
        while (cur < line_end && !is_space(*cur))
            ++cur;
    }
}

/* Holds allocation info about current parsing state */
struct parser_state {
    struct vector positions; /* Array of mesh positions */
    struct vector normals;   /* Array of mesh normals */
    struct vector texcoords; /* Array of mesh texture coordinates */
    struct vector faces;     /* Array of face indice triplets */
    struct hashmap found_materials; /* Hashmap of already found materials */
    int cur_mat_idx; /* Current material index set to meshes when being flushed */
};

static size_t indice_hash(hm_ptr key)
{
    int32_t* k = (int32_t*) hm_pcast(key);
    return (size_t) k[0] ^ k[1] ^ k[2];
}

static int indice_eql(hm_ptr k1, hm_ptr k2)
{
    return memcmp(hm_pcast(k1), hm_pcast(k2), 3 * sizeof(int32_t)) == 0; /* Compare obj vertex index triplets */
}

static struct mesh* mesh_from_parser_state(struct parser_state* ps)
{
    /* Create mesh */
    struct mesh* mesh = mesh_new();
    mesh->num_verts = 0;
    mesh->num_indices = 0;
    /* Allocate top limit */
    mesh->vertices = realloc(mesh->vertices, ps->faces.size * 3 * sizeof(struct vertex));
    mesh->indices = realloc(mesh->indices, ps->faces.size * 3 * sizeof(uint32_t));

    /* Used to find and reuse indices of already stored vertices */
    struct hashmap stored_vertices;
    hashmap_init(&stored_vertices, indice_hash, indice_eql);

    /* Iterate through triples */
    for (size_t i = 0; i < ps->faces.size; ++i) {
        /* Iterate as a triangle face */
        for (size_t j = 0; j < 3; ++j) {
            /* Vertex indices */
            int32_t* vi = (int32_t*)vector_at(&ps->faces, i) + 3 * j;

            /* Check if current triple has already been stored */
            hm_ptr* indice = hashmap_get(&stored_vertices, hm_cast(vi));
            ++mesh->num_indices;
            if (indice) {
                mesh->indices[mesh->num_indices - 1] = *((uint32_t*)indice);
            } else {
                ++mesh->num_verts;
                /* Current working vertex */
                struct vertex* v = mesh->vertices + mesh->num_verts - 1;

                /* Store position data */
                int32_t pos_index = vi[0];
                if (pos_index != 0) {
                    pos_index = pos_index > 0 ? pos_index - 1 : (int32_t)(ps->positions.size + pos_index);
                    memcpy(v->position, vector_at(&ps->positions, pos_index), 3 * sizeof(float));
                }

                /* Store texture data */
                int32_t tex_index = vi[1];
                if (tex_index != 0) {
                    tex_index = tex_index > 0 ? tex_index - 1 : (int32_t)(ps->texcoords.size + tex_index);
                    memcpy(v->uvs, vector_at(&ps->texcoords, tex_index), 2 * sizeof(float));
                }

                /* Store normal data */
                int32_t nm_index = vi[2];
                if (nm_index != 0) {
                    nm_index = nm_index > 0 ? nm_index - 1 : (int32_t)(ps->normals.size + nm_index);
                    memcpy(v->normal, vector_at(&ps->normals, nm_index), 3 * sizeof(float));
                }

                /* Store new vertice index into indices array */
                mesh->indices[mesh->num_indices - 1] = mesh->num_verts - 1;
                /* Store face triple ptr to lookup table */
                hashmap_put(&stored_vertices, hm_cast(vi), hm_cast(mesh->num_verts - 1));
            }
        }
    }
    hashmap_destroy(&stored_vertices);
    mesh->mat_index = ps->cur_mat_idx;
    return mesh;
}

static void flush_mesh(struct parser_state* ps, struct model* m)
{
    /* Create new mesh entry from parser state */
    m->num_meshes++;
    m->meshes = realloc(m->meshes, m->num_meshes * sizeof(struct mesh*));
    m->meshes[m->num_meshes - 1] = mesh_from_parser_state(ps);

    /* Clear gathered faces */
    vector_destroy(&ps->faces);
    vector_init(&ps->faces, 9 * sizeof(int32_t));
}

static size_t found_materials_hash(hm_ptr key)
{
    const char* str = hm_pcast(key);
    unsigned long hash = 5381;
    int c;
    while ((c = *str++) != 0)
        hash = (hash * 33 + c);
    return hash;
}

static int found_materials_eql(hm_ptr k1, hm_ptr k2)
{
    return strcmp((const char*)hm_pcast(k1), (const char*)hm_pcast(k2)) == 0;
}

static void found_materials_iter(hm_ptr key, hm_ptr value)
{
    (void) value;
    free(hm_pcast(key));
}

/* line is null terminated buffer and line_sz is the buffer length with the null terminator */
static void parse_line(struct parser_state* ps, struct model* m, const unsigned char* line, size_t line_sz)
{
    /* Limit to avoid buffer overreads */
    const unsigned char* line_end = line + line_sz;
    /* Running pointer */
    const unsigned char* cur = line;

    /* Skip leading whitespace */
    while (cur < line_end && is_space(*cur))
        ++cur;

    /* Check if comment or empty line and skip them */
    if (cur == line_end - 1 || *cur == '#')
        return;

    /* Find terminator of first word (first whitespace) */
    const unsigned char* wend = cur;
    while(wend < line_end && !is_space(*wend))
        ++wend;
    size_t next_word_sz = wend - cur;

    if (strncmp("v", (const char*)cur, next_word_sz) == 0) {
        /* Vertex */
        /*
         * v x y z (w)
         * with w being optional and with default value 1.0
         */
        ++cur;

        /* Parse entry data */
        float vvv[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        parse_space_sep_entry((const char*)cur, line_end - cur, vvv, 4);

        /* Store parsed positions */
        vector_append(&ps->positions, vvv);
    } else if (strncmp("vn", (const char*) cur, next_word_sz) == 0) {
        /* Vertex normal */
        /*
         * vn i j k
         */
        cur += 2;

        /* Parse entry data */
        float vn[3] = { 0.0f, 0.0f, 0.0f };
        parse_space_sep_entry((const char*)cur, line_end - cur, vn, 3);

        /* Store data */
        vector_append(&ps->normals, vn);
    } else if (strncmp("vt", (const char*) cur, next_word_sz) == 0) {
        /* Texture coordinates */
        /*
         * vt u (v) (w)
         * with v, w being optional with default values of 0
         */
        cur += 2;

        /* Parse entry data */
        float vt[3] = { 0.0f, 0.0f, 0.0f };
        parse_space_sep_entry((const char*)cur, line_end - cur, vt, 3);

        /* Store data */
        vector_append(&ps->texcoords, vt);
    } else if (strncmp("f", (const char*) cur, next_word_sz) == 0) {
        /* Vertex index */
        /*
         * f v/vt/vn v/vt/vn v/vt/vn
         * with vt and vn being optional
         * Negative reference numbers for v can be used
         */
        ++cur;
        int32_t f[12];
        memset(f, 0, sizeof(f));
        int i = 0;
        while (cur != line_end) {
            /* Skip whitespace to next word */
            while (cur < line_end && is_space(*cur))
                ++cur;
            /* Check if eol reached */
            if (cur == line_end - 1)
                break;
            /* Find end of current triple */
            const unsigned char* tend = cur;
            while (tend < line_end && !is_space(*tend))
                ++tend;
            /* Parse the triple */
            parse_face_triple((const char*)cur, tend - cur, f + i * 3);
            /* Skip parsed word */
            while (cur < line_end && !is_space(*cur))
                ++cur;
            /* Quad or more need additional face per triple */
            if (i >= 3) {
                int32_t g[9];
                memcpy(g + 0, f + i * 3, 3 * sizeof(float));
                memcpy(g + 3, f + 0, 3 * sizeof(float));
                memcpy(g + 6, f + (i - 1) * 3, 3 * sizeof(float));
                vector_append(&ps->faces, g);
            }
            ++i;
        }

        /* Store data */
        vector_append(&ps->faces, f);
    } else if (strncmp("o", (const char*) cur, next_word_sz) == 0) {
        if (ps->faces.size > 0)
            flush_mesh(ps, m);
    } else if (strncmp("g", (const char*) cur, next_word_sz) == 0) {
        if (ps->faces.size > 0)
            flush_mesh(ps, m);
    } else if (strncmp("usemtl", (const char*) cur, next_word_sz) == 0) {
        /* Flush, as a new material is comming into use */
        if (ps->faces.size > 0)
            flush_mesh(ps, m);
        /* Skip space after keyword */
        cur += 6;
        /* Skip whitespace to next word */
        while (cur < line_end && is_space(*cur))
            ++cur;
        /* Find the end of the material name */
        const unsigned char* we = cur;
        while (we < line_end && !is_space(*we))
            ++we;
        /* Copy material name to new buffer */
        char* material = calloc(we - cur + 1, sizeof(char));
        memcpy(material, cur, (we - cur) * sizeof(char));
        /* Check if material is already found */
        hm_ptr* fmat = hashmap_get(&ps->found_materials, hm_cast(material));
        if (fmat) {
            free(material);
            ps->cur_mat_idx = *(int*)fmat;
        }
        else {
            ++m->num_materials;
            ps->cur_mat_idx = m->num_materials - 1;
            hashmap_put(&ps->found_materials, hm_cast(material), hm_cast(m->num_materials - 1));
        }
    }
}

struct model* model_from_obj(const unsigned char* data, size_t sz)
{
    /* Pointer to the current reading position */
    unsigned char* cur = (unsigned char*) data;

    /* Create model object */
    struct model* m = model_new();
    /* Create initial parsing state object */
    struct parser_state ps;
    memset(&ps, 0, sizeof(struct parser_state));

    /* Initialize parser state vectors */
    vector_init(&ps.positions, 3 * sizeof(float));
    vector_init(&ps.normals, 3 * sizeof(float));
    vector_init(&ps.texcoords, 3 * sizeof(float));
    vector_init(&ps.faces, 9 * sizeof(int32_t));
    hashmap_init(&ps.found_materials, found_materials_hash, found_materials_eql);

    /* Read line by line */
    while (cur <= data + sz) {
        /* Set end of line point */
        unsigned char* eol = cur;
        while ((eol < data + sz) && *eol != '\n')
            ++eol;

        /* Process line */
        size_t line_sz = eol - cur + 1;
        unsigned char* line_buf = malloc(line_sz);
        memcpy(line_buf, cur, eol - cur);
        *(line_buf + line_sz - 1) = '\0'; /* Overwrite \n with \0 */
        parse_line(&ps, m, line_buf, line_sz);
        free(line_buf);

        /* Advance current position */
        cur = eol + 1;
    }

    /* Flush final mesh */
    flush_mesh(&ps, m);

    /* Deallocate parser state arrays */
    hashmap_iter(&ps.found_materials, found_materials_iter);
    hashmap_destroy(&ps.found_materials);
    vector_destroy(&ps.positions);
    vector_destroy(&ps.normals);
    vector_destroy(&ps.texcoords);
    vector_destroy(&ps.faces);

    return m;
}
