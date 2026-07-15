// 장면 depth와 velocity 입력이 공유하는 표면 alpha 판정 계약이다.
cbuffer SceneSurfacePixelConstants : register(b1) {
    float MaterialOpacity;
    float TextureAlphaEnabled;
    float AlphaCutoff;
    float SceneSurfacePadding;
}

Texture2D BaseTexture : register(t0);
SamplerState BaseSampler : register(s0);

// Depth에서는 Matrix1이 WVP이고, velocity에서는 Matrix0/1이 현재/이전 WVP다.
cbuffer SceneVertexConstants : register(b0) {
    float4x4 Matrix0;
    float4x4 Matrix1;
}

struct SceneDepthVertexInput {
    float3 position : POSITION0;
    float2 uv : UV0;
};

struct SceneDepthVertexOutput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

SceneDepthVertexOutput VSDepth(SceneDepthVertexInput input) {
    SceneDepthVertexOutput output;
    output.position = mul(Matrix1, float4(input.position, 1.0));
    output.uv = float2(input.uv.x, 1.0 - input.uv.y);
    return output;
}

void ApplySurfaceAlphaCutout(float2 uv) {
    float opacity = MaterialOpacity;
    if (TextureAlphaEnabled > 0.5)
        opacity *= BaseTexture.Sample(BaseSampler, uv).a;
    clip(opacity - AlphaCutoff);
}

void PSDepth(SceneDepthVertexOutput input) {
    ApplySurfaceAlphaCutout(input.uv);
}

struct SceneVelocityVertexInput {
    float3 position : POSITION0;
    float3 previousPosition : POSITION1;
    float2 uv : UV0;
};

struct SceneVelocityVertexOutput {
    float4 position : SV_POSITION;
    float4 currentClipPosition : TEXCOORD0;
    float4 previousClipPosition : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

SceneVelocityVertexOutput VSVelocity(SceneVelocityVertexInput input) {
    SceneVelocityVertexOutput output;
    output.currentClipPosition = mul(Matrix0, float4(input.position, 1.0));
    output.previousClipPosition = mul(Matrix1, float4(input.previousPosition, 1.0));
    output.position = output.currentClipPosition;
    output.uv = float2(input.uv.x, 1.0 - input.uv.y);
    return output;
}

float2 CalculateVelocity(SceneVelocityVertexOutput input, float2 clipToUvScale) {
    const float2 currentNdc = input.currentClipPosition.xy / max(abs(input.currentClipPosition.w), 0.00001);
    const float2 previousNdc = input.previousClipPosition.xy / max(abs(input.previousClipPosition.w), 0.00001);
    return (currentNdc - previousNdc) * clipToUvScale;
}

float2 PSVelocity(SceneVelocityVertexOutput input) : SV_Target0 {
    ApplySurfaceAlphaCutout(input.uv);
    return CalculateVelocity(input, float2(0.5, 0.5));
}

float2 PSVelocityInvertedY(SceneVelocityVertexOutput input) : SV_Target0 {
    ApplySurfaceAlphaCutout(input.uv);
    return CalculateVelocity(input, float2(0.5, -0.5));
}
