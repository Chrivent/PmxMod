#version 140

out vec4 out_Color;
uniform vec4 shadowColor;

void main() {
    out_Color = shadowColor;
}
