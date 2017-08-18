#include <assets/model/modelload.h>
#include <stdlib.h>
#include <string.h>
#include <linalgb.h>
#include <hashmap.h>
#include <orb/mdl.h>
#include <orb/anm.h>

struct frameset* frameset_from_anm(const unsigned char* data, size_t sz)
{
    struct anm_file anm;
    anm_parse_from_buf(&anm, (byte*)data, sz);

    struct frameset* fset = frameset_new();
    fset->num_frames = anm.header.num_frames;
    fset->frames = calloc(fset->num_frames, sizeof(struct frame*));

    struct frame* base_frame = frame_new();
    base_frame->num_joints = anm.header.num_joints;
    base_frame->joints = realloc(base_frame->joints, base_frame->num_joints * sizeof(struct joint));
    memset(base_frame->joints, 0, base_frame->num_joints * sizeof(struct joint));
    for (unsigned int i = 0; i < base_frame->num_joints; ++i) {
        struct anm_joint* aj = anm.joints + i;
        struct joint* jnt = base_frame->joints + i;
        jnt->parent = aj->par_idx != MDL_INVALID_OFFSET ? base_frame->joints + aj->par_idx : 0;
        memcpy(jnt->position, aj->position, 3 * sizeof(float));
        memcpy(jnt->rotation, aj->rotation, 4 * sizeof(float));
        memcpy(jnt->scaling,  aj->scaling,  3 * sizeof(float));
    }

    struct frame* prev_frame = base_frame;
    uint16_t* change = anm.changes;
    float* value = anm.values;
    for (unsigned int i = 0; i < fset->num_frames; ++i) {
        struct frame* f = frame_copy(prev_frame);
        for (unsigned int j = 0; j < prev_frame->num_joints; ++j) {
            struct joint* jnt = f->joints + j;
            uint16_t components = *change++;
            if (components & ANM_COMP_POSX)
                jnt->position[0] = *value++;
            if (components & ANM_COMP_POSY)
                jnt->position[1] = *value++;
            if (components & ANM_COMP_POSZ)
                jnt->position[2] = *value++;
            if (components & ANM_COMP_ROTX)
                jnt->rotation[0] = *value++;
            if (components & ANM_COMP_ROTY)
                jnt->rotation[1] = *value++;
            if (components & ANM_COMP_ROTZ)
                jnt->rotation[2] = *value++;
            if (components & ANM_COMP_ROTW)
                jnt->rotation[3] = *value++;
            if (components & ANM_COMP_SCLX)
                jnt->scaling[0] = *value++;
            if (components & ANM_COMP_SCLY)
                jnt->scaling[1] = *value++;
            if (components & ANM_COMP_SCLZ)
                jnt->scaling[2] = *value++;
        }
        fset->frames[i] = prev_frame = f;
    }
    frame_delete(base_frame);
    return fset;
}

struct model* model_from_mdl(const unsigned char* data, size_t sz)
{
    /* Parse mdl file */
    struct mdl_file mdl_file;
    mdl_parse_from_buf(&mdl_file, (byte*)data, sz);

    /* Get vertex attribute arrays */
    f32 (*pos_va)[3] = 0; f32 (*nm_va)[3] = 0; f32 (*uv_va)[2] = 0; u16 (*bi_va)[4] = 0; f32 (*bw_va)[4] = 0;
    for (unsigned int i = 0; i < mdl_file.header.num_vertex_arrays; ++i) {
        struct mdl_vertex_array* mdl_va = mdl_file.va_desc + i;
        void* data = mdl_file.va_data + mdl_va->ofs_data;
        switch (mdl_va->type) {
            case MDL_POSITION:
                pos_va = data;
                break;
            case MDL_NORMAL:
                nm_va = data;
                break;
            case MDL_TEXCOORD0:
                uv_va = data;
                break;
            case MDL_BLEND_INDEXES:
                bi_va = data;
                break;
            case MDL_BLEND_WEIGHTS:
                bw_va = data;
                break;
        }
    }

    /* Mesh name <-> Mesh group index map */
    struct hashmap mgroup_map;
    hashmap_init(&mgroup_map, hm_str_hash, hm_str_eql);

    /* Fill in model struct */
    struct model* m = model_new();
    m->num_meshes = mdl_file.header.num_mesh_descs;
    m->meshes = realloc(m->meshes, m->num_meshes * sizeof(struct mesh*));
    u32 cur_idx = 0, cur_vert = 0;
    for (unsigned int i = 0; i < m->num_meshes; ++i) {
        /* Fill in mesh struct */
        struct mdl_mesh_desc* mdl_mesh = mdl_file.mesh_desc + i;
        struct mesh* mesh = mesh_new();
        mesh->num_verts   = mdl_mesh->num_vertices;
        mesh->num_indices = mdl_mesh->num_indices;
        mesh->vertices    = realloc(mesh->vertices, mesh->num_verts * sizeof(struct vertex));
        if (mdl_file.header.flags.rigged)
            mesh->weights = realloc(mesh->weights, mesh->num_verts * sizeof(struct vertex_weight));
        mesh->indices     = realloc(mesh->indices, mesh->num_indices * sizeof(uint32_t));
        mesh->mat_index   = mdl_mesh->mat_idx;
        for (unsigned int j = 0; j < mesh->num_verts; ++j) {
            struct vertex* v = mesh->vertices + j;
            memcpy(v->position, pos_va + cur_vert + j, sizeof(float) * 3);
            memcpy(v->normal,   nm_va  + cur_vert + j, sizeof(float) * 3);
            memcpy(v->uvs,      uv_va  + cur_vert + j, sizeof(float) * 2);
            if (mdl_file.header.flags.rigged) {
                struct vertex_weight* vw = mesh->weights + j;
                for (unsigned int k = 0; k < 4; ++k) {
                    vw->bone_ids[k]     = ((u16*)(bi_va + cur_vert + j))[k];
                    vw->bone_weights[k] = ((f32*)(bw_va + cur_vert + j))[k];
                }
            }
        }
        memcpy(mesh->indices, mdl_file.indices + cur_idx, mesh->num_indices * sizeof(uint32_t));
        m->meshes[i] = mesh;
        cur_idx  += mesh->num_indices;
        cur_vert += mesh->num_verts;

        /* Setup mesh groups */
        const char* name = (const char*)(mdl_file.strings + mdl_mesh->ofs_name);
        hm_ptr* p = hashmap_get(&mgroup_map, hm_cast(name));
        if (p) {
            mesh->mgroup_idx = (uint32_t)(*p);
        } else {
            /* Create mesh group */
            struct mesh_group* mgroup = mesh_group_new();
            mgroup->name = strdup(name);
            m->num_mesh_groups++;
            m->mesh_groups = realloc(m->mesh_groups, m->num_mesh_groups * sizeof(struct mesh_group*));
            m->mesh_groups[m->num_mesh_groups - 1] = mgroup;
            /* Assign mgroup idx */
            mesh->mgroup_idx = m->num_mesh_groups - 1;
            hashmap_put(&mgroup_map, hm_cast(name), hm_cast(mesh->mgroup_idx));
        }
    }
    hashmap_destroy(&mgroup_map);

    /* Find total materials */
    for (unsigned int i = 0; i < m->num_meshes; ++i) {
        struct mdl_mesh_desc* mdl_mesh = mdl_file.mesh_desc + i;
        m->num_materials = max(m->num_materials, mdl_mesh->mat_idx + 1);
    }

    /* Read the skeleton */
    if (mdl_file.header.flags.rigged) {
        struct mdl_header* h = &mdl_file.header;
        struct skeleton* skel = skeleton_new();

        skel->rest_pose->num_joints = h->num_joints;
        skel->rest_pose->joints = realloc(skel->rest_pose->joints, skel->rest_pose->num_joints * sizeof(struct joint));
        memset(skel->rest_pose->joints, 0, skel->rest_pose->num_joints * sizeof(struct joint));

        skel->joint_names = realloc(skel->joint_names, skel->rest_pose->num_joints * sizeof(const char*));
        memset(skel->joint_names, 0, skel->rest_pose->num_joints * sizeof(const char*));

        for (unsigned int i = 0; i < h->num_joints; ++i) {
            /* Set parent */
            struct mdl_joint* mj = (struct mdl_joint*)mdl_file.joints + i;
            struct joint* jnt = skel->rest_pose->joints + i;
            jnt->parent = mj->ref_parent != MDL_INVALID_OFFSET ? skel->rest_pose->joints + mj->ref_parent : 0;
            /* Copy name */
            const char* name = (const char*)(mdl_file.strings + *((u32*)(mdl_file.joint_name_ofs) + i));
            skel->joint_names[i] = strdup(name);
            /* Copy data */
            memcpy(jnt->position, mj->position, 3 * sizeof(float));
            memcpy(jnt->rotation, mj->rotation, 4 * sizeof(float));
            memcpy(jnt->scaling,  mj->scaling, 3 * sizeof(float));
        }
        m->skeleton = skel;
    }

    return m;
}
