// 1/4 CoC 색상을 1/8 해상도로 축소한다.
// t0 = 1/4 CoC 색상, t1 = SceneDepth, t2 = FocusHistory, s0 = LinearClamp.
Texture2D SceneColor : register(t0);
Texture2D SceneDepth : register(t1);
Texture2D FocusHistory : register(t2);
SamplerState LinearClamp : register(s0);

#include "../../include/depth-of-field.hlsli"

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    float2 offset = InverseViewportSize / BokehQuarterResolutionScale * 0.5;
    float4 sample0 = SceneColor.Sample(LinearClamp, input.uv + float2(-offset.x, -offset.y));
    float4 sample1 = SceneColor.Sample(LinearClamp, input.uv + float2(offset.x, -offset.y));
    float4 sample2 = SceneColor.Sample(LinearClamp, input.uv + float2(-offset.x, offset.y));
    float4 sample3 = SceneColor.Sample(LinearClamp, input.uv + float2(offset.x, offset.y));
    float coc0 = DecodeCircleOfConfusion(sample0.a);
    float coc1 = DecodeCircleOfConfusion(sample1.a);
    float coc2 = DecodeCircleOfConfusion(sample2.a);
    float coc3 = DecodeCircleOfConfusion(sample3.a);
    float signedCoc = ResolveDownsampledCircleOfConfusion(coc0, coc1, coc2, coc3);
    float weight0 = CalculateCircleOfConfusionDownsampleWeight(coc0);
    float weight1 = CalculateCircleOfConfusionDownsampleWeight(coc1);
    float weight2 = CalculateCircleOfConfusionDownsampleWeight(coc2);
    float weight3 = CalculateCircleOfConfusionDownsampleWeight(coc3);
    float totalWeight = weight0 + weight1 + weight2 + weight3;
    float3 color = totalWeight > BokehColorEpsilon
        ? (sample0.rgb * weight0 + sample1.rgb * weight1 + sample2.rgb * weight2 + sample3.rgb * weight3)
            / totalWeight
        : (sample0.rgb + sample1.rgb + sample2.rgb + sample3.rgb) * 0.25;
    return float4(color, EncodeCircleOfConfusion(signedCoc));
}
