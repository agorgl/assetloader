#include "static_data.h"

/* Shader sources */
const char* VS_SRC =
"#version 330 core                      \n\
layout (location = 0) in vec3 position; \n\
layout (location = 1) in vec2 uv;       \n\
out vec2 UV;                            \n\
                                        \n\
uniform mat4 MVP;                       \n\
                                        \n\
void main() {                           \n\
    UV = uv;                            \n\
    gl_Position = MVP * vec4(position, 1.0); \n\
}";

const char* FS_SRC =
"#version 330 core         \n\
in vec2 UV;                \n\
out vec4 out_color;        \n\
                           \n\
uniform sampler2D diffTex; \n\
                           \n\
void main() {              \n\
    vec3 Color = texture(diffTex, UV).rgb; \n\
    out_color = vec4(Color, 1.0);          \n\
}";

/* Vertex data */
const float CUBE_VERTEX_DATA[144] = {
    /* Front */
    -0.5, -0.5,  0.5, /* 0 */
     0.5, -0.5,  0.5, /* 1 */
     0.5,  0.5,  0.5, /* 2 */
     0.5,  0.5,  0.5, /* 2 */
    -0.5,  0.5,  0.5, /* 3 */
    -0.5, -0.5,  0.5, /* 0 */
    /* Top */
     0.5, -0.5,  0.5, /* 1 */
     0.5, -0.5, -0.5, /* 5 */
     0.5,  0.5, -0.5, /* 6 */
     0.5,  0.5, -0.5, /* 6 */
     0.5,  0.5,  0.5, /* 2 */
     0.5, -0.5,  0.5, /* 1 */
    /* Back */
    -0.5,  0.5, -0.5, /* 7 */
     0.5,  0.5, -0.5, /* 6 */
     0.5, -0.5, -0.5, /* 5 */
     0.5, -0.5, -0.5, /* 5 */
    -0.5, -0.5, -0.5, /* 4 */
    -0.5,  0.5, -0.5, /* 7 */
    /* Bottom */
    -0.5, -0.5, -0.5, /* 4 */
    -0.5, -0.5,  0.5, /* 0 */
    -0.5,  0.5,  0.5, /* 3 */
    -0.5,  0.5,  0.5, /* 3 */
    -0.5,  0.5, -0.5, /* 7 */
    -0.5, -0.5, -0.5, /* 4 */
    /* Left */
    -0.5, -0.5, -0.5, /* 4 */
     0.5, -0.5, -0.5, /* 5 */
     0.5, -0.5,  0.5, /* 1 */
     0.5, -0.5,  0.5, /* 1 */
    -0.5, -0.5,  0.5, /* 0 */
    -0.5, -0.5, -0.5, /* 4 */
    /* Right */
    -0.5,  0.5,  0.5, /* 3 */
     0.5,  0.5,  0.5, /* 2 */
     0.5,  0.5, -0.5, /* 6 */
     0.5,  0.5, -0.5, /* 6 */
    -0.5,  0.5, -0.5, /* 7 */
    -0.5,  0.5,  0.5, /* 3 */
};

const unsigned int CUBE_ELEM_DATA[36] = {
    0,  1,  2,
    3,  4,  5,
    6,  7,  8,
    9,  10, 11,
    12, 13, 14,
    15, 16, 17,
    18, 19, 20,
    21, 22, 23,
    24, 25, 26,
    27, 28, 29,
    30, 31, 32,
    33, 34, 35
};

/* Indices */
/*
const unsigned int CUBE_ELEM_DATA[36] = {
    * Front *
    0, 1, 2,
    2, 3, 0,
    * Top *
    1, 5, 6,
    6, 2, 1,
    * Back *
    7, 6, 5,
    5, 4, 7,
    * Bottom *
    4, 0, 3,
    3, 7, 4,
    * Left *
    4, 5, 1,
    1, 0, 4,
    * Right *
    3, 2, 6,
    6, 7, 3,
};
*/

const float CUBE_UVS[96] = {
    /* Front */
    0.0, 0.0,
    1.0, 0.0,
    1.0, 1.0,
    1.0, 1.0,
    0.0, 1.0,
    0.0, 0.0,
    /* Top */
    0.0, 0.0,
    1.0, 0.0,
    1.0, 1.0,
    1.0, 1.0,
    0.0, 1.0,
    0.0, 0.0,
    /* Back */
    0.0, 0.0,
    1.0, 0.0,
    1.0, 1.0,
    1.0, 1.0,
    0.0, 1.0,
    0.0, 0.0,
    /* Bottom */
    0.0, 0.0,
    1.0, 0.0,
    1.0, 1.0,
    1.0, 1.0,
    0.0, 1.0,
    0.0, 0.0,
    /* Left */
    0.0, 0.0,
    1.0, 0.0,
    1.0, 1.0,
    1.0, 1.0,
    0.0, 1.0,
    0.0, 0.0,
    /* Right */
    0.0, 0.0,
    1.0, 0.0,
    1.0, 1.0,
    1.0, 1.0,
    0.0, 1.0,
    0.0, 0.0,
};

const char* NV_VS_SRC =
"#version 330 core                      \n\
layout (location = 0) in vec3 position; \n\
layout (location = 2) in vec3 normal;   \n\
                                        \n\
uniform mat4 projection;                \n\
uniform mat4 view;                      \n\
uniform mat4 model;                     \n\
                                        \n\
out VS_OUT {                            \n\
    vec3 normal;                        \n\
} vs_out;                               \n\
                                        \n\
void main()                             \n\
{                                       \n\
    gl_Position = projection * view * model * vec4(position, 1.0f);                 \n\
    mat3 normalMatrix = mat3(transpose(inverse(view * model)));                     \n\
    vs_out.normal = normalize(vec3(projection * vec4(normalMatrix * normal, 1.0))); \n\
}";

const char* NV_GS_SRC =
"#version 330 core                                     \n\
layout (triangles) in;                                 \n\
layout (line_strip, max_vertices = 6) out;             \n\
                                                       \n\
in VS_OUT {                                            \n\
    vec3 normal;                                       \n\
} gs_in[];                                             \n\
                                                       \n\
const float MAGNITUDE = 0.1f;                          \n\
                                                       \n\
void GenerateLine(int index)                           \n\
{                                                      \n\
    gl_Position = gl_in[index].gl_Position;            \n\
    EmitVertex();                                      \n\
    gl_Position = gl_in[index].gl_Position             \n\
        + vec4(gs_in[index].normal, 0.0f) * MAGNITUDE; \n\
    EmitVertex();                                      \n\
    EndPrimitive();                                    \n\
}                                                      \n\
                                                       \n\
void main()                                            \n\
{                                                      \n\
    GenerateLine(0); // First vertex normal            \n\
    GenerateLine(1); // Second vertex normal           \n\
    GenerateLine(2); // Third vertex normal            \n\
}";

const char* NV_FS_SRC =
"#version 330 core                        \n\
out vec4 out_color;                       \n\
                                          \n\
void main()                               \n\
{                                         \n\
    out_color = vec4(1.0, 1.0, 0.0, 1.0); \n\
}";

const char* SV_VS_SRC =
"#version 330 core                      \n\
layout (location = 0) in vec3 pos;      \n\
                                        \n\
uniform mat4 projection;                \n\
uniform mat4 view;                      \n\
uniform mat4 model;                     \n\
                                        \n\
void main()                             \n\
{                                       \n\
    gl_Position = projection * view * model * vec4(pos, 1.0f); \n\
}";

const char* SV_GS_SRC =
"#version 330 core                                 \n\
layout (lines) in;                                 \n\
layout (line_strip, max_vertices = 2) out;         \n\
                                                   \n\
void main()                                        \n\
{                                                  \n\
    gl_Position = gl_in[0].gl_Position;            \n\
    EmitVertex();                                  \n\
    gl_Position = gl_in[1].gl_Position;            \n\
    EmitVertex();                                  \n\
    EndPrimitive();                                \n\
}";

const char* SV_FS_SRC =
"#version 330 core                        \n\
out vec4 out_color;                       \n\
                                          \n\
void main()                               \n\
{                                         \n\
    out_color = vec4(0.0, 0.0, 1.0, 1.0); \n\
}";
