#version 460 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(std140, binding = 0) uniform EdgeVertexConstants {
    mat4 wv;
    mat4 wvp;
    vec2 screenSize;
    float edgeSize;
} edgeConstants;

#include "../common/edge.vert"
