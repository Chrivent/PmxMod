#version 450

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform GroundShadowPixelConstants {
    vec4 shadowColor;
} shadowConstants;

void main() {
    outColor = shadowConstants.shadowColor;
}
