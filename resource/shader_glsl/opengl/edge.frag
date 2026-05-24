#version 460 core

layout(location = 0) out vec4 outColor;

layout(std140, binding = 1) uniform EdgePixelConstants {
    vec4 edgeColor;
} edgeConstants;

#include "../common/edge.frag"
