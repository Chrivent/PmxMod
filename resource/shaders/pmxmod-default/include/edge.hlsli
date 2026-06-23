#ifndef PMXMOD_DEFAULT_EDGE_HLSLI
#define PMXMOD_DEFAULT_EDGE_HLSLI

cbuffer VSData : register(b0) {
    float4x4 wv;
    float4x4 wvp;
    float2 screenSize;
    float edgeSize;
};

cbuffer PSData : register(b1) {
    float4 edgeColor;
};

struct VSInput {
    float3 Pos : POSITION;
    float3 Nor : NORMAL;
};

struct VSOutput {
    float4 Position : SV_POSITION;
};

#endif
