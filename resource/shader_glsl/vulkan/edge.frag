#version 460

layout(location = 0) out vec4 outColor;

layout(std140, set = 1, binding = 0) uniform EdgePixelConstants {
    vec4 edgeColor;
} edgeConstants;

#include "../common/edge.frag"
