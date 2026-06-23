#include "../../include/ground-shadow.hlsli"

VSOutput VSMain(VSInput input) {
    VSOutput vsOut;
    vsOut.Position = mul(wvp, float4(input.Pos, 1.0));
    return vsOut;
}

float4 PSMain(VSOutput vsOut) : SV_TARGET0 {
    return shadowColor;
}
