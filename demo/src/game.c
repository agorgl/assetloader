#include "game.h"
#include <stdlib.h>
#include <string.h>
#include "window.h"
#include <glad/glad.h>
#include "shader.h"
#include "static_data.h"
#include <linalgb.h>
#include <stdio.h>
#include <assets/assetload.h>

static void on_key(struct window* wnd, int key, int scancode, int action, int mods)
{
    (void)key; (void)scancode; (void)mods;
    struct game_context* ctx = get_userdata(wnd);
    if (action == 0)
        *(ctx->should_terminate) = 1;
}

static void upload_model_geom_data(struct game_context* ctx)
{
    /* Parse obj */
    struct model* m = model_from_file("ext/cube.obj");
    printf("Num meshes: %d\n", m->num_meshes);
    printf("Num materials: %d\n", m->num_materials);

    /* Allocate handle memory */
    ctx->model.num_meshes = m->num_meshes;
    ctx->model.meshes = malloc(m->num_meshes * sizeof(struct mesh_handle));

    unsigned int total_verts = 0;
    unsigned int total_indices = 0;
    for (unsigned int i = 0; i < ctx->model.num_meshes; ++i) {
        struct mesh* mesh = m->meshes[i];
        struct mesh_handle* mh = ctx->model.meshes + i;
        printf("Using material: %d\n", mesh->mat_index);

        /* Create vao */
        glGenVertexArrays(1, &mh->vao);
        glBindVertexArray(mh->vao);

        /* Create vertex data vbo */
        glGenBuffers(1, &mh->vbo);
        glBindBuffer(GL_ARRAY_BUFFER, mh->vbo);
        glBufferData(GL_ARRAY_BUFFER,
                mesh->num_verts * sizeof(struct vertex),
                mesh->vertices,
                GL_STATIC_DRAW);

        GLuint pos_attrib = 0;
        glEnableVertexAttribArray(pos_attrib);
        glVertexAttribPointer(pos_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (GLvoid*)offsetof(struct vertex, position));
        GLuint uv_attrib = 1;
        glEnableVertexAttribArray(uv_attrib);
        glVertexAttribPointer(uv_attrib, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (GLvoid*)offsetof(struct vertex, uvs));

        /* Create indice ebo */
        glGenBuffers(1, &mh->ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mh->ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                mesh->num_indices * sizeof(GLuint),
                mesh->indices,
                GL_STATIC_DRAW);
        mh->indice_count = mesh->num_indices;
        total_verts += mesh->num_verts;
        total_indices += mesh->num_indices;
    }

    /* Print some info */
    printf("Num vertices: %d\n", total_verts);
    printf("Num indices: %d\n", total_indices);

    /* Free model data */
    model_delete(m);
}

void game_init(struct game_context* ctx)
{
    /* Create window */
    const char* title = "AssetLoader";
    int width = 800, height = 600, mode = 0;
    ctx->wnd = create_window(title, width, height, mode);

    /* Associate context to be accessed from callback functions */
    set_userdata(ctx->wnd, ctx);

    /* Set event callbacks */
    struct window_callbacks wnd_callbacks;
    memset(&wnd_callbacks, 0, sizeof(struct window_callbacks));
    wnd_callbacks.key_cb = on_key;
    set_callbacks(ctx->wnd, &wnd_callbacks);

    /* Initialize game state data */
    ctx->rotation = 0.0f;

    /* Load geometry data from file into the GPU */
    upload_model_geom_data(ctx);

    /* Load shaders */
    ctx->vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(ctx->vs, 1, &VS_SRC, 0);
    glCompileShader(ctx->vs);
    gl_check_last_compile_error(ctx->vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &FS_SRC, 0);
    glCompileShader(fs);
    gl_check_last_compile_error(fs);

    /* Create program */
    ctx->prog = glCreateProgram();
    glAttachShader(ctx->prog, ctx->vs);
    glAttachShader(ctx->prog, fs);
    glLinkProgram(ctx->prog);
    gl_check_last_link_error(ctx->prog);

    /* Create sample texture */
    glGenTextures(1, &ctx->diff_tex);
    glBindTexture(GL_TEXTURE_2D, ctx->diff_tex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    /*
    struct pix { GLubyte r, g, b, a; };
    struct pix tex_data[4];
    memset(tex_data, 0, sizeof(tex_data));
    tex_data[0].r = 100;
    tex_data[0].b = 50;
    tex_data[3].b = 255;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data);
    */

    /*
    struct image* im = image_from_file("ext/mahogany_wood.jpg");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, im->width, im->height, 0, GL_RGB, GL_UNSIGNED_BYTE, im->data);
    */
    struct image* im = image_from_file("ext/floor.tga");
    glTexImage2D(
        GL_TEXTURE_2D, 0,
        im->channels == 4 ? GL_RGBA : GL_RGB,
        im->width, im->height, 0,
        im->channels == 4 ? GL_RGBA : GL_RGB,
        GL_UNSIGNED_BYTE,
        im->data);
    image_delete(im);
}

void game_update(void* userdata, float dt)
{
    struct game_context* ctx = userdata;
    /* Process input events */
    poll_events(ctx->wnd);
    /* Update game state */
    ctx->rotation_prev = ctx->rotation;
    ctx->rotation += dt * 0.001f;
}

void game_render(void* userdata, float interpolation)
{
    struct game_context* ctx = userdata;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, 800, 600);
    glEnable(GL_DEPTH_TEST);

    glUseProgram(ctx->prog);
    /* Create MVP matrix */
    float rotation_interpolated = ctx->rotation + (ctx->rotation - ctx->rotation_prev) * interpolation;
    mat4 model = mat4_rotation_euler(0.0f, rotation_interpolated, 0.0f);
    /*
    float scale = 0.1;
    model = mat4_mul_mat4(model, mat4_scale(vec3_new(scale, scale, scale)));
     */
    mat4 view = mat4_view_look_at(
        vec3_new(0.6f, 1.0f, 2.0f),
        vec3_zero(),
        vec3_new(0.0f, 1.0f, 0.0f));
    mat4 proj = mat4_perspective(radians(45.0f), 0.1f, 300.0f, 1.0f / (800.0f / 600.0f));
    mat4 mvp = mat4_mul_mat4(mat4_mul_mat4(proj, view), model);
    GLuint mvp_loc = glGetUniformLocation(ctx->prog, "MVP");
    mvp = mat4_transpose(mvp);
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, (GLfloat*)&mvp);

    /* Render */
    glUseProgram(ctx->prog);
    GLuint img_loc = glGetUniformLocation(ctx->prog, "diffTex");
    glUniform1i(img_loc, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ctx->diff_tex);

    /* Render mesh by mesh */
    for (unsigned int i = 0; i < ctx->model.num_meshes; ++i) {
        struct mesh_handle* mh = ctx->model.meshes + i;
        glBindVertexArray(mh->vao);
        glBindBuffer(GL_ARRAY_BUFFER, mh->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mh->ebo);
        glDrawElements(GL_TRIANGLES, mh->indice_count, GL_UNSIGNED_INT, (void*)0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    glUseProgram(0);

    /* Show rendered contents from the backbuffer */
    swap_buffers(ctx->wnd);
}

void game_shutdown(struct game_context* ctx)
{
    /* Free GPU resources */
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    for (unsigned int i = 0; i < ctx->model.num_meshes; ++i) {
        struct mesh_handle* mh = ctx->model.meshes + i;
        glDeleteBuffers(1, &mh->ebo);
        glDeleteBuffers(1, &mh->uvs);
        glDeleteBuffers(1, &mh->vbo);
        glDeleteVertexArrays(1, &mh->vao);
    }
    free(ctx->model.meshes);

    glDeleteTextures(1, &ctx->diff_tex);
    glDeleteShader(ctx->fs);
    glDeleteShader(ctx->vs);
    glDeleteProgram(ctx->prog);

    /* Close window */
    destroy_window(ctx->wnd);
}
