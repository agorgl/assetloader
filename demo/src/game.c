#include "game.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <gfxwnd/window.h>
#include <glad/glad.h>
#include "shader.h"
#include "static_data.h"
#include <stdio.h>
#include <assets/assetload.h>
#include <assets/abstractfs.h>
#include <prof.h>

static void APIENTRY gl_debug_proc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* user_param)
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

static void shader_load_err(void* userdata, const char* err)
{
    (void) userdata;
    fprintf(stderr, "%s\n", err);
}

static GLuint load_shader_from_files(const char* vsp, const char* gsp, const char* fsp)
{
    /* Load settings */
    struct shader_load_settings settings;
    memset(&settings, 0, sizeof(settings));
    settings.load_type = SHADER_LOAD_FILE;
    settings.error_cb = shader_load_err;
    /* Vertex */
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    const char* vs_src = shader_load(vsp, &settings);
    glShaderSource(vs, 1, &vs_src, 0);
    free((void*)vs_src);
    glCompileShader(vs);
    gl_check_last_compile_error(vs);
    /* Geometry */
    GLuint gs = 0;
    if (gsp) {
        gs = glCreateShader(GL_GEOMETRY_SHADER);
        const char* gs_src = shader_load(gsp, &settings);
        glShaderSource(gs, 1, &gs_src, 0);
        free((void*)gs_src);
        glCompileShader(gs);
        gl_check_last_compile_error(gs);
    }
    /* Fragment */
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fs_src = shader_load(fsp, &settings);
    glShaderSource(fs, 1, &fs_src, 0);
    free((void*)fs_src);
    glCompileShader(fs);
    gl_check_last_compile_error(fs);
    /* Create program */
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    if (gsp)
        glAttachShader(prog, gs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    gl_check_last_link_error(prog);
    /* Free unnecessary resources */
    glDeleteShader(vs);
    if (gsp)
        glDeleteShader(gs);
    glDeleteShader(fs);
    return prog;
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
    else if (action ==  KEY_ACTION_RELEASE && key == KEY_O)
        ctx->show_wireframe = !ctx->show_wireframe;
    else if (action ==  KEY_ACTION_RELEASE && key == KEY_N)
        ctx->visualizing_normals = !ctx->visualizing_normals;
    else if (action ==  KEY_ACTION_RELEASE && key == KEY_B)
        ctx->visualizing_skeleton = !ctx->visualizing_skeleton;
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

static void print_model_info(const char* filename, struct model* m)
{
    printf("Model: %s\n", filename);
    printf(" Num meshes: %d\n", m->num_meshes);

    unsigned int total_verts = 0;
    unsigned int total_indices = 0;
    for (int i = 0; i < m->num_meshes; ++i) {
        struct mesh* mesh = m->meshes[i];
        total_verts += mesh->num_verts;
        total_indices += mesh->num_indices;
    }
    printf(" Num vertices: %d\n", total_verts);
    printf(" Num indices: %d\n", total_indices);

    for (size_t i = 0; i < m->num_mesh_groups; ++i) {
        struct mesh_group* mgroup = m->mesh_groups[i];
        printf(" Mesh group \"%s\" (%lu meshes, %u materials)\n",
                 mgroup->name, mgroup->num_mesh_offs, mgroup->num_materials);
        for (unsigned int j = 0; j < mgroup->num_mesh_offs; ++j) {
            size_t mesh_ofs = mgroup->mesh_offsets[j];
            struct mesh* mesh = m->meshes[mesh_ofs];
            printf("  Mesh[%u] material: %d\n", j, mesh->mat_index);
        }
    }
}

static void upload_model_geom_data(const char* filename, struct model_handle* model)
{
    /* Parse file */
    clock_t t1 = clock();
    struct model* m = model_from_file(filename);
    clock_t t2 = clock();
    print_model_info(filename, m);
    printf("Load time %lu msec\n\n", 1000 * (t2 - t1) / CLOCKS_PER_SEC);

    /* Allocate handle memory */
    model->num_meshes = m->num_meshes;
    model->meshes = malloc(m->num_meshes * sizeof(struct mesh_handle));
    memset(model->meshes, 0, model->num_meshes * sizeof(struct mesh_handle));

    for (unsigned int i = 0; i < model->num_meshes; ++i) {
        struct mesh* mesh = m->meshes[i];
        struct mesh_handle* mh = model->meshes + i;
        mh->mat_idx = mesh->mat_index;

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

        /* Create vertex weight data vbo */
        if (mesh->weights) {
            glGenBuffers(1, &mh->wbo);
            glBindBuffer(GL_ARRAY_BUFFER, mh->wbo);
            glBufferData(GL_ARRAY_BUFFER,
                mesh->num_verts * sizeof(struct vertex_weight),
                mesh->weights,
                GL_STATIC_DRAW);

            GLuint bi_attrib = 3;
            glEnableVertexAttribArray(bi_attrib);
            glVertexAttribIPointer(bi_attrib, 4, GL_UNSIGNED_INT, sizeof(struct vertex_weight), (GLvoid*)offsetof(struct vertex_weight, bone_ids));
            GLuint bw_attrib = 4;
            glEnableVertexAttribArray(bw_attrib);
            glVertexAttribPointer(bw_attrib, 4, GL_FLOAT, GL_FALSE, sizeof(struct vertex_weight), (GLvoid*)offsetof(struct vertex_weight, bone_weights));
        }

        /* Create indice ebo */
        glGenBuffers(1, &mh->ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mh->ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                mesh->num_indices * sizeof(GLuint),
                mesh->indices,
                GL_STATIC_DRAW);
        mh->indice_count = mesh->num_indices;
    }

    /* Move skeleton and frameset */
    model->skel = m->skeleton;
    model->fset = m->frameset;
    model->mesh_groups = m->mesh_groups;
    model->num_mesh_groups = m->num_mesh_groups;
    m->skeleton = 0;
    m->frameset = 0;
    m->mesh_groups = 0;

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

static struct {
    const char* model_loc;
    const char* diff_tex_locs[10];
    size_t diff_tex_refs[16];
    float translation[3];
    float rotation[3];
    float scaling;
    int use_fscale;
} scene_objects[] = {
    {
        /* Podium */
        .model_loc     = "models/podium/podium.obj",
        .diff_tex_locs = {
            "models/podium/podium.png"
        },
        .diff_tex_refs = {0},
        .translation   = {0.0f, -0.5f, 0.0f},
        .rotation      = {0.0f, 0.0f, 0.0f},
        .scaling       = 0.08f,
        .use_fscale    = 0
    },
    {
        /* Warrior Woman */
        .model_loc     = "models/warrior_woman/Medieval_character_01.fbx",
        .diff_tex_locs = {
            "models/warrior_woman/Armor_01.png",
            "models/warrior_woman/Head.png",
            "models/warrior_woman/Kiem.png"
        },
        .diff_tex_refs = {0, 0, 0, 1, 2, 2, 2},
        .translation   = {0.0f, -0.4f, 0.0f},
        .rotation      = {0.0f, 0.0f, 0.0f},
        .scaling       = 0.8f,
        .use_fscale    = 1
    },
    {
        /* Artorias Sword */
        .model_loc     = "models/artorias_sword/Artorias_Sword.fbx",
        .diff_tex_locs = {
            "models/artorias_sword/Sword_albedo.jpg"
        },
        .diff_tex_refs = {0},
        .translation   = {0.0f, -0.4f, 0.0f},
        .rotation      = {0.0f, 0.0f, 0.0f},
        .scaling       = 6.0f,
        .use_fscale    = 1
    },
    {
        /* Alduin */
        .model_loc     = "models/alduin/alduin.obj",
        .diff_tex_locs = {
            "models/alduin/tex/alduin.jpg",
            "models/alduin/tex/alduineyes.jpg"
        },
        .diff_tex_refs = {0, 1},
        .translation   = {0.4f, -0.4f, 0.0f},
        .rotation      = {0.0f, 0.0f, 0.0f},
        .scaling       = 0.25f,
        .use_fscale    = 1
    },
    {
        /* Mr Fixit */
        .model_loc     = "models/mrfixit/mrfixit.iqm",
        .diff_tex_locs = {
            "models/mrfixit/Body.tga",
            "models/mrfixit/Head.tga"
        },
        .diff_tex_refs = {0, 1},
        .translation   = {0.0f, -0.4f, 0.0f},
        .rotation      = {90.0f, 0.0f, 0.0f},
        .scaling       = 0.2f,
        .use_fscale    = 0
    },
    {
        /* Cube */
        .model_loc     = "models/cube.obj",
        .diff_tex_locs = {
            "textures/floor.tga"
        },
        .diff_tex_refs = {0},
        .translation   = {0.0f, 0.1f, 0.0f},
        .rotation      = {0.0f, 0.0f, 0.0f},
        .scaling       = 1.0f,
        .use_fscale    = 0
    },
    {
        /* Cube2 */
        .model_loc     = "models/cube.fbx",
        .diff_tex_locs = {
            "textures/Bark2.tif"
        },
        .diff_tex_refs = {0},
        .translation   = {0.0f, 0.1f, 0.0f},
        .rotation      = {0.0f, 0.0f, 0.0f},
        .scaling       = 1.0f,
        .use_fscale    = 0
    },
    {
        /* Barrel */
        .model_loc     = "models/barrel/barrel.fbx",
        .diff_tex_locs = {
            "models/barrel/barrel.tif"
        },
        .diff_tex_refs = {0},
        .translation   = {0.0f, -0.4f, 0.0f},
        .rotation      = {0.0f, 0.0f, 0.0f},
        .scaling       = 20.0f,
        .use_fscale    = 1
    },
};

static void setup_data(struct game_context* ctx)
{
    /* Initialize vector of game objects */
    vector_init(&ctx->gobjects, sizeof(struct game_object));
    /* Add all scene objects */
    size_t num_scene_objects = sizeof(scene_objects) / sizeof(scene_objects[0]);
    for (size_t i = 0; i < num_scene_objects; ++i) {
        struct game_object go;
        /* Load, parse and upload model */
        upload_model_geom_data(scene_objects[i].model_loc, &go.model);
        /* Load, parse and upload each texture */
        vector_init(&go.diff_textures, sizeof(GLuint));
        for (int j = 0; j < 10; ++j) {
            const char* diff_tex_loc = scene_objects[i].diff_tex_locs[j];
            if (!diff_tex_loc)
                break;
            GLuint tex_id = upload_texture(diff_tex_loc);
            vector_append(&go.diff_textures, &tex_id);
        }
        memcpy(go.mat_refs, scene_objects[i].diff_tex_refs, 16 * sizeof(size_t));
        /* Construct model matrix */
        float* pos = scene_objects[i].translation;
        float* rot = scene_objects[i].rotation;
        float unit_scale = (scene_objects[i].use_fscale ? 0.01f : 1.0f);
        float scl = scene_objects[i].scaling * unit_scale;
        go.transform = mat4_mul_mat4(
            mat4_mul_mat4(
                mat4_translation(vec3_new(pos[0], pos[1], pos[2])),
                mat4_rotation_euler(radians(rot[0]), radians(rot[1]), radians(rot[2]))
            ),
            mat4_scale(vec3_new(scl, scl, scl))
        );
        vector_append(&ctx->gobjects, &go);
    }
}

static void game_visualize_normals_setup(struct game_context* ctx)
{
    ctx->visualizing_normals = 0;
    ctx->vis_nrm_prog = load_shader_from_files(
        "shaders/nm_vis_vs.glsl",
        "shaders/nm_vis_gs.glsl",
        "shaders/nm_vis_fs.glsl"
    );
}

static void game_visualize_skeleton_setup(struct game_context* ctx)
{
    ctx->visualizing_skeleton = 0;
    ctx->vis_skel_prog = load_shader_from_files(
        "shaders/sv_vis_vs.glsl",
        "shaders/sv_vis_gs.glsl",
        "shaders/sv_vis_fs.glsl"
    );
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

    /* Initialize abstract file system */
    afs_init();
    /* Mount "ext" as root data directory */
    afs_mount("ext", "/", 0);
    /* Mount "ext/models.dat" zip file to "models" folder */
    afs_mount("ext/models.dat", "/models", 0);

    /* Initialize game state data */
    ctx->rotation = 0.0f;
    ctx->is_rotating = 1;
    ctx->cur_obj = 1;

    /* Load data from files into the GPU */
    timepoint_t t1 = millisecs();
    setup_data(ctx);
    timepoint_t t2 = millisecs();
    printf("Total time: %lu:%lu\n", (t2 - t1) / 1000, (t2 - t1) % 1000);

    /* Load shaders */
    ctx->prog = load_shader_from_files(
        "shaders/main_vs.glsl",
        0,
        "shaders/main_fs.glsl"
    );

    /* Setup camera */
    camera_defaults(&ctx->cam);
    ctx->cam.pos = vec3_new(0.0, 1.4, 3.0);
    ctx->cam.front = vec3_normalize(vec3_mul(ctx->cam.pos, -1));

    /* Wireframe visualization */
    ctx->show_wireframe = 0;

    /* Normal visualization */
    game_visualize_normals_setup(ctx);

    /* Skeleton visualization */
    game_visualize_skeleton_setup(ctx);

    /* Initialize text rendering */
    ctx->text_rndr = text_render_init();

    /* Animation */
    ctx->anim_tmr = 0;
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
    /* Update animation state */
    ctx->anim_tmr += 25.0f * (dt / 1000.0f);
}

static void game_bones_calculate(struct skeleton* skel, struct frame* f, mat4** bones, size_t* num_bones)
{
    *num_bones = f->num_joints;
    /* Calc inverse skeleton matrices */
    mat4* invskel = malloc(*num_bones * sizeof(mat4));
    memset(invskel, 0, *num_bones * sizeof(mat4));
    for (size_t i = 0; i < *num_bones; ++i) {
        struct joint* j = skel->rest_pose->joints + i;
        float trans[16];
        frame_joint_transform(j, trans);
        invskel[i] = mat4_inverse(*(mat4*)trans);
    }
    /* Calc bone matrices */
    *bones = malloc(*num_bones * sizeof(mat4));
    memset(*bones, 0, *num_bones * sizeof(mat4));
    for (size_t i = 0; i < f->num_joints; ++i) {
        struct joint* j = f->joints + i;
        float trans[16];
        frame_joint_transform(j, trans);
        (*bones)[i] = mat4_mul_mat4(*(mat4*)trans, invskel[i]);
    }
    free(invskel);
}

static void game_upload_bones(struct game_context* ctx, GLuint prog)
{
    struct game_object* gobj = vector_at(&ctx->gobjects, ctx->cur_obj);
    if (gobj->model.skel && gobj->model.fset) {
        /* Current frame */
        size_t cur_fr_idx = (int)ctx->anim_tmr % gobj->model.fset->num_frames;
        struct frame* cur_fr = gobj->model.fset->frames[cur_fr_idx];
        /* Translations */
        mat4* bones;
        size_t num_bones;
        game_bones_calculate(gobj->model.skel, cur_fr, &bones, &num_bones);
        /* Loop through each bone */
        for (size_t i = 0; i < num_bones; ++i) {
            /* Construct uniform name ("bones[" + i + "]" + '\0') */
            size_t uname_sz = 6 + 3 + 1 + 1;
            char* uname = calloc(uname_sz, 1);
            strcat(uname, "bones[");
            snprintf(uname + 6, 3, "%lu", i);
            strcat(uname, "]");
            /* Upload */
            GLuint bone_loc = glGetUniformLocation(prog, uname);
            glUniformMatrix4fv(bone_loc, 1, GL_FALSE, bones[i].m);
            free(uname);
        }
        free(bones);
    }
}

static void game_visualize_normals_render(struct game_context* ctx, mat4* view, mat4* proj)
{
    /* Select game object to be rendered */
    struct game_object* gobj = vector_at(&ctx->gobjects, ctx->cur_obj);
    struct model_handle* mdlh = &gobj->model;

    /* Upload MVP matrix */
    glUseProgram(ctx->vis_nrm_prog);
    glUniformMatrix4fv(glGetUniformLocation(ctx->vis_nrm_prog, "projection"), 1, GL_FALSE, proj->m);
    glUniformMatrix4fv(glGetUniformLocation(ctx->vis_nrm_prog, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(ctx->vis_nrm_prog, "model"), 1, GL_FALSE, gobj->transform.m);
    /* Upload animated flag */
    glUniform1i(glGetUniformLocation(ctx->vis_nrm_prog, "animated"), gobj->model.fset != 0);
    /* Upload bones */
    game_upload_bones(ctx, ctx->vis_nrm_prog);

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

static void game_points_from_skeleton(struct frame* f, float** points, size_t* num_points)
{
    /* Construct points from skeleton */
    *num_points = f->num_joints * 2;
    *points = malloc(*num_points * 3 * sizeof(float));
    memset(*points, 0, *num_points * 3 * sizeof(float));
    for (size_t i = 0; i < f->num_joints; ++i) {
        /* Tranformed current point */
        struct joint* j = f->joints + i;
        float trans[16];
        frame_joint_transform(j, trans);
        vec3 tpt = mat4_mul_vec3(*(mat4*)trans, vec3_new(0, 0, 0));
        /* Transformed parent point */
        vec3 tppt;
        if (j->parent) {
            struct joint* pj = j->parent;
            float ptrans[16];
            frame_joint_transform(pj, ptrans);
            tppt = mat4_mul_vec3(*(mat4*)ptrans, vec3_new(0, 0, 0));
        } else {
            tppt = tpt;
        }
        /* Store local space transformed positions */
        memcpy(*points + i * 6 + 0, &tpt, 3 * sizeof(float));
        memcpy(*points + i * 6 + 3, &tppt, 3 * sizeof(float));
    }
}

static void game_visualize_skeleton_render(struct game_context* ctx, mat4* view, mat4* proj, mat4* model, struct frame* frame)
{
    float* pts = 0;
    size_t num_pts;
    game_points_from_skeleton(frame, &pts, &num_pts);
    glDisable(GL_DEPTH_TEST);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, num_pts * 3 * sizeof(float), pts, GL_STATIC_DRAW);
    free(pts);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLint pos_attrib = glGetAttribLocation(ctx->vis_skel_prog, "pos");
    glEnableVertexAttribArray(pos_attrib);
    glVertexAttribPointer(pos_attrib, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glUseProgram(ctx->vis_skel_prog);
    glUniformMatrix4fv(glGetUniformLocation(ctx->vis_skel_prog, "projection"), 1, GL_FALSE, proj->m);
    glUniformMatrix4fv(glGetUniformLocation(ctx->vis_skel_prog, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(ctx->vis_skel_prog, "model"), 1, GL_FALSE, model->m);

    glDrawArrays(GL_LINES, 0, num_pts);
    glUseProgram(0);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &vbo);
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
        float cam_pos_x = 3.0 * cos(rotation_interpolated);
        float cam_pos_y = 3.0 * sin(rotation_interpolated);
        view = mat4_view_look_at(
            vec3_new(cam_pos_x, 1.4f, cam_pos_y),  /* Position */
            vec3_zero(),                           /* Target */
            vec3_new(0.0f, 1.0f, 0.0f));           /* Up */
    } else {
        //nview = camera_interpolated_view(&ctx->cam, interpolation);
        view = ctx->cam.view_mat;
    }
    mat4 proj = mat4_perspective(radians(45.0f), 0.1f, 300.0f, (800.0f / 600.0f));

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
        glPolygonMode(GL_FRONT_AND_BACK, (i > 0 && ctx->show_wireframe) ? GL_LINE : GL_FILL);
        /* Setup game object to be rendered */
        struct game_object* gobj = gobjl[i];
        struct model_handle* mdlh = &gobj->model;
        /* Upload MVP matrix */
        mat4 mvp = mat4_mul_mat4(mat4_mul_mat4(proj, view), gobj->transform);
        GLuint mvp_loc = glGetUniformLocation(ctx->prog, "MVP");
        glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, mvp.m);
        /* Upload animated flag */
        glUniform1i(glGetUniformLocation(ctx->prog, "animated"), gobj->model.fset != 0);
        /* Upload bones */
        game_upload_bones(ctx, ctx->prog);
        /* Render mesh_group by mesh_group */
        unsigned int mat_list_ofs = 0;
        for (unsigned int i = 0; i < mdlh->num_mesh_groups; ++i) {
            struct mesh_group* mgroup = mdlh->mesh_groups[i];
            for (unsigned int j = 0; j < mgroup->num_mesh_offs; ++j) {
                size_t mesh_ofs = mgroup->mesh_offsets[j];
                struct mesh_handle* mh = mdlh->meshes + mesh_ofs;
                /* Set diffuse texture */
                glActiveTexture(GL_TEXTURE0);
                size_t mat_idx = gobj->mat_refs[mat_list_ofs + mh->mat_idx];
                GLuint diff_tex = *(GLuint*)vector_at(&gobj->diff_textures, mat_idx);
                glBindTexture(GL_TEXTURE_2D, diff_tex);
                /* Render */
                glBindVertexArray(mh->vao);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mh->ebo);
                glDrawElements(GL_TRIANGLES, mh->indice_count, GL_UNSIGNED_INT, (void*)0);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                glBindVertexArray(0);
            }
            mat_list_ofs += mgroup->num_materials;
        }
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glUseProgram(0);

    /* Visualize normals */
    if (ctx->visualizing_normals)
        game_visualize_normals_render(ctx, &view, &proj);

    /* Visualize skeleton */
    struct model_handle* vobj = &gobjl[1]->model;
    if (ctx->visualizing_skeleton && vobj->skel) {
        struct frame* cur_fr = 0;
        if (vobj->fset) {
            size_t cur_fr_idx = (int)ctx->anim_tmr % vobj->fset->num_frames;
            cur_fr = vobj->fset->frames[cur_fr_idx];
        } else {
            cur_fr = vobj->skel->rest_pose;
        }
        game_visualize_skeleton_render(ctx, &view, &proj, &gobjl[1]->transform, cur_fr);
    }

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
            if (mh->wbo)
                glDeleteBuffers(1, &mh->wbo);
            glDeleteVertexArrays(1, &mh->vao);
        }
        free(gobj->model.meshes);
        /* Free textures */
        for (unsigned int j = 0; j < gobj->diff_textures.size; ++j) {
            GLuint diff_tex = *(GLuint*)vector_at(&gobj->diff_textures, j);
            glDeleteTextures(1, &diff_tex);
        }
        vector_destroy(&gobj->diff_textures);
        /* Free skeleton if exists */
        if (gobj->model.skel)
            skeleton_delete(gobj->model.skel);
        /* Free frameset if exists */
        if (gobj->model.fset)
            frameset_delete(gobj->model.fset);
        /* Free mesh group info */
        for (unsigned int i = 0; i < gobj->model.num_mesh_groups; ++i)
            mesh_group_delete(gobj->model.mesh_groups[i]);
        free(gobj->model.mesh_groups);
    }
    vector_destroy(&ctx->gobjects);

    /* Delete main shader */
    glDeleteProgram(ctx->prog);

    /* Deinit abstract file system */
    afs_deinit();

    /* Close window */
    window_destroy(ctx->wnd);
}
