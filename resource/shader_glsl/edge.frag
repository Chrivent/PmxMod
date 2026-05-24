#version 460

#ifdef VULKAN
#define SET(n) set = n,
#else
#define SET(n)
#endif

layout(location = 0) out vec4 outColor;

layout(SET(1) binding = 0) uniform EdgePixelConstants {
    vec4 edgeColor;
} edgeConstants;

void main() {
    outColor = edgeConstants.edgeColor;
}
