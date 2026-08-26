Texture2D PassColor : register(t0);
Texture2D SceneDepth : register(t1);
Texture2D SceneVelocity : register(t2);
Texture2D NeighborhoodMotion : register(t3);
SamplerState LinearClamp : register(s0);

#include "../../include/motion-blur.hlsli"

float2 ReadVelocity(float2 uv) {
    return PrepareVelocity(LoadMotionTexture(SceneVelocity, uv).xy);
}

float2 ReadDominantVelocity(float2 uv) {
    float2 tileTexelSize = InverseViewportSize * VelocityTileScale;
    float4 dominantMotion = float4(0.0, 0.0, 1.0, 0.0);
    const float2 offsets[5] = {
        float2(0.0, 0.0), float2(-0.5, 0.0), float2(0.5, 0.0),
        float2(0.0, -0.5), float2(0.0, 0.5)
    };
    for (int index = 0; index < 5; index++) {
        float4 candidate = LoadMotionTexture(
            NeighborhoodMotion, uv + offsets[index] * tileTexelSize);
        candidate.w = length(candidate.xy);
        dominantMotion = SelectDominantMotion(dominantMotion, candidate);
    }
    return dominantMotion.xy;
}

float4 ResolveDirectionalBlur(float2 uv, float passRate, int sampleRadius) {
    float4 centerColor = PassColor.SampleLevel(LinearClamp, uv, 0.0);
    if (IsMotionSceneChange())
        return centerColor;
    float2 centerVelocity = ReadVelocity(uv);
    float2 dominantVelocity = ReadDominantVelocity(uv);
    float2 blurVelocity = length(centerVelocity) > MotionEpsilon ? centerVelocity : dominantVelocity;
    if (length(blurVelocity) <= MotionEpsilon)
        return centerColor;
    float centerDepth = LoadMotionTexture(SceneDepth, uv).r;
    float4 colorSum = 0.0;
    float weightSum = 0.0;
    for (int sampleIndex = -sampleRadius; sampleIndex <= sampleRadius; sampleIndex++) {
        float sampleRate = (float)sampleIndex / max((float)sampleRadius, 1.0);
        float2 sampleUv = saturate(uv - blurVelocity * DirectionalBlurStrength * passRate * sampleRate);
        float2 sampleVelocity = ReadVelocity(sampleUv);
        float centerCoverage = CalculateMotionSegmentCoverage(
            uv, sampleUv, centerVelocity, DirectionalBlurStrength * passRate);
        float sampleCoverage = CalculateMotionSegmentCoverage(
            sampleUv, uv, sampleVelocity, DirectionalBlurStrength * passRate);
        sampleCoverage *= CalculateDirectionAlignment(blurVelocity, sampleVelocity);
        float motionCoverage = max(centerCoverage, sampleCoverage);
        if (sampleIndex == 0)
            motionCoverage = 1.0;
        float sampleDepth = LoadMotionTexture(SceneDepth, sampleUv).r;
        float depthWeight = CalculateDirectionalDepthWeight(
            centerDepth, sampleDepth, centerCoverage, sampleCoverage);
        float weight = GaussianMotionWeight(sampleRate, 0.5) * motionCoverage * depthWeight;
        colorSum += PassColor.SampleLevel(LinearClamp, sampleUv, 0.0) * weight;
        weightSum += weight;
    }
    return weightSum > MotionEpsilon ? colorSum / weightSum : centerColor;
}

float4 PSDirectionalFirst(FullscreenVertexOutput input) : SV_Target0 {
    return ResolveDirectionalBlur(input.uv, FirstDirectionalRate, DirectionalSampleRadius);
}

float4 PSDirectionalFinal(FullscreenVertexOutput input) : SV_Target0 {
    return ResolveDirectionalBlur(input.uv, FinalDirectionalRate, FinalDirectionalSampleRadius);
}
