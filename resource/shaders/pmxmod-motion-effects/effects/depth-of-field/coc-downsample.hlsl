// DOF CoC 1/4 해상도 축소 패스 입력:
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

float ResolveStrongerCircleOfConfusion(float currentCoc, float candidateCoc) {
    return abs(candidateCoc) > abs(currentCoc) ? candidateCoc : currentCoc;
}

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    float2 offset = InverseViewportSize;
    float4 sample0 = SceneColor.Sample(LinearClamp, input.uv + float2(-offset.x, -offset.y));
    float4 sample1 = SceneColor.Sample(LinearClamp, input.uv + float2(offset.x, -offset.y));
    float4 sample2 = SceneColor.Sample(LinearClamp, input.uv + float2(-offset.x, offset.y));
    float4 sample3 = SceneColor.Sample(LinearClamp, input.uv + float2(offset.x, offset.y));
    float coc0 = DecodeCircleOfConfusion(sample0.a);
    float coc1 = DecodeCircleOfConfusion(sample1.a);
    float coc2 = DecodeCircleOfConfusion(sample2.a);
    float coc3 = DecodeCircleOfConfusion(sample3.a);
    float weight0 = abs(coc0);
    float weight1 = abs(coc1);
    float weight2 = abs(coc2);
    float weight3 = abs(coc3);
    float totalWeight = weight0 + weight1 + weight2 + weight3;
    float3 color = totalWeight > BokehColorEpsilon
        ? (sample0.rgb * weight0 + sample1.rgb * weight1 + sample2.rgb * weight2 + sample3.rgb * weight3)
            / totalWeight
        : (sample0.rgb + sample1.rgb + sample2.rgb + sample3.rgb) * 0.25;
    float signedCoc = ResolveStrongerCircleOfConfusion(coc0, coc1);
    signedCoc = ResolveStrongerCircleOfConfusion(signedCoc, coc2);
    signedCoc = ResolveStrongerCircleOfConfusion(signedCoc, coc3);
    return float4(color, EncodeCircleOfConfusion(signedCoc));
}
