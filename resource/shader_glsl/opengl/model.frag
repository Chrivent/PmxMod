#version 460 core

layout(location = 0) in vec3 vsPos;
layout(location = 1) in vec3 vsNor;
layout(location = 2) in vec2 vsUv;

layout(location = 0) out vec4 outColor;

layout(std140, binding = 1) uniform ModelPixelConstants {
    vec4 diffuseAlpha;
    vec4 ambientSpecularPower;
    vec4 specular;
    vec4 lightColor;
    vec4 lightDir;
    vec4 texMulFactor;
    vec4 texAddFactor;
    vec4 toonTexMulFactor;
    vec4 toonTexAddFactor;
    vec4 sphereTexMulFactor;
    vec4 sphereTexAddFactor;
    ivec4 textureModes;
} pixelConstants;

layout(binding = 0) uniform sampler2D tex;
layout(binding = 1) uniform sampler2D toonTex;
layout(binding = 2) uniform sampler2D sphereTex;

#include "../common/model.frag"
