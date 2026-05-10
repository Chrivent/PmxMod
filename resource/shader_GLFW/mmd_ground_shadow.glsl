#version 140

#ifdef VERTEX

in vec3 position;
uniform mat4 wvp;

void main() {
    gl_Position = wvp * vec4(position, 1.0);
}

#endif

#ifdef FRAGMENT

out vec4 out_Color;
uniform vec4 shadowColor;

void main() {
    out_Color = shadowColor;
}

#endif
