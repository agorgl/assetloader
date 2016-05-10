#include "renderutils.h"
#include <glad/glad.h>

/* --------------------------------------------------
 * Renders a screen space quad
 * -------------------------------------------------- */
void render_spquad()
{
    GLfloat quad_vert[] = {
       -1.0f,  1.0f, 0.0f,
       -1.0f, -1.0f, 0.0f,
        1.0f,  1.0f, 0.0f,
        1.0f, -1.0f, 0.0f
    };

    /* Setup vertex data */
    GLuint quad_vbo;
    glGenBuffers(1, &quad_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vert), &quad_vert, GL_STATIC_DRAW);

    /* Setup vertex attributes */
    GLuint quad_vao;
    glGenVertexArrays(1, &quad_vao);
    glBindVertexArray(quad_vao);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);
    glEnableVertexAttribArray(0);

    /* Dispatch draw operation */
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    /* Free uploaded resources */
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &quad_vbo);
    glDeleteVertexArrays(1, &quad_vao);
}
