#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec2 uv;
layout (location = 3) in uvec4 bids;
layout (location = 4) in vec4 bweights;
out vec2 UV;

const int MAX_BONES = 100;
uniform mat4 MVP;
uniform mat4 bones[MAX_BONES];
uniform bool animated;

void main()
{
    UV = uv;
    if (animated) {
        mat4 btrans = mat4(0.0);
        btrans += bones[bids[0]] * bweights[0];
        btrans += bones[bids[1]] * bweights[1];
        btrans += bones[bids[2]] * bweights[2];
        btrans += bones[bids[3]] * bweights[3];
        gl_Position = MVP * btrans * vec4(position, 1.0);
    } else {
        gl_Position = MVP * vec4(position, 1.0);
    }
};
