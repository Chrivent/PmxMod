#version 460

#ifdef VULKAN
#define SET(n) set = n,
#else
#define SET(n)
#endif

layout(location = 0) out vec4 outColor;

layout(SET(1) binding = 0) uniform GroundShadowPixelConstants {
    vec4 shadowColor;
} shadowConstants;

void main() {
    outColor = shadowConstants.shadowColor;
}
