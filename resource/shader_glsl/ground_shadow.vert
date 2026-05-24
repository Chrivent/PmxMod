#version 460

#ifdef VULKAN
#define SET(n) set = n,
#else
#define SET(n)
#endif

layout(location = 0) in vec3 inPosition;

layout(SET(0) binding = 0) uniform GroundShadowVertexConstants {
    mat4 wvp;
} shadowConstants;

void main() {
    gl_Position = shadowConstants.wvp * vec4(inPosition, 1.0);
}
