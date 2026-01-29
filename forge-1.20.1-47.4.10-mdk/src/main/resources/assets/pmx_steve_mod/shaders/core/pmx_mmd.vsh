#version 150

in vec3 Position;
in vec2 UV0;
in vec3 Normal;

out vec3 vs_Pos;
out vec3 vs_Nor;
out vec2 vs_UV;

uniform mat4 u_WV;
uniform mat4 u_WVP;

void main() {
    gl_Position = u_WVP * vec4(Position, 1.0);
    vs_Pos = (u_WV * vec4(Position, 1.0)).xyz;
    vs_Nor = mat3(u_WV) * Normal;
    vs_UV = UV0;
}
