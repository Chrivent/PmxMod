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

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    static const float2 offsets[16] = {
        float2(0.0000, 0.0000),
        float2(0.3827, 0.9239),
        float2(-0.7071, 0.7071),
        float2(-0.9239, -0.3827),
        float2(0.0000, -1.0000),
        float2(0.9239, -0.3827),
        float2(0.7071, 0.7071),
        float2(-0.3827, 0.9239),
        float2(0.7500, 0.0000),
        float2(0.5303, 0.5303),
        float2(0.0000, 0.7500),
        float2(-0.5303, 0.5303),
        float2(-0.7500, 0.0000),
        float2(-0.5303, -0.5303),
        float2(0.0000, -0.7500),
        float2(0.5303, -0.5303)
    };

    float focusDistance = ReadDelayedFocusDistance();
    float3 foregroundColor = 0.0;
    float3 backgroundColor = 0.0;
    float foregroundColorWeight = 0.0;
    float backgroundColorWeight = 0.0;
    float foregroundSupport = 0.0;
    float backgroundSupport = 0.0;
    for (int index = 0; index < 16; index++) {
        float sampleDistance = length(offsets[index]) * MediumBlurPixels;
        float2 sampleUv = input.uv + offsets[index] * InverseViewportSize * MediumBlurPixels;
        float3 sampleColor = PrepareBokehColor(EffectSourceColor.Sample(LinearClamp, sampleUv).rgb);
        float signedCocPixels = CalculateCircleOfConfusionPixels(ReadCameraDistance(sampleUv), focusDistance);
        float sampleRadius = min(abs(signedCocPixels), MediumBlurPixels);
        float support = saturate(sampleRadius - sampleDistance + 1.0);
        float brightnessWeight = CalculateCircleOfConfusionBrightness(sampleRadius);
        float centerWeight = index == 0 ? 1.5 : 1.0;
        float colorWeight = support * brightnessWeight * centerWeight;
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
    float4 broadBokeh = SceneColor.Sample(LinearClamp, input.uv);
    float centerCocPixels = CalculateCircleOfConfusionPixels(ReadCameraDistance(input.uv), focusDistance);
    float broadBlurAmount = smoothstep(MediumBlurPixels - 1.0, MaxBlurPixels, abs(centerCocPixels));
    float broadInfluence = max(broadBlurAmount, saturate(broadBokeh.a));
    float3 bokehColor = lerp(mediumBokeh, broadBokeh.rgb, saturate(broadInfluence));
    return float4(bokehColor, max(saturate(foregroundCoverage), saturate(broadBokeh.a)));
}
