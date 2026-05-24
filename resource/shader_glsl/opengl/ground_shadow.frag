#version 460 core

layout(location = 0) out vec4 outColor;

layout(std140, binding = 1) uniform GroundShadowPixelConstants {
    vec4 shadowColor;
} shadowConstants;

#include "../common/ground_shadow.frag"
