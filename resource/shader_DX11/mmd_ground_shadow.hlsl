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

VSOutput VSMain(VSInput input) {
    VSOutput vsOut;
    vsOut.Position = mul(wvp, float4(input.Pos, 1.0));
    return vsOut;
}

float4 PSMain(VSOutput vsOut) : SV_TARGET0 {
    return shadowColor;
}
