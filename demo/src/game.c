#include "game.h"
#include <stdlib.h>
#include <string.h>
#include "window.h"
#include <glad/glad.h>
#include "shader.h"
#include "static_data.h"
#include <stdio.h>
#include <assets/assetload.h>

static void on_key(struct window* wnd, int key, int scancode, int action, int mods)
{
    (void)key; (void)scancode; (void)mods;
    struct game_context* ctx = window_get_userdata(wnd);
    if (action == 0)
        *(ctx->should_terminate) = 1;
}

static void upload_model_geom_data(const char* filename, struct model_handle* model)
{
    /* Parse obj */
    struct model* m = model_from_file(filename);
    printf("Num meshes: %d\n", m->num_meshes);
    printf("Num materials: %d\n", m->num_materials);

    /* Allocate handle memory */
    model->num_meshes = m->num_meshes;
    model->meshes = malloc(m->num_meshes * sizeof(struct mesh_handle));

    unsigned int total_verts = 0;
    unsigned int total_indices = 0;
    for (unsigned int i = 0; i < model->num_meshes; ++i) {
        struct mesh* mesh = m->meshes[i];
        struct mesh_handle* mh = model->meshes + i;
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

static unsigned int upload_texture(const char* filename)
{
    unsigned int id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    struct image* im = image_from_file(filename);
    glTexImage2D(
        GL_TEXTURE_2D, 0,
        im->channels == 4 ? GL_RGBA : GL_RGB,
        im->width, im->height, 0,
        im->channels == 4 ? GL_RGBA : GL_RGB,
        GL_UNSIGNED_BYTE,
        im->data);
    image_delete(im);
    return id;
}

static void setup_data(struct game_context* ctx)
{
    struct game_object go;
    float scale = 1.0f;
    vector_init(&ctx->gobjects, sizeof(struct game_object));
    /* Cube */
    upload_model_geom_data("ext/cube.obj", &go.model);
    go.diff_tex = upload_texture("ext/floor.tga");
    go.transform = mat4_translation(vec3_new(0.0f, 0.1f, 0.0f));
    vector_append(&ctx->gobjects, &go);
    /* Cube2 */
    /*
    upload_model_geom_data("ext/cube.fbx", &go.model);
    go.diff_tex = upload_texture("ext/Bark2.tif");
    go.transform = mat4_translation(vec3_new(0.8f, 0.0f, 0.0f));
    vector_append(&ctx->gobjects, &go);
    */
    /* Podium */
    upload_model_geom_data("ext/models/podium/podium.obj", &go.model);
    go.diff_tex = upload_texture("ext/models/podium/podium.png");
    scale = 0.08;
    go.transform = mat4_mul_mat4(
        mat4_translation(vec3_new(0.0f, -0.5f, 0.0f)),
        mat4_scale(vec3_new(scale, scale, scale)));
    vector_append(&ctx->gobjects, &go);
}

void game_init(struct game_context* ctx)
{
    /* Create window */
    const char* title = "demo";
    int width = 800, height = 600, mode = 0;
    ctx->wnd = window_create(title, width, height, mode);

    /* Associate context to be accessed from callback functions */
    window_set_userdata(ctx->wnd, ctx);

    /* Set event callbacks */
    struct window_callbacks wnd_callbacks;
    memset(&wnd_callbacks, 0, sizeof(struct window_callbacks));
    wnd_callbacks.key_cb = on_key;
    window_set_callbacks(ctx->wnd, &wnd_callbacks);

    /* Initialize game state data */
    ctx->rotation = 0.0f;

    /* Load data from files into the GPU */
    setup_data(ctx);

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
}

void game_update(void* userdata, float dt)
{
    struct game_context* ctx = userdata;
    /* Process input events */
    window_poll_events(ctx->wnd);
    /* Update game state */
    ctx->rotation_prev = ctx->rotation;
    ctx->rotation += dt * 0.000001f;
}

void game_render(void* userdata, float interpolation)
{
    struct game_context* ctx = userdata;
    float rotation_interpolated = ctx->rotation + (ctx->rotation - ctx->rotation_prev) * interpolation;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, 800, 600);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(ctx->prog);

    /* Create view and projection matrices */
    mat4 view = mat4_view_look_at(
        vec3_new(0.6f, 1.0f, 2.5f),  /* Position */
        vec3_zero(),                 /* Target */
        vec3_new(0.0f, 1.0f, 0.0f)); /* Up */
    mat4 proj = mat4_perspective(radians(45.0f), 0.1f, 300.0f, 1.0f / (800.0f / 600.0f));

    /* Render */
    glUseProgram(ctx->prog);
    GLuint img_loc = glGetUniformLocation(ctx->prog, "diffTex");
    glUniform1i(img_loc, 0);

    /* Loop through objects */
    for (unsigned int i = 0; i < ctx->gobjects.size; ++i) {
        /* Setup game object to be rendered */
        struct game_object* gobj = vector_at(&ctx->gobjects, i);
        struct model_handle* mdlh = &gobj->model;
        /* Transform */
        mat4 model = gobj->transform;
        mat4 rot = mat4_rotation_euler(0.0f, rotation_interpolated, 0.0f);
        model = mat4_mul_mat4(model, rot);
        /*
        float scale = 0.1;
        model = mat4_mul_mat4(model, mat4_scale(vec3_new(scale, scale, scale)));
         */
        /* Upload MVP matrix */
        mat4 mvp = mat4_mul_mat4(mat4_mul_mat4(proj, view), model);
        GLuint mvp_loc = glGetUniformLocation(ctx->prog, "MVP");
        mvp = mat4_transpose(mvp);
        glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, (GLfloat*)&mvp);
        /* Set diffuse texture */
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gobj->diff_tex);

        /* Render mesh by mesh */
        for (unsigned int i = 0; i < mdlh->num_meshes; ++i) {
            struct mesh_handle* mh = mdlh->meshes + i;
            glBindVertexArray(mh->vao);
            glBindBuffer(GL_ARRAY_BUFFER, mh->vbo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mh->ebo);
            glDrawElements(GL_TRIANGLES, mh->indice_count, GL_UNSIGNED_INT, (void*)0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }
    }

    glUseProgram(0);

    /* Show rendered contents from the backbuffer */
    window_swap_buffers(ctx->wnd);
}

void game_shutdown(struct game_context* ctx)
{
    /* Free GPU resources */
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    for (unsigned int i = 0; i < ctx->gobjects.size; ++i) {
        struct game_object* gobj = vector_at(&ctx->gobjects, i);
        /* Free geometry */
        for (unsigned int i = 0; i < gobj->model.num_meshes; ++i) {
            struct mesh_handle* mh = gobj->model.meshes + i;
            glDeleteBuffers(1, &mh->ebo);
            glDeleteBuffers(1, &mh->uvs);
            glDeleteBuffers(1, &mh->vbo);
            glDeleteVertexArrays(1, &mh->vao);
        }
        free(gobj->model.meshes);
        /* Free texture */
        glDeleteTextures(1, &gobj->diff_tex);
    }
    vector_destroy(&ctx->gobjects);

    glDeleteShader(ctx->fs);
    glDeleteShader(ctx->vs);
    glDeleteProgram(ctx->prog);

    /* Close window */
    window_destroy(ctx->wnd);
}
