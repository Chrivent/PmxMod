#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec3 vsPos;
layout(location = 1) out vec3 vsNor;
layout(location = 2) out vec2 vsUv;

layout(std140, set = 0, binding = 0) uniform ModelVertexConstants {
    mat4 wv;
    mat4 wvp;
} vertexConstants;

#include "../common/model.vert"
