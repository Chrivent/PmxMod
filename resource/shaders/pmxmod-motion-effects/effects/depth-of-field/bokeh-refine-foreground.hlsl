// DOF 반해상도 중간 전경 보케 보정 패스 입력:
// t0 = 1/4 해상도 광역 전경 Bokeh, t1 = SceneDepth, t2 = FocusHistory,
// t3 = EffectSourceColor, s0 = LinearClamp.
Texture2D SceneColor : register(t0);
Texture2D SceneDepth : register(t1);
Texture2D FocusHistory : register(t2);
Texture2D EffectSourceColor : register(t3);
SamplerState LinearClamp : register(s0);

#include "../../include/depth-of-field.hlsli"

// 1/4 해상도의 누적 색상과 가중치를 반해상도로 복원한다.
float4 ResolveBroadBokeh(float2 uv) {
    float2 offset = InverseViewportSize / BokehQuarterResolutionScale * 0.5;
    float2 uv0 = uv + float2(-offset.x, -offset.y);
    float2 uv1 = uv + float2(offset.x, -offset.y);
    float2 uv2 = uv + float2(-offset.x, offset.y);
    float2 uv3 = uv + float2(offset.x, offset.y);
    float4 sample0 = SceneColor.Sample(LinearClamp, uv0);
    float4 sample1 = SceneColor.Sample(LinearClamp, uv1);
    float4 sample2 = SceneColor.Sample(LinearClamp, uv2);
    float4 sample3 = SceneColor.Sample(LinearClamp, uv3);
    return (sample0 + sample1 + sample2 + sample3) * 0.25;
}

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    float focusDistance = ReadDelayedFocusDistance();
    float3 foregroundColor = 0.0;
    float foregroundColorWeight = 0.0;
    for (int index = 0; index < MediumBokehSampleCount; index++) {
        float2 sampleOffset = TransformBokehKernelOffset(MediumBokehKernel[index]);
        float sampleDistance = length(sampleOffset) * MediumBlurPixels;
        float2 sampleUv = input.uv + sampleOffset * InverseViewportSize * MediumBlurPixels;
        float3 sampleColor = PrepareBokehColor(EffectSourceColor.Sample(LinearClamp, sampleUv).rgb);
        float sampleCameraDistance = ReadCircleOfConfusionCameraDistance(sampleUv, focusDistance);
        float signedCocPixels = CalculateCircleOfConfusionPixels(sampleCameraDistance, focusDistance);
        float sampleRadius = min(abs(signedCocPixels), MediumBlurPixels);
        float support = saturate(sampleRadius - sampleDistance + 1.0);
        float brightnessWeight = CalculateCircleOfConfusionBrightness(sampleRadius);
        float layerWeight = CalculateFirstBokehLayerWeight(signedCocPixels);
        float centerWeight = index == 0 ? 1.5 : 1.0;
        float colorWeight = support * brightnessWeight * layerWeight * centerWeight
            * CalculateBokehHighlightWeight(sampleColor);
        if (signedCocPixels < 0.0) {
            foregroundColor += sampleColor * colorWeight;
            foregroundColorWeight += colorWeight;
        }
    }

    float4 mediumBokeh = float4(foregroundColor, foregroundColorWeight);
    float4 broadBokeh = ResolveBroadBokeh(input.uv);
    return mediumBokeh + broadBokeh;
}
