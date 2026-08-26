// DOF 최종 합성 패스 입력:
// t0 = 원해상도 누적 Bokeh, t1 = SceneDepth, t2 = FocusHistory,
// t3 = EffectSourceColor, s0 = LinearClamp.
Texture2D SceneColor : register(t0);
Texture2D SceneDepth : register(t1);
Texture2D FocusHistory : register(t2);
Texture2D EffectSourceColor : register(t3);
SamplerState LinearClamp : register(s0);

#include "../../include/depth-of-field.hlsli"

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    float4 sourceColor = EffectSourceColor.Sample(LinearClamp, input.uv);
    float focusDistance = ReadDelayedFocusDistance();
    float cameraDistance = ReadCameraDistance(input.uv);
    float4 weightedBokeh = SceneColor.Sample(LinearClamp, input.uv);
    float cocCameraDistance = CalculateTiltedCameraDistance(input.uv, cameraDistance, focusDistance);
    float signedCocPixels = CalculateCircleOfConfusionPixels(cocCameraDistance, focusDistance);
    float gatherCocPixels = max(signedCocPixels - FocusBokehMarginPixels, -signedCocPixels);
    float sourceWeight = CalculateCircleOfConfusionBrightness(gatherCocPixels) + BokehWeightCompensation;
    float3 preparedSourceColor = PrepareBokehColor(sourceColor.rgb);
    float totalWeight = sourceWeight + weightedBokeh.a;
    float3 resolvedColor = (preparedSourceColor * sourceWeight + weightedBokeh.rgb)
        / max(totalWeight, BokehColorEpsilon);
    return float4(ResolveBokehColor(resolvedColor), sourceColor.a);
}
