// DOF CoC 축소 패스 입력:
// t0 = SceneColor, t1 = SceneDepth, t2 = FocusHistory, t3 = EffectSourceColor, s0 = LinearClamp.
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

float ResolveStrongerCircleOfConfusion(float currentCoc, float candidateCoc) {
    return abs(candidateCoc) > abs(currentCoc) ? candidateCoc : currentCoc;
}

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    float2 offset = InverseViewportSize * BokehHalfResolutionScale;
    float2 uv0 = input.uv + float2(-offset.x, -offset.y);
    float2 uv1 = input.uv + float2(offset.x, -offset.y);
    float2 uv2 = input.uv + float2(-offset.x, offset.y);
    float2 uv3 = input.uv + float2(offset.x, offset.y);
    float3 color0 = SceneColor.Sample(LinearClamp, uv0).rgb;
    float3 color1 = SceneColor.Sample(LinearClamp, uv1).rgb;
    float3 color2 = SceneColor.Sample(LinearClamp, uv2).rgb;
    float3 color3 = SceneColor.Sample(LinearClamp, uv3).rgb;
    float focusDistance = ReadDelayedFocusDistance();
    float coc0 = CalculateCircleOfConfusionPixels(ReadCameraDistance(uv0), focusDistance);
    float coc1 = CalculateCircleOfConfusionPixels(ReadCameraDistance(uv1), focusDistance);
    float coc2 = CalculateCircleOfConfusionPixels(ReadCameraDistance(uv2), focusDistance);
    float coc3 = CalculateCircleOfConfusionPixels(ReadCameraDistance(uv3), focusDistance);
    float weight0 = abs(coc0);
    float weight1 = abs(coc1);
    float weight2 = abs(coc2);
    float weight3 = abs(coc3);
    float totalWeight = weight0 + weight1 + weight2 + weight3;
    float3 prepared0 = PrepareBokehColor(color0);
    float3 prepared1 = PrepareBokehColor(color1);
    float3 prepared2 = PrepareBokehColor(color2);
    float3 prepared3 = PrepareBokehColor(color3);
    float3 color = totalWeight > BokehColorEpsilon
        ? (prepared0 * weight0 + prepared1 * weight1 + prepared2 * weight2 + prepared3 * weight3) / totalWeight
        : (prepared0 + prepared1 + prepared2 + prepared3) * 0.25;
    float signedCoc = ResolveStrongerCircleOfConfusion(coc0, coc1);
    signedCoc = ResolveStrongerCircleOfConfusion(signedCoc, coc2);
    signedCoc = ResolveStrongerCircleOfConfusion(signedCoc, coc3);
    return float4(color, EncodeCircleOfConfusion(signedCoc));
}
