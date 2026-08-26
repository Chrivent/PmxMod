// 분리된 반해상도 전경·후경 보케를 원해상도에서 합치는 패스 입력:
// t0 = BackgroundBokeh, t1 = SceneDepth, t2 = FocusHistory,
// t3 = ForegroundBokeh, s0 = LinearClamp.
Texture2D BackgroundBokeh : register(t0);
Texture2D SceneDepth : register(t1);
Texture2D FocusHistory : register(t2);
Texture2D ForegroundBokeh : register(t3);
SamplerState LinearClamp : register(s0);

#include "../../include/depth-of-field.hlsli"

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    static const float2 offsets[9] = {
        float2(-1.0, -1.0),
        float2(0.0, -1.0),
        float2(1.0, -1.0),
        float2(-1.0, 0.0),
        float2(0.0, 0.0),
        float2(1.0, 0.0),
        float2(-1.0, 1.0),
        float2(0.0, 1.0),
        float2(1.0, 1.0)
    };
    static const float spatialWeights[9] = { 1.0, 2.0, 1.0, 2.0, 4.0, 2.0, 1.0, 2.0, 1.0 };
    float2 halfResolutionTexelSize = InverseViewportSize / BokehHalfResolutionScale;
    float4 backgroundBokeh = 0.0;
    float4 foregroundBokeh = 0.0;
    float totalSpatialWeight = 0.0;
    for (int index = 0; index < 9; index++) {
        float2 sampleUv = input.uv + offsets[index] * halfResolutionTexelSize;
        float weight = spatialWeights[index];
        backgroundBokeh += BackgroundBokeh.Sample(LinearClamp, sampleUv) * weight;
        foregroundBokeh += ForegroundBokeh.Sample(LinearClamp, sampleUv) * weight;
        totalSpatialWeight += weight;
    }
    backgroundBokeh /= totalSpatialWeight;
    foregroundBokeh /= totalSpatialWeight;

    float focusDistance = ReadDelayedFocusDistance();
    float cameraDistance = ReadCircleOfConfusionCameraDistance(input.uv, focusDistance);
    float signedCocPixels = CalculateCircleOfConfusionPixels(cameraDistance, focusDistance);
    float backgroundMask = saturate(signedCocPixels - FocusBokehMarginPixels);
    return backgroundBokeh * backgroundMask + foregroundBokeh * ForegroundBokehWeightScale;
}
