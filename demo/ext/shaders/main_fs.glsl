#version 330 core
in vec2 UV;
out vec4 out_color;

uniform vec3 diffCol;
uniform sampler2D diffTex;

void main()
{
    vec4 albedo = texture(diffTex, UV);
    vec3 Color = albedo.rgb + diffCol;
    if (albedo.a < 0.01)
        discard;
    out_color = vec4(Color, 1.0);
}
