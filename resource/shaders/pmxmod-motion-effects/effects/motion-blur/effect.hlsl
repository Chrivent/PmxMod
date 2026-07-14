#include "../../include/post-process-frame.hlsli"

// t0 = 현재 패스 색상, t1 = 장면 깊이, t2 = 화면 UV 단위 장면 속도,
// t3 = 효과 적용 전 원본 색상, s0 = LinearClamp.
Texture2D PassColor : register(t0);
Texture2D SceneDepth : register(t1);
Texture2D SceneVelocity : register(t2);
Texture2D OriginalColor : register(t3);
SamplerState LinearClamp : register(s0);

// MotionBlur3의 DirectionalBlurStrength 기본값이다. 노출 시간에 해당하는 방향 블러 길이를 조정한다.
static const float DirectionalBlurStrength = 0.5;

// MotionBlur3의 LineBlurLength 기본값이다. 빠른 움직임의 꼬리를 얼마나 길게 보정할지 정한다.
static const float LineBlurLength = 1.5;

// MotionBlur3의 LineBlurStrength 기본값이다. 방향 블러 위에 더하는 선형 보정의 강도를 정한다.
static const float LineBlurStrength = 1.0;

// MotionBlur3의 VelocityLimit 기본값이다. 장면 전환이나 순간 이동이 화면 전체 블러가 되지 않게 제한한다.
static const float VelocityLimit = 0.12;

// MotionBlur3의 VelocityUnderCut 기본값이다. 이보다 작은 화면 이동은 블러를 적용하지 않는다.
static const float VelocityUnderCut = 0.006;

// MotionBlur3 방향 블러의 중심 양쪽 샘플 수다. 중심을 포함해 총 19회 샘플링한다.
static const int DirectionalSampleRadius = 9;

// 두 번째 선형 보정 패스의 중심 양쪽 샘플 수다.
static const int LineSampleRadius = 4;

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

float LinearizeDepth(float depth) {
    return CameraNearPlane * CameraFarPlane
        / max(CameraFarPlane - depth * (CameraFarPlane - CameraNearPlane), 0.0001);
}

float2 ClampVelocity(float2 velocity) {
    return clamp(velocity, float2(-VelocityLimit, -VelocityLimit), float2(VelocityLimit, VelocityLimit));
}

float2 ReadFrameVelocity(float2 uv) {
    float2 velocity = SceneVelocity.SampleLevel(LinearClamp, uv, 0.0).xy;
    if (any(isnan(velocity)) || any(isinf(velocity)))
        return float2(0.0, 0.0);
    velocity = ClampVelocity(velocity);
    const float velocityLength = length(velocity);
    if (velocityLength <= VelocityUnderCut)
        return float2(0.0, 0.0);
    velocity *= (velocityLength - VelocityUnderCut) / velocityLength;
    return velocity;
}

float DepthWeight(float centerDepth, float sampleDepth, float sampleRate) {
    const float centerDistance = LinearizeDepth(centerDepth);
    const float sampleDistance = LinearizeDepth(sampleDepth);
    const float tolerance = max(0.02 * centerDistance, 0.02);
    const float foregroundDifference = centerDistance - sampleDistance;
    const float foregroundWeight = saturate(1.0 - max(foregroundDifference - tolerance, 0.0) / tolerance);
    const float backgroundDifference = sampleDistance - centerDistance;
    const float backgroundWeight = saturate(1.0 - max(backgroundDifference - tolerance * 4.0, 0.0) / (tolerance * 4.0));
    return lerp(1.0, foregroundWeight * backgroundWeight, saturate(abs(sampleRate)));
}

float GaussianWeight(float sampleRate, float sigma) {
    const float normalized = sampleRate / max(sigma, 0.0001);
    return exp(-0.5 * normalized * normalized);
}

float4 DirectionalBlur(float2 uv, float strength, int sampleRadius) {
    const float4 centerColor = PassColor.SampleLevel(LinearClamp, uv, 0.0);
    if (FrameHistoryReset > 0.5)
        return centerColor;
    const float2 velocity = ReadFrameVelocity(uv);
    if (dot(velocity, velocity) <= 0.0)
        return centerColor;
    const float centerDepth = SceneDepth.SampleLevel(LinearClamp, uv, 0.0).r;
    float4 colorSum = 0.0;
    float weightSum = 0.0;
    for (int sampleIndex = -sampleRadius; sampleIndex <= sampleRadius; sampleIndex++) {
        const float sampleRate = (float)sampleIndex / max((float)sampleRadius, 1.0);
        const float2 sampleUv = clamp(uv - velocity * strength * sampleRate, 0.0, 1.0);
        const float sampleDepth = SceneDepth.SampleLevel(LinearClamp, sampleUv, 0.0).r;
        const float weight = GaussianWeight(sampleRate, 0.5) * DepthWeight(centerDepth, sampleDepth, sampleRate);
        colorSum += PassColor.SampleLevel(LinearClamp, sampleUv, 0.0) * weight;
        weightSum += weight;
    }
    return colorSum / max(weightSum, 0.0001);
}

float4 PSDirectional(FullscreenVertexOutput input) : SV_Target0 {
    return DirectionalBlur(input.uv, DirectionalBlurStrength, DirectionalSampleRadius);
}

float4 PSLineRefine(FullscreenVertexOutput input) : SV_Target0 {
    const float4 original = OriginalColor.SampleLevel(LinearClamp, input.uv, 0.0);
    if (FrameHistoryReset > 0.5)
        return original;
    const float2 velocity = ReadFrameVelocity(input.uv);
    const float speedWeight = saturate(length(velocity) / max(VelocityUnderCut * 4.0, 0.0001));
    if (speedWeight <= 0.0)
        return original;
    const float4 refined = DirectionalBlur(input.uv,
        DirectionalBlurStrength * LineBlurLength, LineSampleRadius);
    const float4 directional = PassColor.SampleLevel(LinearClamp, input.uv, 0.0);
    const float refineWeight = saturate(LineBlurStrength * speedWeight * 0.5);
    return lerp(directional, refined, refineWeight);
}
