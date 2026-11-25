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
    float EdgeScale : TEXCOORD0;
};

struct VSOutput {
    float4 Position : SV_POSITION;
};

VSOutput VSMain(VSInput input) {
    VSOutput vsOut;
    float3 nor = mul((float3x3)wv, input.Nor);
    float4 pos = mul(wvp, float4(input.Pos, 1.0));
    float2 screenNor = nor.xy * rsqrt(max(dot(nor.xy, nor.xy), 1e-12));
    pos.xy += screenNor / (screenSize * 0.5) * (edgeSize * input.EdgeScale) * pos.w;
    vsOut.Position = pos;
    return vsOut;
}

float4 PSMain(VSOutput vsOut) : SV_TARGET0 {
    return edgeColor;
}
