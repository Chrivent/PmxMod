#version 450

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform EdgePixelConstants {
    vec4 edgeColor;
} edgeConstants;

void main() {
    outColor = edgeConstants.edgeColor;
}
