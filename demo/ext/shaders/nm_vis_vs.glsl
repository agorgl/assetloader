#version 330 core
layout (location = 0) in vec3 position;
layout (location = 2) in vec3 normal;
layout (location = 3) in uvec4 bids;
layout (location = 4) in vec4 bweights;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

const int MAX_BONES = 100;
uniform mat4 bones[MAX_BONES];
uniform bool animated;

out VS_OUT {
    vec3 normal;
} vs_out;

void main()
{
    mat4 btrans = mat4(1.0);
    if (animated) {
        btrans = mat4(0.0);
        btrans += bones[bids[0]] * bweights[0];
        btrans += bones[bids[1]] * bweights[1];
        btrans += bones[bids[2]] * bweights[2];
        btrans += bones[bids[3]] * bweights[3];
    }
    gl_Position = projection * view * model * btrans * vec4(position, 1.0f);
    mat3 normalMatrix = mat3(transpose(inverse(view * model)));
    vs_out.normal = normalize(vec3(projection * vec4(normalMatrix * normal, 1.0)));
};
