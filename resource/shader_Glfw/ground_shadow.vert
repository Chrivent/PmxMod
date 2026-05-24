#version 140

in vec3 position;
uniform mat4 wvp;

void main() {
    gl_Position = wvp * vec4(position, 1.0);
}
