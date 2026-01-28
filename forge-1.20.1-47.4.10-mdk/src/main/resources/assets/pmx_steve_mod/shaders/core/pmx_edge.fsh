#version 150

out vec4 fragColor;

uniform vec4 u_EdgeColor;

void main() {
    fragColor = u_EdgeColor;
}
