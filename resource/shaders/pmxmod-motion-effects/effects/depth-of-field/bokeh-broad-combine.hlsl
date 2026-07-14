// 1/4 보케와 1/8 보케를 CoC 크기와 깊이 경계에 맞춰 합친다.
// t0 = 1/8 보케, t1 = SceneDepth, t2 = FocusHistory, t3 = 1/4 보케,
// s0 = LinearClamp.
Texture2D SceneColor : register(t0);
Texture2D SceneDepth : register(t1);
Texture2D FocusHistory : register(t2);
Texture2D QuarterBokeh : register(t3);
SamplerState LinearClamp : register(s0);

#include "../../include/depth-of-field.hlsli"

float4 ResolveEighthBokeh(float2 uv, float centerDistance) {
    float2 offset = InverseViewportSize / BokehEighthResolutionScale * 0.5;
    float2 uv0 = uv + float2(-offset.x, -offset.y);
    float2 uv1 = uv + float2(offset.x, -offset.y);
    float2 uv2 = uv + float2(-offset.x, offset.y);
    float2 uv3 = uv + float2(offset.x, offset.y);
    float4 sample0 = SceneColor.Sample(LinearClamp, uv0);
    float4 sample1 = SceneColor.Sample(LinearClamp, uv1);
    float4 sample2 = SceneColor.Sample(LinearClamp, uv2);
    float4 sample3 = SceneColor.Sample(LinearClamp, uv3);
    float weight0 = CalculateDepthAwareUpsampleWeight(centerDistance, ReadCameraDistance(uv0), sample0.a);
    float weight1 = CalculateDepthAwareUpsampleWeight(centerDistance, ReadCameraDistance(uv1), sample1.a);
    float weight2 = CalculateDepthAwareUpsampleWeight(centerDistance, ReadCameraDistance(uv2), sample2.a);
    float weight3 = CalculateDepthAwareUpsampleWeight(centerDistance, ReadCameraDistance(uv3), sample3.a);
    float totalWeight = weight0 + weight1 + weight2 + weight3;
    if (totalWeight <= BokehColorEpsilon)
        return SceneColor.Sample(LinearClamp, uv);
    return (sample0 * weight0 + sample1 * weight1 + sample2 * weight2 + sample3 * weight3) / totalWeight;
}

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    float focusDistance = ReadDelayedFocusDistance();
    float centerDistance = ReadCameraDistance(input.uv);
    float cocDistance = CalculateTiltedCameraDistance(input.uv, centerDistance, focusDistance);
    float cocPixels = CalculateCircleOfConfusionPixels(cocDistance, focusDistance);
    float4 quarterBokeh = QuarterBokeh.Sample(LinearClamp, input.uv);
    float4 eighthBokeh = ResolveEighthBokeh(input.uv, centerDistance);
    float broadBlurAmount = smoothstep(QuarterBlurPixels * 0.75, QuarterBlurPixels, abs(cocPixels));
    float broadInfluence = max(broadBlurAmount, saturate(eighthBokeh.a));
    return lerp(quarterBokeh, eighthBokeh, saturate(broadInfluence));
}
