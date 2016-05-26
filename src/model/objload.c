#include "assets/model/model.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <vector.h>
#include <hashmap.h>

/* Token is not null terminated with token_sz being the token length */
static float parse_float(const char* token, size_t token_sz)
{
    (void) token_sz; /* TODO: Use functions with bound checking */
    return atof(token);
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
        while (*wend != '/' && wend < tok_end)
            ++wend;
        /* Parse integer */
        if (wend - cur > 1)
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
        while (isspace(*cur) && cur < line_end)
            ++cur;
        /* Check if eol reached */
        if (cur == line_end)
            break;
        arr[i] = parse_float((const char*)cur, line_end - cur);
        /* Skip parsed word */
        while (!isspace(*cur) && cur < line_end)
            ++cur;
    }
}

/* Holds allocation info about current parsing state */
struct parser_state {
    struct vector positions; /* Array of mesh positions */
    struct vector normals;   /* Array of mesh normals */
    struct vector texcoords; /* Array of mesh texture coordinates */
    struct vector faces;     /* Array of face indice triplets */
};

/* line is null terminated buffer and line_sz is the buffer length with the null terminator */
static void parse_line(struct parser_state* ps, struct model* m, const unsigned char* line, size_t line_sz)
{
    /* Limit to avoid buffer overreads */
    const unsigned char* line_end = line + line_sz;
    /* Running pointer */
    const unsigned char* cur = line;

    /* Skip leading whitespace */
    while (isspace(*cur) && cur < line_end)
        ++cur;

    /* Check if comment or empty line and skip them */
    if (*cur == '#' || cur == line_end - 1)
        return;

    /* Find terminator of first word (first whitespace) */
    const unsigned char* wend = cur;
    while(!isspace(*wend) && wend < line_end)
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
        int32_t f[9];
        memset(f, 0, sizeof(f));
        for (int i = 0; i < 3; ++i) {
            /* Skip whitespace to next word */
            while (isspace(*cur) && cur < line_end)
                ++cur;
            /* Check if eol reached */
            if (cur == line_end)
                break;
            parse_face_triple((const char*)cur, line_end - cur, f + i * 3);
            /* Skip parsed word */
            while (!isspace(*cur) && cur < line_end)
                ++cur;
        }

        /* Store data */
        vector_append(&ps->faces, f);
    }
}

size_t indice_hash(void* key)
{
    int32_t* k = (int32_t*) key;
    return (size_t) k[0] ^ k[1] ^ k[2];
}

int indice_eql(void* k1, void* k2)
{
    return memcmp(k1, k2, 3 * sizeof(int32_t)) == 0; /* Compare obj vertex index triplets */
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
            void* indice = hashmap_get(&stored_vertices, vi);
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
                    pos_index = pos_index > 0 ? pos_index + 1 : (int)(ps->positions.size + pos_index);
                    memcpy(v->position, vector_at(&ps->positions, + pos_index), 3 * sizeof(float));
                }

                /* Store texture data */
                int32_t tex_index = vi[1];
                if (tex_index != 0) {
                    tex_index = tex_index > 0 ? tex_index + 1 : (int)(ps->texcoords.size + tex_index);
                    memcpy(v->uvs, vector_at(&ps->texcoords, tex_index), 2 * sizeof(float));
                }

                /* Store normal data */
                int32_t nm_index = vi[2];
                if (nm_index != 0) {
                    nm_index = nm_index > 0 ? nm_index + 1 : (int)(ps->normals.size + nm_index);
                    memcpy(v->normal, vector_at(&ps->normals, nm_index), 3 * sizeof(float));
                }

                /* Store new vertice index into indices array */
                mesh->indices[mesh->num_indices - 1] = mesh->num_verts - 1;
                /* Store face triple ptr to lookup table */
                hashmap_put(&stored_vertices, vi, (void*)(mesh->num_verts - 1));
            }
        }
    }
    hashmap_destroy(&stored_vertices);
    return mesh;
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

    /* Read line by line */
    while (cur <= data + sz) {
        /* Set end of line point */
        unsigned char* eol = cur;
        while (*eol != '\n' && (eol < data + sz))
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

    /* Create new mesh entry from parser state */
    m->num_meshes++;
    m->meshes = realloc(m->meshes, m->num_meshes * sizeof(struct mesh*));
    m->meshes[0] = mesh_from_parser_state(&ps);

    /*
    for (int i = 0; i < m->meshes[0]->num_verts; ++i) {
        float* vvv = m->meshes[0]->vertices[i].position;
        printf("%.1f %.1f %.1f\n", vvv[0], vvv[1], vvv[2]);
    }
    for (int i = 0; i < m->meshes[0]->num_indices; ++i) {
        uint32_t ind = m->meshes[0]->indices[i];
        printf("%d\n", ind);
    }

    for (size_t i = 0; i < ps.positions.size; ++i) {
        float* vvv = vector_at(&ps.positions, i);
        printf("Position: %.1f %.1f %.1f\n", vvv[0], vvv[1], vvv[2]);
    }
    for (size_t i = 0; i < ps.normals.size; ++i) {
        float* vn = vector_at(&ps.normals, i);
        printf("Normal: %.1f %.1f %.1f\n", vn[0], vn[1], vn[2]);
    }
    for (size_t i = 0; i < ps.texcoords.size; ++i) {
        float* vt = vector_at(&ps.texcoords, i);
        printf("Texcoord: %.1f %.1f %.1f\n", vt[0], vt[1], vt[2]);
    }
    printf("Num vertices: %d\n", m->meshes[0]->num_verts);
    printf("Num indices: %d\n", m->meshes[0]->num_indices);
    */

    /* Deallocate parser state arrays */
    vector_destroy(&ps.positions);
    vector_destroy(&ps.normals);
    vector_destroy(&ps.texcoords);
    vector_destroy(&ps.faces);

    return m;
}
