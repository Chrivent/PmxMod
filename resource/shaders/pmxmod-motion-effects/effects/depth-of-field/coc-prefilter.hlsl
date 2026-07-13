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
    float2 offset = InverseViewportSize * 0.5;
    float2 uv0 = input.uv + float2(-offset.x, -offset.y);
    float2 uv1 = input.uv + float2(offset.x, -offset.y);
    float2 uv2 = input.uv + float2(-offset.x, offset.y);
    float2 uv3 = input.uv + float2(offset.x, offset.y);
    float3 color = (SceneColor.Sample(LinearClamp, uv0).rgb
        + SceneColor.Sample(LinearClamp, uv1).rgb
        + SceneColor.Sample(LinearClamp, uv2).rgb
        + SceneColor.Sample(LinearClamp, uv3).rgb) * 0.25;
    float focusDistance = ReadDelayedFocusDistance();
    float coc0 = CalculateCircleOfConfusionPixels(ReadCameraDistance(uv0), focusDistance);
    float coc1 = CalculateCircleOfConfusionPixels(ReadCameraDistance(uv1), focusDistance);
    float coc2 = CalculateCircleOfConfusionPixels(ReadCameraDistance(uv2), focusDistance);
    float coc3 = CalculateCircleOfConfusionPixels(ReadCameraDistance(uv3), focusDistance);
    float signedCoc = ResolveStrongerCircleOfConfusion(coc0, coc1);
    signedCoc = ResolveStrongerCircleOfConfusion(signedCoc, coc2);
    signedCoc = ResolveStrongerCircleOfConfusion(signedCoc, coc3);
    return float4(color, EncodeCircleOfConfusion(signedCoc));
}
