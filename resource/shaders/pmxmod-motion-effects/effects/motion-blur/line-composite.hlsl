Texture2D DirectionalColor : register(t0);
Texture2D LineBlur : register(t1);
SamplerState LinearClamp : register(s0);

#include "../../include/motion-blur.hlsli"

float4 PSLineComposite(FullscreenVertexOutput input) : SV_Target0 {
    float4 baseColor = DirectionalColor.SampleLevel(LinearClamp, input.uv, 0.0);
    if (FrameHistoryReset > 0.5)
        return baseColor;
    float4 lineSample = LineBlur.SampleLevel(LinearClamp, input.uv, 0.0);
    float coverage = saturate(lineSample.a * 1.3 * LineBlurStrength);
    float3 lineColor = lineSample.a > MotionEpsilon ? lineSample.rgb / lineSample.a : baseColor.rgb;
    return float4(lerp(baseColor.rgb, lineColor, coverage), baseColor.a);
}
