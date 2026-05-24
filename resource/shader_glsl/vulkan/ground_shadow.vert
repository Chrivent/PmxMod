#version 460

layout(location = 0) in vec3 inPosition;

layout(std140, set = 0, binding = 0) uniform GroundShadowVertexConstants {
    mat4 wvp;
} shadowConstants;

#include "../common/ground_shadow.vert"
