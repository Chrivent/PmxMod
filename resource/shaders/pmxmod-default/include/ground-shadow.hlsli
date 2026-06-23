#ifndef PMXMOD_DEFAULT_GROUND_SHADOW_HLSLI
#define PMXMOD_DEFAULT_GROUND_SHADOW_HLSLI

cbuffer VSData : register(b0) {
    float4x4 wvp;
};

cbuffer PSData : register(b1) {
    float4 shadowColor;
};

struct VSInput {
    float3 Pos : POSITION;
};

struct VSOutput {
    float4 Position : SV_POSITION;
};

#endif
