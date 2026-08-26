// DOF CoC 축소 패스 입력:
// t0 = SceneColor, t1 = SceneDepth, t2 = FocusHistory, s0 = LinearClamp.
Texture2D SceneColor : register(t0);
Texture2D SceneDepth : register(t1);
Texture2D FocusHistory : register(t2);
SamplerState LinearClamp : register(s0);

#include "../../include/depth-of-field.hlsli"

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
    float cocDistance0 = ReadCircleOfConfusionCameraDistance(uv0, focusDistance);
    float cocDistance1 = ReadCircleOfConfusionCameraDistance(uv1, focusDistance);
    float cocDistance2 = ReadCircleOfConfusionCameraDistance(uv2, focusDistance);
    float cocDistance3 = ReadCircleOfConfusionCameraDistance(uv3, focusDistance);
    float coc0 = CalculateCircleOfConfusionPixels(cocDistance0, focusDistance);
    float coc1 = CalculateCircleOfConfusionPixels(cocDistance1, focusDistance);
    float coc2 = CalculateCircleOfConfusionPixels(cocDistance2, focusDistance);
    float coc3 = CalculateCircleOfConfusionPixels(cocDistance3, focusDistance);
    float3 prepared0 = PrepareBokehColor(color0);
    float3 prepared1 = PrepareBokehColor(color1);
    float3 prepared2 = PrepareBokehColor(color2);
    float3 prepared3 = PrepareBokehColor(color3);
    float signedCoc = ResolveDownsampledCircleOfConfusion(coc0, coc1, coc2, coc3);
    float weight0 = CalculateCircleOfConfusionDownsampleWeight(coc0);
    float weight1 = CalculateCircleOfConfusionDownsampleWeight(coc1);
    float weight2 = CalculateCircleOfConfusionDownsampleWeight(coc2);
    float weight3 = CalculateCircleOfConfusionDownsampleWeight(coc3);
    float totalWeight = weight0 + weight1 + weight2 + weight3;
    float3 color = totalWeight > BokehColorEpsilon
        ? (prepared0 * weight0 + prepared1 * weight1 + prepared2 * weight2 + prepared3 * weight3) / totalWeight
        : (prepared0 + prepared1 + prepared2 + prepared3) * 0.25;
    return float4(color, EncodeCircleOfConfusion(signedCoc));
}
