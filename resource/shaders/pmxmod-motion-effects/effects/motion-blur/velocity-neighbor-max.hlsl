Texture2D TileMotion : register(t0);
SamplerState LinearClamp : register(s0);

#include "../../include/motion-blur.hlsli"

float4 PSNeighborMax(FullscreenVertexOutput input) : SV_Target0 {
    float2 tileTexelSize = InverseViewportSize * VelocityTileScale;
    float4 dominantMotion = float4(0.0, 0.0, 1.0, 0.0);
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            float2 sampleUv = saturate(input.uv + float2(x, y) * tileTexelSize);
            float4 candidateMotion = TileMotion.SampleLevel(LinearClamp, sampleUv, 0.0);
            candidateMotion.xy = PrepareVelocity(candidateMotion.xy);
            candidateMotion.w = length(candidateMotion.xy);
            dominantMotion = SelectDominantMotion(dominantMotion, candidateMotion);
        }
    }
    return dominantMotion;
}
