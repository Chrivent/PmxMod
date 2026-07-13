// DOF 최종 합성 패스 입력:
// t0 = 반해상도 Bokeh, t1 = SceneDepth, t2 = FocusHistory, t3 = EffectSourceColor, s0 = LinearClamp.
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

// 반해상도 보케를 3x3 깊이 인식 필터로 원해상도에 복원한다.
float4 ResolveDepthAwareBokeh(float2 uv, float centerDistance) {
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
    float4 weightedBokeh = 0.0;
    float totalWeight = 0.0;
    for (int index = 0; index < 9; index++) {
        float2 sampleUv = uv + offsets[index] * halfResolutionTexelSize;
        float4 sampleBokeh = SceneColor.Sample(LinearClamp, sampleUv);
        float depthWeight = CalculateDepthAwareUpsampleWeight(centerDistance, ReadCameraDistance(sampleUv), sampleBokeh.a);
        float weight = spatialWeights[index] * depthWeight;
        weightedBokeh += sampleBokeh * weight;
        totalWeight += weight;
    }
    return totalWeight > BokehColorEpsilon
        ? weightedBokeh / totalWeight : SceneColor.Sample(LinearClamp, uv);
}

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    float4 sourceColor = EffectSourceColor.Sample(LinearClamp, input.uv);
    float focusDistance = ReadDelayedFocusDistance();
    float cameraDistance = ReadCameraDistance(input.uv);
    float4 bokeh = ResolveDepthAwareBokeh(input.uv, cameraDistance);
    float signedCocPixels = CalculateCircleOfConfusionPixels(cameraDistance, focusDistance);
    float localBlur = smoothstep(0.0, 1.0, abs(signedCocPixels));
    float foregroundSpread = saturate(bokeh.a);
    float blendAmount = max(localBlur, foregroundSpread);
    float3 resolvedBokehColor = ResolveBokehColor(bokeh.rgb);
    return float4(lerp(sourceColor.rgb, resolvedBokehColor, blendAmount), sourceColor.a);
}
