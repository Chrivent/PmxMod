#version 460

layout(location = 0) out vec4 outColor;

layout(std140, set = 1, binding = 0) uniform GroundShadowPixelConstants {
    vec4 shadowColor;
} shadowConstants;

#include "../common/ground_shadow.frag"
