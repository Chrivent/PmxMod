Texture2D SceneDepth : register(t1);
Texture2D SceneVelocity : register(t2);
SamplerState LinearClamp : register(s0);

#include "../../include/motion-blur.hlsli"

float4 PSTileMax(FullscreenVertexOutput input) : SV_Target0 {
    float4 dominantMotion = float4(0.0, 0.0, 1.0, 0.0);
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            float2 offset = (float2(x, y) - 1.5) * InverseViewportSize;
            float2 sampleUv = saturate(input.uv + offset);
            float2 velocity = SceneVelocity.SampleLevel(LinearClamp, sampleUv, 0.0).xy;
            float depth = SceneDepth.SampleLevel(LinearClamp, sampleUv, 0.0).r;
            dominantMotion = SelectDominantMotion(dominantMotion, PackMotion(velocity, depth));
        }
    }
    return dominantMotion;
}
