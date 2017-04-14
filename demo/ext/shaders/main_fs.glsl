#version 330 core
in vec2 UV;
out vec4 out_color;

uniform vec3 diffCol;
uniform sampler2D diffTex;

void main()
{
    vec3 Color = texture(diffTex, UV).rgb + diffCol;
    out_color = vec4(Color, 1.0);
}
