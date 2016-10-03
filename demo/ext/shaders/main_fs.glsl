#version 330 core
in vec2 UV;
out vec4 out_color;

uniform sampler2D diffTex;

void main()
{
    vec3 Color = texture(diffTex, UV).rgb;
    out_color = vec4(Color, 1.0);
};
