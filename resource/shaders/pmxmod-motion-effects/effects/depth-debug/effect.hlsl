// 포스트 프로세스 입력 규격:
// t0 = SceneColor, t1 = SceneDepth, s0 = LinearClamp.
Texture2D SceneColor : register(t0);
Texture2D SceneDepth : register(t1);
SamplerState LinearClamp : register(s0);

#include "../../include/fullscreen.hlsli"

static const float DepthPower = 0.35;

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    const float depth = saturate(SceneDepth.Sample(LinearClamp, input.uv).r);
    const float visibleDepth = pow(saturate(1.0 - depth), DepthPower);
    return float4(visibleDepth.xxx, 1.0);
}
