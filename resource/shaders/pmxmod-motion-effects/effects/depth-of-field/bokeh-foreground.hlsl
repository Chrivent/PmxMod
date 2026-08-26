// DOF 1/4 해상도 광역 전경 보케 누적 패스 입력:
// t1 = SceneDepth, t2 = FocusHistory, t3 = EffectSourceColor, s0 = LinearClamp.
Texture2D SceneDepth : register(t1);
Texture2D FocusHistory : register(t2);
Texture2D EffectSourceColor : register(t3);
SamplerState LinearClamp : register(s0);

#include "../../include/depth-of-field.hlsli"

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    float focusDistance = ReadDelayedFocusDistance();
    float3 foregroundColor = 0.0;
    float foregroundColorWeight = 0.0;
    for (int index = 0; index < BroadBokehSampleCount; index++) {
        float2 sampleOffset = TransformBokehKernelOffset(BroadBokehKernel[index]);
        float normalizedDistance = length(sampleOffset);
        float2 sampleUv = input.uv + sampleOffset * InverseViewportSize * QuarterBlurPixels;
        float sampleCameraDistance = ReadCircleOfConfusionCameraDistance(sampleUv, focusDistance);
        float signedCocPixels = CalculateCircleOfConfusionPixels(sampleCameraDistance, focusDistance);
        if (signedCocPixels < 0.0) {
            float sampleRadius = min(abs(signedCocPixels), QuarterBlurPixels);
            float sampleDistance = normalizedDistance * QuarterBlurPixels;
            float support = saturate(sampleRadius - sampleDistance + 1.0);
            float brightnessWeight = CalculateCircleOfConfusionBrightness(sampleRadius);
            float layerWeight = CalculateMiddleBokehLayerWeight(signedCocPixels);
            float centerWeight = index == 0 ? 1.5 : 1.0;
            float3 sampleColor = PrepareBokehColor(EffectSourceColor.Sample(LinearClamp, sampleUv).rgb);
            float colorWeight = support * brightnessWeight * layerWeight * centerWeight
                * CalculateBokehHighlightWeight(sampleColor);
            foregroundColor += sampleColor * colorWeight;
            foregroundColorWeight += colorWeight;
        }
    }

    return float4(foregroundColor, foregroundColorWeight);
}
