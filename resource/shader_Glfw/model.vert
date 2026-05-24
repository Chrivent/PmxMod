#version 140

in vec3 position;
in vec3 normal;
in vec2 uv;

out vec3 vs_Pos;
out vec3 vs_Nor;
out vec2 vs_UV;

uniform mat4 wv;
uniform mat4 wvp;

void main() {
    gl_Position = wvp * vec4(position, 1.0);
    vs_Pos = (wv * vec4(position, 1.0)).xyz;
    vs_Nor = mat3(wv) * normal;
    vs_UV = vec2(uv.x, 1.0 - uv.y);
}
