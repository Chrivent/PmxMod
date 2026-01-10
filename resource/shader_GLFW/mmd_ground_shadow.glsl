#version 140

#ifdef VERTEX

in vec3 in_Pos;
uniform mat4 u_WVP;

void main() {
    gl_Position = u_WVP * vec4(in_Pos, 1.0);
}

#endif

#ifdef FRAGMENT

out vec4 out_Color;
uniform vec4 u_ShadowColor;

void main() {
    out_Color = u_ShadowColor;
}

#endif
