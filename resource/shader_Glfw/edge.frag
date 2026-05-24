#version 140

out vec4 out_Color;
uniform vec4 edgeColor;

void main() {
    out_Color = edgeColor;
}
