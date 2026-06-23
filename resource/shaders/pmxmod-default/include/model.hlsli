#ifndef PMXMOD_DEFAULT_MODEL_HLSLI
#define PMXMOD_DEFAULT_MODEL_HLSLI

cbuffer VSData : register(b0) {
    float4x4 wv;
    float4x4 wvp;
};

cbuffer PSData : register(b1) {
    float4 texMulFactor;
    float4 texAddFactor;
    float4 toonTexMulFactor;
    float4 toonTexAddFactor;
    float4 sphereTexMulFactor;
    float4 sphereTexAddFactor;
    int4 textureModes;
    float4 diffuseAlpha;
    float4 ambientSpecularPower;
    float4 specular;
    float4 lightColor;
    float4 lightDir;
};

Texture2D tex : register(t0);
Texture2D toonTex : register(t1);
Texture2D sphereTex : register(t2);
SamplerState texSampler : register(s0);
SamplerState toonTexSampler : register(s1);
SamplerState sphereTexSampler : register(s2);

struct VSInput {
    float3 Pos : POSITION;
    float3 Nor : NORMAL;
    float2 UV : UV;
};

struct VSOutput {
    float4 Position : SV_POSITION;
    float3 Pos : VIEWPOS;
    float3 Nor : VIEWNORMAL;
    float2 UV : UV;
};

#endif
