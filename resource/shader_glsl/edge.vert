#version 460

#ifdef VULKAN
#define SET(n) set = n,
#else
#define SET(n)
#endif

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(SET(0) binding = 0) uniform EdgeVertexConstants {
    mat4 wv;
    mat4 wvp;
    vec2 screenSize;
    float edgeSize;
} edgeConstants;

void main() {
    vec3 nor = mat3(edgeConstants.wv) * inNormal;
    vec4 pos = edgeConstants.wvp * vec4(inPosition, 1.0);
    vec2 screenNor = normalize(vec2(nor));
    pos.xy += screenNor * vec2(1.0) / (edgeConstants.screenSize * 0.5) * edgeConstants.edgeSize * pos.w;
    gl_Position = pos;
}
