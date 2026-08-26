Texture2D PassColor : register(t0);
Texture2D SceneDepth : register(t1);
Texture2D SceneVelocity : register(t2);
Texture2D NeighborhoodMotion : register(t3);
SamplerState LinearClamp : register(s0);

#include "../../include/motion-blur.hlsli"

float2 ReadVelocity(float2 uv) {
    return PrepareVelocity(LoadMotionTexture(SceneVelocity, uv).xy);
}

float2 ReadLineVelocity(float2 uv) {
    float2 centerVelocity = ReadVelocity(uv);
    if (CalculateMotionSpeed(centerVelocity) > MotionEpsilon)
        return centerVelocity;
    return LoadMotionTexture(NeighborhoodMotion, uv).xy;
}

float4 PSLineReconstruction(FullscreenVertexOutput input) : SV_Target0 {
    if (IsMotionHistoryReset())
        return 0.0;
    float2 lineVelocity = ReadLineVelocity(input.uv);
    float lineSpeed = CalculateMotionSpeed(lineVelocity);
    if (lineSpeed <= MotionEpsilon)
        return 0.0;
    float centerDepth = LoadMotionTexture(SceneDepth, input.uv).r;
    float3 colorSum = 0.0;
    float weightSum = 0.0;
    float maximumCoverage = 0.0;
    float sampleJitter = CalculateMotionJitter(input.position.xy);
    for (int sampleIndex = -LineSampleRadius; sampleIndex <= LineSampleRadius; sampleIndex++) {
        float jitter = sampleIndex == 0 ? 0.0 : sampleJitter;
        float sampleRate = clamp(((float)sampleIndex + jitter) / max((float)LineSampleRadius, 1.0), -1.0, 1.0);
        float2 sampleUv = saturate(input.uv - lineVelocity * LineBlurLength * sampleRate);
        float2 sampleVelocity = ReadVelocity(sampleUv);
        float sampleSpeed = CalculateMotionSpeed(sampleVelocity);
        float alignment = CalculateDirectionAlignment(lineVelocity, sampleVelocity);
        float centerCoverage = CalculateMotionSegmentCoverage(
            input.uv, sampleUv, lineVelocity, LineBlurLength);
        float sampleCoverage = CalculateMotionSegmentCoverage(
            sampleUv, input.uv, sampleVelocity, LineBlurLength) * alignment;
        float segmentCoverage = max(centerCoverage, sampleCoverage);
        float sampleDepth = LoadMotionTexture(SceneDepth, sampleUv).r;
        float depthWeight = CalculateTrailDepthWeight(centerDepth, sampleDepth);
        float taper = pow(saturate(1.0 - abs(sampleRate)), 0.7);
        float speedWeight = saturate(sampleSpeed / max(VelocityUnderCut * 6.0, MotionEpsilon));
        float weight = segmentCoverage * depthWeight * taper * speedWeight;
        colorSum += PassColor.SampleLevel(LinearClamp, sampleUv, 0.0).rgb * weight;
        weightSum += weight;
        maximumCoverage = max(maximumCoverage, weight);
    }
    if (weightSum <= MotionEpsilon)
        return 0.0;
    float speedCoverage = saturate(lineSpeed / max(VelocityUnderCut * 8.0, MotionEpsilon));
    float coverage = saturate(maximumCoverage * speedCoverage);
    float3 lineColor = colorSum / weightSum;
    return float4(lineColor * coverage, coverage);
}
