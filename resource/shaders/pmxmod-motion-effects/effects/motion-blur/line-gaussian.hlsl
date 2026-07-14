Texture2D LineInput : register(t0);
SamplerState LinearClamp : register(s0);

#include "../../include/motion-blur.hlsli"

float4 ResolveLineGaussian(float2 uv, float2 direction) {
    float4 lineSum = 0.0;
    float weightSum = 0.0;
    for (int sampleIndex = -LineGaussianSampleRadius; sampleIndex <= LineGaussianSampleRadius; sampleIndex++) {
        float weight = GaussianMotionWeight((float)sampleIndex, LineGaussianSigma);
        float2 sampleUv = saturate(uv + direction * (float)sampleIndex);
        lineSum += LineInput.SampleLevel(LinearClamp, sampleUv, 0.0) * weight;
        weightSum += weight;
    }
    return lineSum / max(weightSum, MotionEpsilon);
}

float4 PSHorizontal(FullscreenVertexOutput input) : SV_Target0 {
    return ResolveLineGaussian(input.uv, float2(InverseViewportSize.x, 0.0));
}

float4 PSVertical(FullscreenVertexOutput input) : SV_Target0 {
    return ResolveLineGaussian(input.uv, float2(0.0, InverseViewportSize.y));
}
