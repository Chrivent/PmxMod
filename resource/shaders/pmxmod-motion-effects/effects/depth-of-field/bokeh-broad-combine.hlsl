// 1/4 보케와 1/8 보케의 누적 색상과 가중치를 합친다.
// t0 = 1/8 보케, t1 = SceneDepth, t2 = FocusHistory, t3 = 1/4 보케,
// s0 = LinearClamp.
Texture2D SceneColor : register(t0);
Texture2D SceneDepth : register(t1);
Texture2D FocusHistory : register(t2);
Texture2D QuarterBokeh : register(t3);
SamplerState LinearClamp : register(s0);

#include "../../include/depth-of-field.hlsli"

float4 ResolveEighthBokeh(float2 uv) {
    float2 offset = InverseViewportSize / BokehEighthResolutionScale * 0.5;
    float2 uv0 = uv + float2(-offset.x, -offset.y);
    float2 uv1 = uv + float2(offset.x, -offset.y);
    float2 uv2 = uv + float2(-offset.x, offset.y);
    float2 uv3 = uv + float2(offset.x, offset.y);
    float4 sample0 = SceneColor.Sample(LinearClamp, uv0);
    float4 sample1 = SceneColor.Sample(LinearClamp, uv1);
    float4 sample2 = SceneColor.Sample(LinearClamp, uv2);
    float4 sample3 = SceneColor.Sample(LinearClamp, uv3);
    return (sample0 + sample1 + sample2 + sample3) * 0.25;
}

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    float4 quarterBokeh = QuarterBokeh.Sample(LinearClamp, input.uv);
    float4 eighthBokeh = ResolveEighthBokeh(input.uv);
    return quarterBokeh + eighthBokeh;
}
