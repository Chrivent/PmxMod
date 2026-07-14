// DOF 1/4 해상도 광역 후경 보케 누적 패스 입력:
// t0 = CoC가 포함된 1/4 해상도 색상, t1 = SceneDepth, t2 = FocusHistory,
// s0 = LinearClamp.
Texture2D SceneColor : register(t0);
Texture2D SceneDepth : register(t1);
Texture2D FocusHistory : register(t2);
SamplerState LinearClamp : register(s0);

#include "../../include/depth-of-field.hlsli"

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    float2 quarterResolutionTexelSize = InverseViewportSize / BokehQuarterResolutionScale;
    float quarterResolutionMaxRadius = MaxBlurPixels * BokehQuarterResolutionScale;
    float2 kernelRotation = CalculateBokehKernelRotation(input.position.xy);
    float3 backgroundColor = 0.0;
    float backgroundColorWeight = 0.0;
    for (int index = 0; index < BroadBokehSampleCount; index++) {
        float2 sampleOffset = TransformBokehKernelOffset(BroadBokehKernel[index], kernelRotation);
        float normalizedDistance = length(sampleOffset);
        float2 sampleUv = input.uv + sampleOffset * quarterResolutionTexelSize * quarterResolutionMaxRadius;
        float4 sampleData = SceneColor.Sample(LinearClamp, sampleUv);
        float signedCocPixels = DecodeCircleOfConfusion(sampleData.a);
        float sampleRadius = abs(signedCocPixels) * BokehQuarterResolutionScale;
        float sampleDistance = normalizedDistance * quarterResolutionMaxRadius;
        float support = saturate(sampleRadius - sampleDistance + 1.0);
        float brightnessWeight = CalculateCircleOfConfusionBrightness(sampleRadius);
        float centerWeight = index == 0 ? 1.5 : 1.0;
        float colorWeight = support * brightnessWeight * centerWeight * CalculateBokehHighlightWeight(sampleData.rgb);
        if (signedCocPixels >= 0.0) {
            backgroundColor += sampleData.rgb * colorWeight;
            backgroundColorWeight += colorWeight;
        }
    }

    float3 centerColor = SceneColor.Sample(LinearClamp, input.uv).rgb;
    float3 resolvedBackground = backgroundColorWeight > BokehColorEpsilon
        ? backgroundColor / backgroundColorWeight : centerColor;
    return float4(resolvedBackground, 0.0);
}
