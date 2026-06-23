#ifdef PMX_SPIRV
#define PMX_LOCATION(index) [[vk::location(index)]]
cbuffer VSData : register(b0, space0) {
#else
#define PMX_LOCATION(index)
cbuffer VSData : register(b0) {
#endif
    float4x4 wvp;
};

#ifdef PMX_SPIRV
cbuffer PSData : register(b0, space1) {
#else
cbuffer PSData : register(b1) {
#endif
    float4 shadowColor;
};

struct VSInput {
    PMX_LOCATION(0) float3 Pos : POSITION;
};

struct VSOutput {
    float4 Position : SV_POSITION;
};

VSOutput VSMain(VSInput input) {
    VSOutput vsOut;
    vsOut.Position = mul(wvp, float4(input.Pos, 1.0));
    return vsOut;
}

float4 PSMain(VSOutput vsOut) : SV_TARGET0 {
    return shadowColor;
}
