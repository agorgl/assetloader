#include "assets/model/model.h"
#include <stdlib.h>
#include <string.h>

struct model* model_new()
{
    struct model* m = malloc(sizeof(struct model));
    memset(m, 0, sizeof(struct model));
    m->meshes = malloc(0);
    return m;
}

struct mesh* mesh_new()
{
    struct mesh* mesh = malloc(sizeof(struct mesh));
    memset(mesh, 0, sizeof(struct mesh));
    mesh->vertices = malloc(0);
    mesh->indices = malloc(0);
    return mesh;
}

void model_delete(struct model* m)
{
    for (int i = 0; i < m->num_meshes; ++i)
        mesh_delete(m->meshes[i]);
    free(m->meshes);
    free(m);
}

void mesh_delete(struct mesh* mesh)
{
    free(mesh->vertices);
    free(mesh->indices);
    free(mesh);
}
