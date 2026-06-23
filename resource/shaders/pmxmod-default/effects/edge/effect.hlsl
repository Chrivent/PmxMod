#ifdef PMX_SPIRV
#define PMX_LOCATION(index) [[vk::location(index)]]
cbuffer VSData : register(b0, space0) {
    float4x4 wv;
    float4x4 wvp;
    float2 screenSize;
    float edgeSize;
};
#else
#define PMX_LOCATION(index)
cbuffer VSData : register(b0) {
    float4x4 wv;
    float4x4 wvp;
    float2 screenSize;
};

cbuffer VSEdgeData : register(b1) {
    float edgeSize;
};
#endif

#ifdef PMX_SPIRV
cbuffer PSData : register(b0, space1) {
#else
cbuffer PSData : register(b2) {
#endif
    float4 edgeColor;
};

struct VSInput {
    PMX_LOCATION(0) float3 Pos : POSITION;
    PMX_LOCATION(1) float3 Nor : NORMAL;
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
