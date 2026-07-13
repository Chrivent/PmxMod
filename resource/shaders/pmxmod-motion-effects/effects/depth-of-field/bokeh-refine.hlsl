// DOF 반해상도 중간 보케 보정 패스 입력:
// t0 = 1/4 해상도 광역 Bokeh, t1 = SceneDepth, t2 = FocusHistory,
// t3 = EffectSourceColor, s0 = LinearClamp.
Texture2D SceneColor : register(t0);
Texture2D SceneDepth : register(t1);
Texture2D FocusHistory : register(t2);
Texture2D EffectSourceColor : register(t3);
SamplerState LinearClamp : register(s0);

#include "../../include/post-process-frame.hlsli"
#include "../../include/depth-of-field.hlsli"

struct FullscreenVertexOutput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

FullscreenVertexOutput VSMain(uint vertexId : SV_VertexID) {
    FullscreenVertexOutput output;
    output.uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

// 1/4 해상도 보케를 깊이 경계를 보존하며 반해상도로 복원한다.
float4 ResolveBroadBokeh(float2 uv, float centerDistance) {
    float2 offset = InverseViewportSize / BokehQuarterResolutionScale * 0.5;
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
    float4 resolvedBokeh = SceneColor.Sample(LinearClamp, uv);
    if (totalWeight > BokehColorEpsilon)
        resolvedBokeh = (sample0 * weight0 + sample1 * weight1 + sample2 * weight2 + sample3 * weight3) / totalWeight;
    return resolvedBokeh;
}

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    float focusDistance = ReadDelayedFocusDistance();
    float centerCameraDistance = ReadCameraDistance(input.uv);
    float2 kernelRotation = CalculateBokehKernelRotation(input.position.xy);
    float3 foregroundColor = 0.0;
    float3 backgroundColor = 0.0;
    float foregroundColorWeight = 0.0;
    float backgroundColorWeight = 0.0;
    float foregroundSupport = 0.0;
    float backgroundSupport = 0.0;
    for (int index = 0; index < MediumBokehSampleCount; index++) {
        float2 sampleOffset = TransformBokehKernelOffset(MediumBokehKernel[index], kernelRotation);
        float sampleDistance = length(sampleOffset) * MediumBlurPixels;
        float2 sampleUv = input.uv + sampleOffset * InverseViewportSize * MediumBlurPixels;
        float3 sampleColor = PrepareBokehColor(EffectSourceColor.Sample(LinearClamp, sampleUv).rgb);
        float signedCocPixels = CalculateCircleOfConfusionPixels(ReadCameraDistance(sampleUv), focusDistance);
        float sampleRadius = min(abs(signedCocPixels), MediumBlurPixels);
        float support = saturate(sampleRadius - sampleDistance + 1.0);
        float brightnessWeight = CalculateCircleOfConfusionBrightness(sampleRadius);
        float centerWeight = index == 0 ? 1.5 : 1.0;
        float colorWeight = support * brightnessWeight * centerWeight * CalculateBokehHighlightWeight(sampleColor);
        float coverageWeight = support * centerWeight;
        if (signedCocPixels < 0.0) {
            foregroundColor += sampleColor * colorWeight;
            foregroundColorWeight += colorWeight;
            foregroundSupport += coverageWeight;
        } else {
            backgroundColor += sampleColor * colorWeight;
            backgroundColorWeight += colorWeight;
            backgroundSupport += coverageWeight;
        }
    }

    float3 centerColor = PrepareBokehColor(EffectSourceColor.Sample(LinearClamp, input.uv).rgb);
    float3 resolvedBackground = backgroundColorWeight > BokehColorEpsilon
        ? backgroundColor / backgroundColorWeight : centerColor;
    float3 resolvedForeground = foregroundColorWeight > BokehColorEpsilon
        ? foregroundColor / foregroundColorWeight : resolvedBackground;
    float foregroundCoverage = foregroundSupport
        / max(foregroundSupport + backgroundSupport, BokehColorEpsilon);
    float3 mediumBokeh = lerp(resolvedBackground, resolvedForeground, saturate(foregroundCoverage));
    float4 broadBokeh = ResolveBroadBokeh(input.uv, centerCameraDistance);
    float centerCocPixels = CalculateCircleOfConfusionPixels(centerCameraDistance, focusDistance);
    float broadBlurAmount = smoothstep(MediumBlurPixels - 1.0, MaxBlurPixels, abs(centerCocPixels));
    float broadInfluence = max(broadBlurAmount, saturate(broadBokeh.a));
    float3 bokehColor = lerp(mediumBokeh, broadBokeh.rgb, saturate(broadInfluence));
    return float4(bokehColor, max(saturate(foregroundCoverage), saturate(broadBokeh.a)));
}
