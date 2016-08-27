#include "game.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "window.h"
#include "input.h"
#include <glad/glad.h>
#include "shader.h"
#include "static_data.h"
#include <stdio.h>
#include <assets/assetload.h>

static APIENTRY void gl_debug_proc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* user_param)
{
    (void) source;
    (void) id;
    (void) severity;
    (void) length;
    (void) user_param;

    if (type == GL_DEBUG_TYPE_ERROR) {
        fprintf(stderr, "%s", message);
        exit(1);
    }
}

static void on_key(struct window* wnd, int key, int scancode, int action, int mods)
{
    (void)scancode; (void)mods;
    struct game_context* ctx = window_get_userdata(wnd);
    if (action == KEY_ACTION_RELEASE && key == KEY_ESCAPE)
        *(ctx->should_terminate) = 1;
    else if (action == KEY_ACTION_RELEASE && key == KEY_SPACE) {
        /* Cycle through shown objects skipping podium that is first */
        ctx->cur_obj = ctx->cur_obj < ctx->gobjects.size - 1 ? ctx->cur_obj + 1 : 1;
    }
    else if (action == KEY_ACTION_RELEASE && key == KEY_RIGHT_CONTROL) {
        window_grub_cursor(wnd, 0);
        ctx->is_rotating = 1;
    }
    else if (action ==  KEY_ACTION_RELEASE && key == KEY_N)
        ctx->visualizing_normals = !ctx->visualizing_normals;
}

static void on_mouse_button(struct window* wnd, int button, int action, int mods)
{
    (void) mods;
    struct game_context* ctx = window_get_userdata(wnd);
    if (action == KEY_ACTION_RELEASE && button == MOUSE_LEFT) {
        window_grub_cursor(wnd, 1);
        ctx->is_rotating = 0;
    }
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
        GLuint nm_attrib = 2;
        glEnableVertexAttribArray(nm_attrib);
        glVertexAttribPointer(nm_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (GLvoid*)offsetof(struct vertex, normal));

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
    GLuint tex_id[16] = {};
    vector_init(&ctx->gobjects, sizeof(struct game_object));

    /* Podium */
    upload_model_geom_data("ext/models/podium/podium.obj", &go.model);
    /*-*/
    vector_init(&go.diff_textures, sizeof(GLuint));
    tex_id[0] = upload_texture("ext/models/podium/podium.png");
    vector_append(&go.diff_textures, tex_id + 0);
    /*-*/
    scale = 0.08;
    go.transform = mat4_mul_mat4(
        mat4_translation(vec3_new(0.0f, -0.5f, 0.0f)),
        mat4_scale(vec3_new(scale, scale, scale)));
    vector_append(&ctx->gobjects, &go);

    /* Sword */
    upload_model_geom_data("ext/models/artorias_sword/Artorias_Sword.fbx", &go.model);
    /*-*/
    vector_init(&go.diff_textures, sizeof(GLuint));
    tex_id[0] = upload_texture("ext/models/artorias_sword/Sword_albedo.jpg");
    vector_append(&go.diff_textures, tex_id + 0);
    /*-*/
    scale = 0.06;
    go.transform = mat4_mul_mat4(
        mat4_translation(vec3_new(0.0f, -0.5f, 0.0f)),
        mat4_scale(vec3_new(scale, scale, scale)));
    go.transform = mat4_mul_mat4(go.transform, mat4_rotation_x(-radians(90)));
    go.transform = mat4_mul_mat4(go.transform, mat4_rotation_z(radians(45)));
    vector_append(&ctx->gobjects, &go);

    /* Alduin */
    upload_model_geom_data("ext/models/alduin/alduin.obj", &go.model);
    /*-*/
    vector_init(&go.diff_textures, sizeof(GLuint));
    tex_id[0] = upload_texture("ext/models/alduin/tex/alduin.jpg");
    tex_id[1] = upload_texture("ext/models/alduin/tex/alduineyes.jpg");
    vector_append(&go.diff_textures, tex_id + 0);
    vector_append(&go.diff_textures, tex_id + 1);
    /*-*/
    scale = 0.0025;
    go.transform = mat4_mul_mat4(
        mat4_translation(vec3_new(0.4f, -0.4f, 0.0f)),
        mat4_scale(vec3_new(scale, scale, scale)));
    vector_append(&ctx->gobjects, &go);

    /* Cube */
    upload_model_geom_data("ext/cube.obj", &go.model);
    /*-*/
    vector_init(&go.diff_textures, sizeof(GLuint));
    tex_id[0] = upload_texture("ext/floor.tga");
    vector_append(&go.diff_textures, tex_id + 0);
    /*-*/
    go.transform = mat4_translation(vec3_new(0.0f, 0.1f, 0.0f));
    vector_append(&ctx->gobjects, &go);

    /* Cube2 */
    upload_model_geom_data("ext/cube.fbx", &go.model);
    /*-*/
    vector_init(&go.diff_textures, sizeof(GLuint));
    tex_id[0] = upload_texture("ext/Bark2.tif");
    vector_append(&go.diff_textures, tex_id + 0);
    /*-*/
    go.transform = mat4_translation(vec3_new(0.0f, 0.1f, 0.0f));
    vector_append(&ctx->gobjects, &go);

    /* Barrel */
    upload_model_geom_data("ext/models/barrel/barrel.fbx", &go.model);
    /*-*/
    vector_init(&go.diff_textures, sizeof(GLuint));
    tex_id[0] = upload_texture("ext/models/barrel/barrel.tif");
    vector_append(&go.diff_textures, tex_id + 0);
    /*-*/
    scale = 0.2;
    go.transform = mat4_mul_mat4(
        mat4_translation(vec3_new(0.0f, -0.4f, 0.0f)),
        mat4_scale(vec3_new(scale, scale, scale)));
    vector_append(&ctx->gobjects, &go);
}

static void game_visualize_normals_setup(struct game_context* ctx)
{
    ctx->visualizing_normals = 0;
    /* Load shaders */
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &NV_VS_SRC, 0);
    glCompileShader(vs);
    gl_check_last_compile_error(vs);
    GLuint gs = glCreateShader(GL_GEOMETRY_SHADER);
    glShaderSource(gs, 1, &NV_GS_SRC, 0);
    glCompileShader(gs);
    gl_check_last_compile_error(gs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &NV_FS_SRC, 0);
    glCompileShader(fs);
    gl_check_last_compile_error(fs);
    /* Create program */
    ctx->vis_nrm_prog = glCreateProgram();
    glAttachShader(ctx->vis_nrm_prog, vs);
    glAttachShader(ctx->vis_nrm_prog, gs);
    glAttachShader(ctx->vis_nrm_prog, fs);
    glLinkProgram(ctx->vis_nrm_prog);
    gl_check_last_link_error(ctx->vis_nrm_prog);
    glDeleteShader(vs);
    glDeleteShader(gs);
    glDeleteShader(fs);
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
    wnd_callbacks.mouse_button_cb = on_mouse_button;
    window_set_callbacks(ctx->wnd, &wnd_callbacks);

    /* Setup OpenGL debug handler */
    glDebugMessageCallback(gl_debug_proc, ctx);

    /* Initialize game state data */
    ctx->rotation = 0.0f;
    ctx->is_rotating = 1;
    ctx->cur_obj = 1;

    /* Load data from files into the GPU */
    setup_data(ctx);

    /* Load shaders */
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &VS_SRC, 0);
    glCompileShader(vs);
    gl_check_last_compile_error(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &FS_SRC, 0);
    glCompileShader(fs);
    gl_check_last_compile_error(fs);

    /* Create program */
    ctx->prog = glCreateProgram();
    glAttachShader(ctx->prog, vs);
    glAttachShader(ctx->prog, fs);
    glLinkProgram(ctx->prog);
    gl_check_last_link_error(ctx->prog);
    glDeleteShader(fs);
    glDeleteShader(vs);

    /* Setup camera */
    camera_defaults(&ctx->cam);
    ctx->cam.pos = vec3_new(0.0, 1.0, 2.0);
    ctx->cam.front = vec3_normalize(vec3_mul(ctx->cam.pos, -1));

    /* Normal visualization */
    game_visualize_normals_setup(ctx);

    /* Initialize text rendering */
    ctx->text_rndr = text_render_init();
}

void game_update(void* userdata, float dt)
{
    struct game_context* ctx = userdata;
    /* Process input events */
    window_update(ctx->wnd);
    /* Update game state */
    ctx->rotation_prev = ctx->rotation;
    ctx->rotation += dt * 0.001f;
    /* Update camera position */
    int cam_mov_flags = 0x0;
    if (window_key_state(ctx->wnd, KEY_W) == KEY_ACTION_PRESS)
        cam_mov_flags |= cmd_forward;
    if (window_key_state(ctx->wnd, KEY_A) == KEY_ACTION_PRESS)
        cam_mov_flags |= cmd_left;
    if (window_key_state(ctx->wnd, KEY_S) == KEY_ACTION_PRESS)
        cam_mov_flags |= cmd_backward;
    if (window_key_state(ctx->wnd, KEY_D) == KEY_ACTION_PRESS)
        cam_mov_flags |= cmd_right;
    camera_move(&ctx->cam, cam_mov_flags);
    /* Update camera look */
    float cur_diff_x = 0, cur_diff_y = 0;
    window_get_cursor_diff(ctx->wnd, &cur_diff_x, &cur_diff_y);
    if (window_is_cursor_grubbed(ctx->wnd))
        camera_look(&ctx->cam, cur_diff_x, cur_diff_y);
    /* Update camera matrix */
    camera_update(&ctx->cam);
}

static void game_visualize_normals_render(struct game_context* ctx, mat4* view, mat4* proj)
{
    /* Setup game object to be rendered */
    struct game_object* gobj = vector_at(&ctx->gobjects, ctx->cur_obj);
    struct model_handle* mdlh = &gobj->model;

    /* Upload MVP matrix */
    glUseProgram(ctx->vis_nrm_prog);
    glUniformMatrix4fv(glGetUniformLocation(ctx->vis_nrm_prog, "projection"), 1, GL_TRUE, (GLfloat*)proj);
    glUniformMatrix4fv(glGetUniformLocation(ctx->vis_nrm_prog, "view"), 1, GL_TRUE, (GLfloat*)view);
    glUniformMatrix4fv(glGetUniformLocation(ctx->vis_nrm_prog, "model"), 1, GL_TRUE, (GLfloat*)&gobj->transform);

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

    glUseProgram(0);
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
    mat4 view;
    if (ctx->is_rotating) {
        float cam_pos_x = 2.0 * cos(rotation_interpolated);
        float cam_pos_y = 2.0 * sin(rotation_interpolated);
        view = mat4_view_look_at(
            vec3_new(cam_pos_x, 1.0f, cam_pos_y),  /* Position */
            vec3_zero(),                           /* Target */
            vec3_new(0.0f, 1.0f, 0.0f));           /* Up */
    } else {
        //nview = camera_interpolated_view(&ctx->cam, interpolation);
        view = ctx->cam.view_mat;
    }
    mat4 proj = mat4_perspective(radians(45.0f), 0.1f, 300.0f, 1.0f / (800.0f / 600.0f));

    /* Render */
    glUseProgram(ctx->prog);
    GLuint img_loc = glGetUniformLocation(ctx->prog, "diffTex");
    glUniform1i(img_loc, 0);

    /* Construct list of shown objects */
    struct game_object* gobjl[] = {
        vector_at(&ctx->gobjects, 0),
        vector_at(&ctx->gobjects, ctx->cur_obj)
    };
    /* Loop through objects */
    for (unsigned int i = 0; i < 2; ++i) {
        /* Setup game object to be rendered */
        struct game_object* gobj = gobjl[i];
        struct model_handle* mdlh = &gobj->model;
        /* Upload MVP matrix */
        mat4 mvp = mat4_mul_mat4(mat4_mul_mat4(proj, view), gobj->transform);
        GLuint mvp_loc = glGetUniformLocation(ctx->prog, "MVP");
        mvp = mat4_transpose(mvp);
        glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, (GLfloat*)&mvp);

        /* Render mesh by mesh */
        for (unsigned int i = 0; i < mdlh->num_meshes; ++i) {
            struct mesh_handle* mh = mdlh->meshes + i;
            /* Set diffuse texture */
            glActiveTexture(GL_TEXTURE0);
            GLuint diff_tex = *(GLuint*)vector_at(&gobj->diff_textures, i);
            glBindTexture(GL_TEXTURE_2D, diff_tex);
            /* Render */
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

    /* Visualize normals */
    if (ctx->visualizing_normals)
        game_visualize_normals_render(ctx, &view, &proj);

    /* Render sample text */
    char* text = "A Quick Brown Fox Jumps Over The Lazy Dog 0123456789";
    text_render_print(ctx->text_rndr, text, vec2_new(10, 10), vec4_light_grey());

    /* Show rendered contents from the backbuffer */
    window_swap_buffers(ctx->wnd);
}

void game_shutdown(struct game_context* ctx)
{
    /* Unbind GPU handles */
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    /* Free text resources */
    text_render_shutdown(ctx->text_rndr);

    /* Free normal visualization resources */
    glDeleteProgram(ctx->vis_nrm_prog);

    /* Free model and texture resources */
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
        /* Free textures */
        for (unsigned int j = 0; j < gobj->diff_textures.size; ++j) {
            GLuint diff_tex = *(GLuint*)vector_at(&gobj->diff_textures, j);
            glDeleteTextures(1, &diff_tex);
        }
        vector_destroy(&gobj->diff_textures);
    }
    vector_destroy(&ctx->gobjects);

    /* Delete main shader */
    glDeleteProgram(ctx->prog);

    /* Close window */
    window_destroy(ctx->wnd);
}
