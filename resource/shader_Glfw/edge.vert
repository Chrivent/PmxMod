#version 140

in vec3 position;
in vec3 normal;

uniform mat4 wv;
uniform mat4 wvp;
uniform vec2 screenSize;
uniform float edgeSize;

void main() {
    vec3 nor = mat3(wv) * normal;
    vec4 pos = wvp * vec4(position, 1.0);
    vec2 screenNor = normalize(vec2(nor));
    pos.xy += screenNor * vec2(1.0) / (screenSize * 0.5) * edgeSize * pos.w;
    gl_Position = pos;
}
