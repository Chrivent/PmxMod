#include "post-process-frame.hlsli"

// MotionBlur3의 기본 방향 블러 강도다.
static const float DirectionalBlurStrength = 0.5;

// MotionBlur3의 라인 잔상 길이다.
static const float LineBlurLength = 1.5;

// MotionBlur3의 라인 잔상 합성 강도다.
static const float LineBlurStrength = 1.0;

// 비정상적으로 큰 화면 속도가 전체 화면을 덮지 않도록 제한하는 UV 길이다.
static const float VelocityLimit = 0.12;

// 이 값보다 느린 화면 이동에는 블러를 적용하지 않는다.
static const float VelocityUnderCut = 0.006;

// 첫 번째 방향 블러가 사용하는 MotionBlur3의 패스 배율이다.
static const float FirstDirectionalRate = 0.7;

// 라인 잔상 합성 뒤 방향을 정돈하는 MotionBlur3의 패스 배율이다.
static const float FinalDirectionalRate = 0.4;

// 첫 번째 방향 블러의 중심 양쪽 샘플 수다.
static const int DirectionalSampleRadius = 9;

// 최종 방향 보정의 중심 양쪽 샘플 수다.
static const int FinalDirectionalSampleRadius = 6;

// 라인 잔상 재구성의 중심 양쪽 샘플 수다.
static const int LineSampleRadius = 9;

// 라인 잔상 안정화 가우시안의 중심 양쪽 샘플 수다.
static const int LineGaussianSampleRadius = 6;

// 깊이 경계에서 서로 다른 표면의 색이 섞이는 정도를 줄이는 강도다.
static const float MotionDepthFalloff = 6.0;

// 라인 잔상의 가로 폭을 픽셀 단위로 지정한다.
static const float LineWidthPixels = 2.5;

// 가우시안 안정화 커널의 표준편차다.
static const float LineGaussianSigma = 2.75;

// 1/4 해상도 속도 타일의 원본 화면 배율이다.
static const float VelocityTileScale = 4.0;

static const float MotionEpsilon = 1.0e-5;

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

float2 PrepareVelocity(float2 velocity) {
    if (any(isnan(velocity)) || any(isinf(velocity)))
        return 0.0;
    float speed = length(velocity);
    if (speed <= VelocityUnderCut)
        return 0.0;
    float limitedSpeed = min(speed - VelocityUnderCut, VelocityLimit);
    return velocity / max(speed, MotionEpsilon) * limitedSpeed;
}

float LinearizeMotionDepth(float depth) {
    return CameraNearPlane * CameraFarPlane
        / max(CameraFarPlane - saturate(depth) * (CameraFarPlane - CameraNearPlane), 0.0001);
}

float4 PackMotion(float2 velocity, float depth) {
    float2 preparedVelocity = PrepareVelocity(velocity);
    return float4(preparedVelocity, saturate(depth), length(preparedVelocity));
}

float4 SelectDominantMotion(float4 currentMotion, float4 candidateMotion) {
    float maximumSpeed = max(currentMotion.w, candidateMotion.w);
    float speedDifference = candidateMotion.w - currentMotion.w;
    if (speedDifference > max(maximumSpeed * 0.05, MotionEpsilon))
        return candidateMotion;
    if (abs(speedDifference) <= max(maximumSpeed * 0.05, MotionEpsilon)
        && candidateMotion.z < currentMotion.z)
        return candidateMotion;
    return currentMotion;
}

float GaussianMotionWeight(float sampleRate, float sigma) {
    float normalizedRate = sampleRate / max(sigma, MotionEpsilon);
    return exp(-0.5 * normalizedRate * normalizedRate);
}

float CalculateDirectionAlignment(float2 referenceVelocity, float2 sampleVelocity) {
    float referenceSpeed = length(referenceVelocity);
    float sampleSpeed = length(sampleVelocity);
    if (referenceSpeed <= MotionEpsilon || sampleSpeed <= MotionEpsilon)
        return 0.0;
    float alignment = dot(referenceVelocity / referenceSpeed, sampleVelocity / sampleSpeed);
    return smoothstep(0.2, 0.9, alignment);
}

float CalculateMotionSegmentCoverage(float2 sourceUv, float2 targetUv, float2 velocity, float lengthScale) {
    float speed = length(velocity);
    if (speed <= MotionEpsilon)
        return 0.0;
    float2 direction = velocity / speed;
    float2 offset = targetUv - sourceUv;
    float alongDistance = abs(dot(offset, direction));
    float acrossDistance = abs(offset.x * direction.y - offset.y * direction.x);
    float maximumDistance = max(speed * lengthScale, max(InverseViewportSize.x, InverseViewportSize.y));
    float lineWidth = max(max(InverseViewportSize.x, InverseViewportSize.y) * LineWidthPixels,
        maximumDistance * 0.025);
    float alongCoverage = saturate(1.0 - alongDistance / max(maximumDistance, MotionEpsilon));
    float acrossCoverage = saturate(1.0 - acrossDistance / max(lineWidth, MotionEpsilon));
    return alongCoverage * acrossCoverage;
}

float CalculateDepthSimilarity(float firstDepth, float secondDepth) {
    float firstDistance = LinearizeMotionDepth(firstDepth);
    float secondDistance = LinearizeMotionDepth(secondDepth);
    float tolerance = max(min(firstDistance, secondDistance) * 0.02, 0.02);
    return exp2(-abs(firstDistance - secondDistance) / tolerance * MotionDepthFalloff);
}

float CalculateDirectionalDepthWeight(float centerDepth, float sampleDepth,
    float centerCoverage, float sampleCoverage) {
    float centerDistance = LinearizeMotionDepth(centerDepth);
    float sampleDistance = LinearizeMotionDepth(sampleDepth);
    float tolerance = max(min(centerDistance, sampleDistance) * 0.02, 0.02);
    float similarity = CalculateDepthSimilarity(centerDepth, sampleDepth);
    if (sampleDistance + tolerance < centerDistance)
        return max(similarity, sampleCoverage);
    if (sampleDistance > centerDistance + tolerance)
        return max(similarity, centerCoverage);
    return 1.0;
}

float CalculateTrailDepthWeight(float centerDepth, float sampleDepth) {
    float centerDistance = LinearizeMotionDepth(centerDepth);
    float sampleDistance = LinearizeMotionDepth(sampleDepth);
    float tolerance = max(min(centerDistance, sampleDistance) * 0.02, 0.02);
    if (sampleDistance <= centerDistance + tolerance)
        return 1.0;
    return CalculateDepthSimilarity(centerDepth, sampleDepth);
}
