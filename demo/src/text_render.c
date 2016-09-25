#include "text_render.h"
#include <string.h>
#include <assets/font/font.h>
#include <glad/glad.h>
#include <linalgb.h>

static const char* vshader = "\
#version 330 core                          \n\
layout (location = 0) in vec3 vertex;      \n\
layout (location = 1) in vec2 tex_coord;   \n\
layout (location = 2) in vec4 color;       \n\
                                           \n\
out vec4 vsCol;                            \n\
out vec2 vsTexCoord;                       \n\
uniform mat4 mvp;                          \n\
                                           \n\
void main()                                \n\
{                                          \n\
    vsTexCoord = tex_coord.xy;             \n\
    vsCol = color;                         \n\
    gl_Position = mvp * vec4(vertex, 1.0); \n\
}";

static const char* fshader = "\
#version 330 core                                  \n\
in vec4 vsCol;                                     \n\
in vec2 vsTexCoord;                                \n\
                                                   \n\
out vec4 color;                                    \n\
uniform sampler2D texture;                         \n\
                                                   \n\
void main()                                        \n\
{                                                  \n\
    float a = texture2D(texture, vsTexCoord.xy).r; \n\
    color = vec4(vsCol.rgb, vsCol.a*a);            \n\
}";

struct text_renderer {
    /* GPU resource identifiers */
    unsigned int shader;
    /* Glyph bucket */
    texture_atlas_t* atlas;
    /* Font handle */
    texture_font_t* font;
};

typedef struct {
    float x, y, z;    /* position */
    float s, t;       /* uv */
    float r, g, b, a; /* color */
} vertex_t;

static unsigned int text_renderer_shader_build(const char* vss, const char* fss)
{
    /* Load shaders */
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vss, 0);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fss, 0);
    glCompileShader(fs);

    /* Create program */
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(fs);
    glDeleteShader(vs);
    return prog;
}

static void text_renderer_add_text(struct vector* tot_verts,
                                   struct vector* tot_indcs,
                                   texture_font_t* font,
                                   const char* text,
                                   vec4* color,
                                   vec2* pen)
{
    float r = color->x, g = color->y, b = color->z, a = color->w;
    /* Iterate through each character */
    for (size_t i = 0; i < strlen(text); ++i) {
        /* Retrieve glyph from the given font corresponding to the current char */
        texture_glyph_t* glyph = texture_font_get_glyph(font, text + i);
        /* Skip non existing glyphs */
        if (glyph == 0)
            continue;
        /* Calculate glyph render triangles */
        float kerning = i == 0 ? 0.0f : texture_glyph_get_kerning(glyph, text + i - 1);
        pen->x += kerning;
        int x0 = (int)(pen->x + glyph->offset_x);
        int y0 = (int)(pen->y + glyph->offset_y);
        int x1 = (int)(x0 + glyph->width);
        int y1 = (int)(y0 - glyph->height);
        float s0 = glyph->s0;
        float t0 = glyph->t0;
        float s1 = glyph->s1;
        float t1 = glyph->t1;
        vertex_t vertices[4] =
            {{x0, y0, 0, s0, t0, r, g, b, a},
             {x0, y1, 0, s0, t1, r, g, b, a},
             {x1, y1, 0, s1, t1, r, g, b, a},
             {x1, y0, 0, s1, t0, r, g, b, a}};
        /* Construct indice list */
        GLuint indices[6] = {0, 1, 2, 0, 2, 3};
        for (int j = 0; j < 6; ++j)
            indices[j] += tot_verts->size * 4;
        /* Append vertices and indices to buffers */
        vector_append(tot_verts, vertices);
        vector_append(tot_indcs, indices);
        /* Advance cursor */
        pen->x += glyph->advance_x;
    }
}

text_renderer_t* text_render_init()
{
    struct text_renderer* tr = malloc(sizeof(struct  text_renderer));
    /* Create the atlas */
    tr->atlas = texture_atlas_new(512, 512, 1);
    /* Load the font */
    int font_sz = 27;
    const char* filename = "ext/fonts/vera.ttf";
    tr->font = texture_font_new_from_file(tr->atlas, font_sz, filename);
    /* Preload most glyphs */
    texture_font_load_glyphs(tr->font, "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz_0123456789");
    /* Upload atlas to the GPU */
    glGenTextures(1, &tr->atlas->id);
    glBindTexture(GL_TEXTURE_2D, tr->atlas->id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,
         0,
         GL_RED,
         tr->atlas->width,
         tr->atlas->height,
         0,
         GL_RED,
         GL_UNSIGNED_BYTE,
         tr->atlas->data
    );
    /* Build rendering shader */
    tr->shader = text_renderer_shader_build(vshader, fshader);
    return tr;
}

void text_render_print(text_renderer_t* tr, const char* text, vec2 pos, vec4 color)
{
    /*
     * Setup phase
     */
    struct vector tot_verts;
    struct vector tot_indcs;
    vector_init(&tot_verts, 4 * sizeof(vertex_t));
    vector_init(&tot_indcs, 6 * sizeof(GLuint));

    /* Gather vertices and indices according to the given text */
    text_renderer_add_text(&tot_verts, &tot_indcs, tr->font, text, &color, &pos);

    /* Upload Vertices */
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(vertex_t) * tot_verts.size, tot_verts.data, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Upload Indices */
    GLuint ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(GLuint) * tot_indcs.size, tot_indcs.data, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    /* Setup vertex attributes */
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    GLuint pos_attrib = 0;
    glEnableVertexAttribArray(pos_attrib);
    glVertexAttribPointer(pos_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_t), (GLvoid*)offsetof(vertex_t, x));
    GLuint uv_attrib = 1;
    glEnableVertexAttribArray(uv_attrib);
    glVertexAttribPointer(uv_attrib, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_t), (GLvoid*)offsetof(vertex_t, s));
    GLuint col_attrib = 2;
    glEnableVertexAttribArray(col_attrib);
    glVertexAttribPointer(col_attrib, 4, GL_FLOAT, GL_FALSE, sizeof(vertex_t), (GLvoid*)offsetof(vertex_t, r));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    /*
     * Render phase
     */
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(tr->shader);
    glUniform1i(glGetUniformLocation(tr->shader, "texture"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tr->atlas->id);

    mat4 model = mat4_id();
    mat4 view = mat4_id();
    mat4 proj = mat4_orthographic(0, 800, 0, 600, -1, 1);
    mat4 mvp = mat4_mul_mat4(mat4_mul_mat4(proj, view), model);
    mvp = mat4_transpose(mvp);
    glUniformMatrix4fv(glGetUniformLocation(tr->shader, "mvp"), 1, GL_FALSE, (void*)&mvp);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glDrawElements(GL_TRIANGLES, tot_indcs.size * 6, GL_UNSIGNED_INT, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    /*
     * Free resources
     */
    vector_destroy(&tot_verts);
    vector_destroy(&tot_indcs);
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

void text_render_shutdown(text_renderer_t* tr)
{
    glUseProgram(0);
    glDeleteProgram(tr->shader);
    glDeleteTextures(1, &tr->atlas->id);
    tr->atlas->id = 0;
    texture_font_delete(tr->font);
    texture_atlas_delete(tr->atlas);
    free(tr);
}
