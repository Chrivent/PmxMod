cbuffer VSData : register(b0) {
    float4x4 wv;
    float4x4 wvp;
    float2 screenSize;
};

cbuffer VSEdgeData : register(b1) {
    float edgeSize;
};

cbuffer PSData : register(b2) {
    float4 edgeColor;
};

struct VSInput {
    float3 Pos : POSITION;
    float3 Nor : NORMAL;
};

struct VSOutput {
    float4 Position : SV_POSITION;
};

VSOutput VSMain(VSInput input) {
    VSOutput vsOut;
    float3 nor = mul((float3x3)wv, input.Nor);
    float4 pos = mul(wvp, float4(input.Pos, 1.0));
    float2 screenNor = normalize((float2)nor);
    pos.xy += screenNor * float2(1.0, 1.0) / (screenSize * 0.5) * edgeSize * pos.w;
    vsOut.Position = pos;
    return vsOut;
}

float4 PSMain(VSOutput vsOut) : SV_TARGET0 {
    return edgeColor;
}
