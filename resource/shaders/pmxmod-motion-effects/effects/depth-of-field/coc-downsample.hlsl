// DOF CoC 1/4 해상도 축소 패스 입력:
// t0 = CoC가 포함된 반해상도 색상, t1 = SceneDepth, t2 = FocusHistory,
// s0 = LinearClamp.
Texture2D SceneColor : register(t0);
Texture2D SceneDepth : register(t1);
Texture2D FocusHistory : register(t2);
SamplerState LinearClamp : register(s0);

#include "../../include/depth-of-field.hlsli"

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
    float signedCoc = ResolveDominantCircleOfConfusion(coc0, coc1, coc2, coc3);
    float weight0 = CalculateDownsampleLayerWeight(coc0, signedCoc);
    float weight1 = CalculateDownsampleLayerWeight(coc1, signedCoc);
    float weight2 = CalculateDownsampleLayerWeight(coc2, signedCoc);
    float weight3 = CalculateDownsampleLayerWeight(coc3, signedCoc);
    if (abs(signedCoc) > BokehColorEpsilon) {
        weight0 *= CalculateBokehHighlightWeight(sample0.rgb);
        weight1 *= CalculateBokehHighlightWeight(sample1.rgb);
        weight2 *= CalculateBokehHighlightWeight(sample2.rgb);
        weight3 *= CalculateBokehHighlightWeight(sample3.rgb);
    }
    float totalWeight = weight0 + weight1 + weight2 + weight3;
    float3 color = totalWeight > BokehColorEpsilon
        ? (sample0.rgb * weight0 + sample1.rgb * weight1 + sample2.rgb * weight2 + sample3.rgb * weight3)
            / totalWeight
        : (sample0.rgb + sample1.rgb + sample2.rgb + sample3.rgb) * 0.25;
    return float4(color, EncodeCircleOfConfusion(signedCoc));
}
