#version 460 core

layout(location = 0) in vec3 inPosition;

layout(std140, binding = 0) uniform GroundShadowVertexConstants {
    mat4 wvp;
} shadowConstants;

#include "../common/ground_shadow.vert"
