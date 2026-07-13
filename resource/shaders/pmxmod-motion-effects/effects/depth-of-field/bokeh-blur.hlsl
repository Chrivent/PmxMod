// DOF 전경/후경 보케 분리 패스 입력:
// t0 = CoC가 포함된 반해상도 색상, t1 = SceneDepth, t2 = FocusHistory,
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
    static const float2 offsets[24] = {
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
        float2(0.5303, -0.5303),
        float2(1.2500, 0.0000),
        float2(0.8839, 0.8839),
        float2(0.0000, 1.2500),
        float2(-0.8839, 0.8839),
        float2(-1.2500, 0.0000),
        float2(-0.8839, -0.8839),
        float2(0.0000, -1.2500),
        float2(0.8839, -0.8839)
    };

    float2 halfResolutionTexelSize = InverseViewportSize / BokehDownsampleScale;
    float halfResolutionMaxRadius = MaxBlurPixels * BokehDownsampleScale;
    float3 foregroundColor = 0.0;
    float3 backgroundColor = 0.0;
    float foregroundColorWeight = 0.0;
    float backgroundColorWeight = 0.0;
    float foregroundSupport = 0.0;
    float backgroundSupport = 0.0;
    for (int index = 0; index < 24; index++) {
        float normalizedDistance = length(offsets[index]) / 1.25;
        float2 sampleUv = input.uv + offsets[index] / 1.25 * halfResolutionTexelSize * halfResolutionMaxRadius;
        float4 sampleData = SceneColor.Sample(LinearClamp, sampleUv);
        float signedCocPixels = DecodeCircleOfConfusion(sampleData.a);
        float sampleRadius = abs(signedCocPixels) * BokehDownsampleScale;
        float sampleDistance = normalizedDistance * halfResolutionMaxRadius;
        float support = saturate(sampleRadius - sampleDistance + 1.0);
        float brightnessWeight = CalculateCircleOfConfusionBrightness(sampleRadius);
        float centerWeight = index == 0 ? 1.5 : 1.0;
        float colorWeight = support * brightnessWeight * centerWeight;
        float coverageWeight = support * centerWeight;
        if (signedCocPixels < 0.0) {
            foregroundColor += sampleData.rgb * colorWeight;
            foregroundColorWeight += colorWeight;
            foregroundSupport += coverageWeight;
        } else {
            backgroundColor += sampleData.rgb * colorWeight;
            backgroundColorWeight += colorWeight;
            backgroundSupport += coverageWeight;
        }
    }

    float3 centerColor = SceneColor.Sample(LinearClamp, input.uv).rgb;
    float3 resolvedBackground = backgroundColorWeight > 0.0001
        ? backgroundColor / backgroundColorWeight : centerColor;
    float3 resolvedForeground = foregroundColorWeight > 0.0001
        ? foregroundColor / foregroundColorWeight : resolvedBackground;
    float foregroundCoverage = foregroundSupport
        / max(foregroundSupport + backgroundSupport, 0.0001);
    float3 bokehColor = lerp(resolvedBackground, resolvedForeground, saturate(foregroundCoverage));
    return float4(bokehColor, saturate(foregroundCoverage));
}
