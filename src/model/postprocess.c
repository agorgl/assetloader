#include "assets/model/postprocess.h"
#include <string.h>
#include <linalgb.h>

void triangle_tangent(float tangent[3], struct vertex* v1, struct vertex* v2, struct vertex* v3)
{
    vec2 uv1 = *(vec2*)v1->uvs;
    vec2 uv2 = *(vec2*)v2->uvs;
    vec2 uv3 = *(vec2*)v3->uvs;

    /* Calculate edges */
    vec3 e1 = vec3_sub(*(vec3*)v2->position, *(vec3*)v1->position);
    vec3 e2 = vec3_sub(*(vec3*)v3->position, *(vec3*)v1->position);

    /* Generate uv space vectors */
    float s1 = uv2.x - uv1.x;
    float t1 = uv2.y - uv1.y;

    float s2 = uv3.x - uv1.x;
    float t2 = uv3.y - uv1.y;

    float f = 1.0f / ((s1 * t2) - (s2 * t1));

    vec3 tdir = vec3_new(
        f * (t2 * e1.x - t1 * e2.x),
        f * (t2 * e1.y - t1 * e2.y),
        f * (t2 * e1.z - t1 * e2.z));

    tdir = vec3_normalize(tdir);
    memcpy(tangent, &tdir, 3 * sizeof(float));
}

void triangle_binormal(float binormal[3], struct vertex* v1, struct vertex* v2, struct vertex* v3)
{
    vec2 uv1 = *(vec2*)v1->uvs;
    vec2 uv2 = *(vec2*)v2->uvs;
    vec2 uv3 = *(vec2*)v3->uvs;

    /* Calculate edges */
    vec3 e1 = vec3_sub(*(vec3*)v2->position, *(vec3*)v1->position);
    vec3 e2 = vec3_sub(*(vec3*)v3->position, *(vec3*)v1->position);

    /* Generate uv space vectors */
    float s1 = uv2.x - uv1.x;
    float t1 = uv2.y - uv1.y;

    float s2 = uv3.x - uv1.x;
    float t2 = uv3.y - uv1.y;

    float f = 1.0f / ((s1 * t2) - (s2 * t1));

    vec3 sdir = vec3_new(
        f * (-s2 * e1.x - s1 * e2.x),
        f * (-s2 * e1.y - s1 * e2.y),
        f * (-s2 * e1.z - s1 * e2.z));
    sdir = vec3_normalize(sdir);
    memcpy(binormal, &sdir, 3 * sizeof(float));
}

void triangle_normal(float out_normal[3], struct vertex* v1, struct vertex* v2, struct vertex* v3)
{
    vec3 edge1 = vec3_sub(*(vec3*)v2->position, *(vec3*)v1->position);
    vec3 edge2 = vec3_sub(*(vec3*)v3->position, *(vec3*)v1->position);
    vec3 normal = vec3_cross(edge1, edge2);
    normal = vec3_normalize(normal);
    memcpy(out_normal, &normal, 3 * sizeof(float));
}

static void vec3_append(float v[3], float val[3]) { v[0] += val[0]; v[1] += val[1]; v[2] += val[2]; }

void mesh_generate_tangents(struct mesh* m)
{
    /* Clear all tangents to 0,0,0 */
    for (size_t i = 0; i < m->num_verts; i++) {
        memset(m->vertices[i].tangent, 0, sizeof(m->vertices[i].tangent));
        memset(m->vertices[i].binormal, 0, sizeof(m->vertices[i].binormal));
    }

    /* Loop over faces, calculate tangent and append to verticies of that face */
    size_t i = 0;
    while (i < m->num_indices) {
        uint32_t t_i1 = m->indices[i];
        uint32_t t_i2 = m->indices[i + 1];
        uint32_t t_i3 = m->indices[i + 2];

        struct vertex* v1 = m->vertices + t_i1;
        struct vertex* v2 = m->vertices + t_i2;
        struct vertex* v3 = m->vertices + t_i3;

        float face_tangent[3], face_binormal[3];
        triangle_tangent(face_tangent, v1, v2, v3);
        triangle_binormal(face_binormal, v1, v2, v3);

        vec3_append(v1->tangent, face_tangent);
        vec3_append(v2->tangent, face_tangent);
        vec3_append(v3->tangent, face_tangent);

        vec3_append(v1->binormal, face_binormal);
        vec3_append(v2->binormal, face_binormal);
        vec3_append(v3->binormal, face_binormal);
        i = i + 3;
    }

    /* Normalize all tangents */
    for (size_t i = 0; i < m->num_verts; i++) {
        vec3 ntangent = vec3_normalize(*(vec3*)m->vertices[i].tangent);
        vec3 nbinormal = vec3_normalize(*(vec3*)m->vertices[i].binormal);
        memcpy(m->vertices[i].tangent, &ntangent, 3 * sizeof(float));
        memcpy(m->vertices[i].binormal, &nbinormal, 3 * sizeof(float));
    }
}

void mesh_generate_normals(struct mesh* m)
{
    /* Clear all normals to 0,0,0 */
    for (size_t i = 0; i < m->num_verts; i++)
        memset(m->vertices[i].normal, 0, sizeof(m->vertices[i].normal));

    /* Loop over faces, calculate normals and append to verticies of that face */
    size_t i = 0;
    while (i < m->num_indices) {
        int t_i1 = m->indices[i];
        int t_i2 = m->indices[i + 1];
        int t_i3 = m->indices[i + 2];

        struct vertex* v1 = m->vertices + t_i1;
        struct vertex* v2 = m->vertices + t_i2;
        struct vertex* v3 = m->vertices + t_i3;

        float face_normal[3];
        triangle_normal(face_normal, v1, v2, v3);

        vec3_append(v1->normal, face_normal);
        vec3_append(v2->normal, face_normal);
        vec3_append(v3->normal, face_normal);
        i = i + 3;
    }

    /* Normalize all normals */
    for (size_t i = 0; i < m->num_verts; i++) {
        vec3 nnormal = vec3_normalize(*(vec3*)m->vertices[i].normal);
        memcpy(m->vertices[i].normal, &nnormal, 3 * sizeof(float));
    }
}

void mesh_generate_orthagonal_tangents(struct mesh* m)
{
    /* Clear all tangents to 0,0,0 */
    for (size_t i = 0; i < m->num_verts; i++) {
        memset(m->vertices[i].tangent, 0, sizeof(m->vertices[i].tangent));
        memset(m->vertices[i].binormal, 0, sizeof(m->vertices[i].binormal));
    }

    /* Loop over faces, calculate tangent and append to verticies of that face */
    size_t i = 0;
    while (i < m->num_indices) {
        int t_i1 = m->indices[i];
        int t_i2 = m->indices[i + 1];
        int t_i3 = m->indices[i + 2];

        struct vertex* v1 = m->vertices + t_i1;
        struct vertex* v2 = m->vertices + t_i2;
        struct vertex* v3 = m->vertices + t_i3;

        float face_normal[3], face_binormal_temp[3];
        triangle_normal(face_normal, v1, v2, v3);
        triangle_binormal(face_binormal_temp, v1, v2, v3);

        vec3 face_tangent = vec3_normalize(vec3_cross(*(vec3*)face_binormal_temp, *(vec3*)face_normal));
        vec3 face_binormal = vec3_normalize(vec3_cross(face_tangent, *(vec3*)face_normal));

        vec3_append(v1->tangent, (float*)&face_tangent);
        vec3_append(v2->tangent, (float*)&face_tangent);
        vec3_append(v3->tangent, (float*)&face_tangent);

        vec3_append(v1->binormal, (float*)&face_binormal);
        vec3_append(v2->binormal, (float*)&face_binormal);
        vec3_append(v3->binormal, (float*)&face_binormal);
        i = i + 3;
    }

    /* Normalize all tangents */
    for (size_t i = 0; i < m->num_verts; i++) {
        vec3 ntangent = vec3_normalize(*(vec3*)m->vertices[i].tangent);
        vec3 nbinormal = vec3_normalize(*(vec3*)m->vertices[i].binormal);
        memcpy(m->vertices[i].tangent, &ntangent, 3 * sizeof(float));
        memcpy(m->vertices[i].binormal, &nbinormal, 3 * sizeof(float));
    }
}

void mesh_generate_texcoords_cylinder(struct mesh* m)
{
    vec2 unwrap_vector = vec2_new(1, 0);

    float max_height = -99999999;
    float min_height = 99999999;

    for (size_t i = 0; i < m->num_verts; i++) {
        float v = m->vertices[i].position[1];
        max_height = max(max_height, v);
        min_height = min(min_height, v);

        vec2 proj_position = vec2_new(m->vertices[i].position[0], m->vertices[i].position[2]);
        vec2 from_center = vec2_normalize(proj_position);
        float u = (vec2_dot(from_center, unwrap_vector) + 1) / 8;

        m->vertices[i].uvs[0] = u;
        m->vertices[i].uvs[1] = v;
    }

    float scale = (max_height - min_height);
    for (size_t i = 0; i < m->num_verts; i++)
        m->vertices[i].uvs[1] = m->vertices[i].uvs[1] / scale;
}

void model_generate_normals(struct model* m)
{
    for (size_t i = 0; i < m->num_meshes; i++)
        mesh_generate_normals(m->meshes[i]);
}

void model_generate_tangents(struct model* m)
{
    for (size_t i = 0; i < m->num_meshes; i++)
        mesh_generate_tangents(m->meshes[i]);
}

void model_generate_orthagonal_tangents(struct model* m)
{
    for (size_t i = 0; i < m->num_meshes; i++)
        mesh_generate_orthagonal_tangents(m->meshes[i]);
}

void model_generate_texcoords_cylinder(struct model* m)
{
    for (size_t i = 0; i < m->num_meshes; i++)
        mesh_generate_texcoords_cylinder(m->meshes[i]);
}
