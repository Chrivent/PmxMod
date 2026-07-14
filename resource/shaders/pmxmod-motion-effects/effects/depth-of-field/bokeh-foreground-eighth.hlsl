// 1/8 해상도에서 넓은 전경 보케를 누적하고 후경 보케와 합성한다.
// t0 = 1/8 후경 보케, t1 = SceneDepth, t2 = FocusHistory,
// t3 = EffectSourceColor, s0 = LinearClamp.
Texture2D SceneColor : register(t0);
Texture2D SceneDepth : register(t1);
Texture2D FocusHistory : register(t2);
Texture2D EffectSourceColor : register(t3);
SamplerState LinearClamp : register(s0);

#include "../../include/depth-of-field.hlsli"

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    float focusDistance = ReadDelayedFocusDistance();
    float2 kernelRotation = CalculateBokehKernelRotation(input.position.xy);
    float3 foregroundColor = 0.0;
    float foregroundColorWeight = 0.0;
    float foregroundSupport = 0.0;
    for (int index = 0; index < BroadBokehSampleCount; index++) {
        float2 sampleOffset = TransformBokehKernelOffset(BroadBokehKernel[index], kernelRotation);
        float normalizedDistance = length(sampleOffset);
        float2 sampleUv = input.uv + sampleOffset * InverseViewportSize * MaxBlurPixels;
        float sampleCameraDistance = ReadCircleOfConfusionCameraDistance(sampleUv, focusDistance);
        float signedCocPixels = CalculateCircleOfConfusionPixels(sampleCameraDistance, focusDistance);
        if (signedCocPixels < 0.0) {
            float sampleRadius = abs(signedCocPixels);
            float sampleDistance = normalizedDistance * MaxBlurPixels;
            float support = saturate(sampleRadius - sampleDistance + 1.0);
            float brightnessWeight = CalculateCircleOfConfusionBrightness(sampleRadius);
            float centerWeight = index == 0 ? 1.5 : 1.0;
            float3 sampleColor = PrepareBokehColor(EffectSourceColor.Sample(LinearClamp, sampleUv).rgb);
            float colorWeight = support * brightnessWeight * centerWeight
                * CalculateBokehHighlightWeight(sampleColor);
            foregroundColor += sampleColor * colorWeight;
            foregroundColorWeight += colorWeight;
            foregroundSupport += support * centerWeight;
        }
    }

    float4 backgroundBokeh = SceneColor.Sample(LinearClamp, input.uv);
    float3 resolvedForeground = foregroundColorWeight > BokehColorEpsilon
        ? foregroundColor / foregroundColorWeight : backgroundBokeh.rgb;
    float foregroundCoverage = saturate(foregroundSupport / ForegroundCoverageNormalization);
    float3 bokehColor = lerp(backgroundBokeh.rgb, resolvedForeground, foregroundCoverage);
    return float4(bokehColor, foregroundCoverage);
}
