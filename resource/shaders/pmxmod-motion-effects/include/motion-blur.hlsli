#ifndef PMXMOD_MOTION_BLUR_HLSLI
#define PMXMOD_MOTION_BLUR_HLSLI

#include "post-process-frame.hlsli"
#include "post-process-parameters.hlsli"
#include "fullscreen.hlsli"

// MotionBlur3의 기본 방향 블러 강도다.
#define DirectionalBlurStrength ReadEffectParameter(0)

// MotionBlur3의 라인 잔상 길이다.
#define LineBlurLength ReadEffectParameter(1)

// MotionBlur3의 라인 잔상 합성 강도다.
#define LineBlurStrength ReadEffectParameter(2)

// 비정상적으로 큰 화면 속도가 전체 화면을 덮지 않도록 제한하는 UV 길이다.
#define VelocityLimit ReadEffectParameter(3)

// 이 값보다 느린 화면 이동에는 블러를 적용하지 않는다.
#define VelocityUnderCut ReadEffectParameter(4)

// 카메라가 이 거리보다 크게 이동하면 MotionBlur3처럼 장면 전환으로 판정한다.
#define SceneChangePositionThreshold ReadEffectParameter(5)

// 카메라 방향 변화가 이 각도를 넘으면 장면 전환으로 판정한다.
#define SceneChangeAngleThreshold ReadEffectParameter(6)

// 첫 번째 방향 블러가 사용하는 MotionBlur3의 패스 배율이다.
static const float FirstDirectionalRate = 0.7;

// 라인 잔상 합성 뒤 방향을 정돈하는 MotionBlur3의 패스 배율이다.
static const float FinalDirectionalRate = 0.4;

// 첫 번째 방향 블러의 중심 양쪽 샘플 수다.
static const int DirectionalSampleRadius = 9;

// 최종 방향 보정의 중심 양쪽 샘플 수다.
static const int FinalDirectionalSampleRadius = 9;

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

// 모션 벡터를 30fps 기준의 고정 셔터 시간으로 정규화할 때 사용하는 초 단위 기준값이다.
static const float ReferenceShutterTime = 1.0 / 30.0;

static const float MotionEpsilon = 1.0e-5;

// 불연속적인 depth와 velocity가 물체 경계에서 보간되지 않도록 가장 가까운 texel을 읽는다.
float4 LoadMotionTexture(Texture2D textureInput, float2 uv) {
    uint width;
    uint height;
    textureInput.GetDimensions(width, height);
    int2 maximumTexel = int2(max(width, 1u), max(height, 1u)) - 1;
    int2 texel = clamp(int2(saturate(uv) * float2(width, height)), int2(0, 0), maximumTexel);
    return textureInput.Load(int3(texel, 0));
}

// 픽셀마다 고정된 분산값을 만들어 라인 샘플의 규칙적인 띠를 줄인다.
float CalculateMotionJitter(float2 pixelPosition) {
    float noise = frac(52.9829189 * frac(dot(floor(pixelPosition), float2(0.06711056, 0.00583715))));
    return noise - 0.5;
}

// 카메라 컷이나 히스토리 초기화 프레임에서는 이전 장면의 잔상을 사용하지 않는다.
bool IsMotionSceneChange() {
    float directionDot = dot(normalize(CameraWorldDirection.xyz), normalize(PreviousCameraWorldDirection.xyz));
    float positionDelta = distance(CameraWorldPosition.xyz, PreviousCameraWorldPosition.xyz);
    float directionThreshold = cos(SceneChangeAngleThreshold * 0.017453292519943295);
    return FrameHistoryReset > 0.5 || positionDelta > SceneChangePositionThreshold
        || directionDot < directionThreshold;
}

// 속도 입력의 비정상값과 미세 움직임을 제거하고 최대 길이를 제한한다.
float2 PrepareVelocity(float2 velocity) {
    if (FrameHistoryReset > 0.5 || FrameDeltaTime <= 0.0)
        return 0.0;
    float frameDuration = clamp(FrameDeltaTime, 1.0 / 240.0, 1.0 / 15.0);
    float shutterScale = clamp(ReferenceShutterTime / frameDuration, 0.5, 4.0);
    velocity *= shutterScale;
    if (!all(abs(velocity) < 1.0e8))
        return 0.0;
    float speed = length(velocity);
    if (speed <= VelocityUnderCut)
        return 0.0;
    float limitedSpeed = min(speed - VelocityUnderCut, VelocityLimit);
    return velocity / max(speed, MotionEpsilon) * limitedSpeed;
}

// 하드웨어 깊이를 모션 경계 판정에 사용할 뷰 공간 거리로 변환한다.
float LinearizeMotionDepth(float depth) {
    return CameraNearPlane * CameraFarPlane
        / max(CameraFarPlane - saturate(depth) * (CameraFarPlane - CameraNearPlane), 0.0001);
}

// 정규화된 속도, 깊이, 속력 정보를 하나의 값으로 묶는다.
float4 PackMotion(float2 velocity, float depth) {
    float2 preparedVelocity = PrepareVelocity(velocity);
    return float4(preparedVelocity, saturate(depth), length(preparedVelocity));
}

// 속력이 크고 깊이가 가까운 모션 후보를 선택한다.
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

// 모션 샘플에 적용할 가우시안 가중치를 계산한다.
float GaussianMotionWeight(float sampleRate, float sigma) {
    float normalizedRate = sampleRate / max(sigma, MotionEpsilon);
    return exp(-0.5 * normalizedRate * normalizedRate);
}

// 두 속도 벡터가 같은 방향으로 움직이는 정도를 계산한다.
float CalculateDirectionAlignment(float2 referenceVelocity, float2 sampleVelocity) {
    float referenceSpeed = length(referenceVelocity);
    float sampleSpeed = length(sampleVelocity);
    if (referenceSpeed <= MotionEpsilon || sampleSpeed <= MotionEpsilon)
        return 0.0;
    float alignment = dot(referenceVelocity / referenceSpeed, sampleVelocity / sampleSpeed);
    return smoothstep(0.2, 0.9, alignment);
}

// 한 픽셀의 이동 선분이 대상 화면 위치를 덮는 비율을 계산한다.
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

// 두 깊이가 같은 표면에 속할 가능성을 계산한다.
float CalculateDepthSimilarity(float firstDepth, float secondDepth) {
    float firstDistance = LinearizeMotionDepth(firstDepth);
    float secondDistance = LinearizeMotionDepth(secondDepth);
    float tolerance = max(min(firstDistance, secondDistance) * 0.02, 0.02);
    return exp2(-abs(firstDistance - secondDistance) / tolerance * MotionDepthFalloff);
}

// 방향 블러에서 전경과 후경의 잘못된 색 혼합을 억제한다.
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

// 라인 잔상에서 뒤쪽 표면이 앞쪽으로 번지는 현상을 억제한다.
float CalculateTrailDepthWeight(float centerDepth, float sampleDepth) {
    float centerDistance = LinearizeMotionDepth(centerDepth);
    float sampleDistance = LinearizeMotionDepth(sampleDepth);
    float tolerance = max(min(centerDistance, sampleDistance) * 0.02, 0.02);
    if (sampleDistance <= centerDistance + tolerance)
        return 1.0;
    return CalculateDepthSimilarity(centerDepth, sampleDepth);
}

#endif
