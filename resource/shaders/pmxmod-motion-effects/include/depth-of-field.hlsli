#ifndef PMXMOD_DEPTH_OF_FIELD_HLSLI
#define PMXMOD_DEPTH_OF_FIELD_HLSLI

#include "post-process-frame.hlsli"
#include "post-process-parameters.hlsli"
#include "fullscreen.hlsli"
#include "depth-of-field-kernels.hlsli"

// ikBokeh 계열 DOF에서 각 패스가 공유하는 임시 파라미터다.
// 나중에 UI와 모션 파라미터 시스템이 생기면 상수 버퍼나 패키지 파라미터로 교체한다.

// 보케를 강하게 만드는 ikBokeh의 "Bokeh+" 값이다.
#define BokehPlus ReadEffectParameter(0)

// 보케를 약하게 만드는 ikBokeh의 "Bokeh-" 값이다.
#define BokehMinus ReadEffectParameter(1)

// 전경 보케를 약하게 만드는 ikBokeh의 "Front Bokeh-" 값이다.
#define FrontBokehMinus ReadEffectParameter(2)

// 계산된 CoC를 추가 확대하는 ikBokeh의 "CoC size" 값이다.
#define CoCSize ReadEffectParameter(3)

// 자동 초점 측정 영역을 정하는 값이다. 0은 한 점, 0.5는 좁은 영역, 1은 넓은 영역을 사용한다.
#define AutoMeasuringMode ReadEffectParameter(4)

// 초점이 맞는 범위를 픽셀 단위로 넓히는 ikBokeh의 "Focus range" 값이다.
#define FocusRangeParam ReadEffectParameter(5)

// 밝은 보케 색을 강조하는 ikBokeh의 "emphasize" 값이다.
#define Emphasize ReadEffectParameter(6)

// 초점 이동의 지연 강도다. 0이면 즉시 따라가고 1에 가까울수록 천천히 접근한다.
#define FocusDelay ReadEffectParameter(7)

// 초점 이동 관성의 유지 비율이다. 0이면 속도가 빠르게 사라지고 1에 가까울수록 오래 유지된다.
#define FocusSlip ReadEffectParameter(8)

// 수동 초점 모드의 혼합 비율이다. 0은 자동, 1은 수동 초점이다.
#define ManualFocusMode ReadEffectParameter(9)

// 수동 초점 거리를 0~1 범위로 지정한다. 1은 MMD 좌표계 기준 50미터다.
#define ManualFocusDistance ReadEffectParameter(10)

// 수동 렌즈 초점거리를 0~1 범위로 지정한다. 0은 20mm, 1은 120mm다.
#define ManualFocalLength ReadEffectParameter(11)

// 상대 초점 거리를 앞으로 이동하는 컨트롤러 값이다.
#define RelativeFocusPlus ReadEffectParameter(12)

// 상대 초점 거리를 뒤로 이동하는 컨트롤러 값이다.
#define RelativeFocusMinus ReadEffectParameter(13)

// 측정 위치 XY와 상대 초점 Z를 함께 움직이는 ikBokeh 컨트롤러 위치다.
#define FocusControllerPosition float3(ReadEffectParameter(14), ReadEffectParameter(15), ReadEffectParameter(16))

// 자동 초점 측정 위치의 X 양의 방향 보정값이다.
#define MeasuringPositionXPlus ReadEffectParameter(17)

// 자동 초점 측정 위치의 X 음의 방향 보정값이다.
#define MeasuringPositionXMinus ReadEffectParameter(18)

// 자동 초점 측정 위치의 Y 양의 방향 보정값이다.
#define MeasuringPositionYPlus ReadEffectParameter(19)

// 자동 초점 측정 위치의 Y 음의 방향 보정값이다.
#define MeasuringPositionYMinus ReadEffectParameter(20)

// 초점면을 피사체에 수직으로 기울이는 ikBokeh 틸트 업 값이다.
#define TiltFocusUp ReadEffectParameter(21)

// 초점면을 지정 축과 평행하게 기울이는 ikBokeh 틸트 다운 값이다.
#define TiltFocusDown ReadEffectParameter(22)

// 틸트 초점면의 기준 축이다. UI가 생기기 전에는 월드 위쪽을 사용한다.
#define TiltFocusDirection float3(ReadEffectParameter(23), ReadEffectParameter(24), ReadEffectParameter(25))

// 현재 반해상도와 1/4 해상도 피라미드가 안정적으로 처리할 최대 CoC 반경이다.
static const float MaxBlurPixels = 64.0;

// 1/4 피라미드가 담당하고 1/8 피라미드로 넘기기 시작하는 CoC 반경이다.
static const float QuarterBlurPixels = 32.0;

// 반해상도 보정 패스가 직접 처리할 최대 흐림 반경이다.
static const float MediumBlurPixels = 8.0;

// 첫 번째 CoC target과 중간 보케 보정 target의 해상도 비율이다.
static const float BokehHalfResolutionScale = 0.5;

// 광역 보케 target의 해상도 비율이다.
static const float BokehQuarterResolutionScale = 0.25;

// 가장 넓은 보케 target의 해상도 비율이다.
static const float BokehEighthResolutionScale = 0.125;

// ikBokeh가 색 처리에 사용하는 감마다.
static const float BokehColorGamma = 2.2;

// 색 지수 연산에서 0으로 인한 불안정한 값을 방지하는 최솟값이다.
static const float BokehColorEpsilon = 1.0e-4;

// ikBokeh의 보케 색 강조 최대 배율이다.
static const float EmphasizeRateScale = 2.0;

// CoC 밝기 보정이 발산하지 않도록 보장하는 최소 반경이다.
static const float MinimumCoCRadius = 1.0;

// 서로 다른 깊이의 보케가 업샘플 과정에서 섞이지 않도록 감쇠하는 강도다.
static const float DepthEdgeFalloff = 32.0;

// 광역 전경 보케의 커버리지를 0~1 범위로 정규화하는 커널 가중치다.
static const float ForegroundCoverageNormalization = 16.0;

// 축소 과정에서 전경으로 확장할 최소 CoC 반경이다.
static const float ForegroundDilationPixels = 0.5;

// 원형 보케에 8각 조리개 형태를 섞는 비율이다.
static const float ApertureShapeStrength = 0.25;

// 밝은 점광원이 축소와 블러 과정에서 사라지지 않게 유지하는 최대 가중치다.
static const float BokehHighlightBias = 0.5;

// 밝은 보케 보존 가중치가 적용되기 시작하는 선형 색상 밝기다.
static const float BokehHighlightThreshold = 0.55;

// 초점 히스토리 텍스처에 유효한 데이터가 저장되었는지 판별하는 표식이다.
static const float FocusHistoryMarker = 0.5;

// MMD 좌표계에서 1미터에 해당하는 길이다.
static const float MmdUnitsPerMeter = 1.0 / 0.08;

// MMD 좌표계에서 1밀리미터에 해당하는 길이다.
static const float MmdUnitsPerMillimeter = MmdUnitsPerMeter * 0.001;

// 수동 초점 거리의 최대 조정 범위인 50미터다.
static const float AbsoluteFocusScale = 50.0 * MmdUnitsPerMeter;

// 상대 초점 거리의 최대 조정 범위인 5미터다.
static const float RelativeFocusScale = 5.0 * MmdUnitsPerMeter;

// 수동 렌즈 초점거리의 100mm 조정 범위다.
static const float ManualFocalLengthScale = 100.0 * MmdUnitsPerMillimeter;

// ikBokeh가 사용하는 24mm 필름 높이다.
static const float FilmHeight = 24.0 * MmdUnitsPerMillimeter;

// 자동 초점이 카메라에 지나치게 가까워지지 않도록 제한하는 최소 거리다.
static const float MinFocusDistance = 0.1 * MmdUnitsPerMeter;

// ikBokeh가 허용하는 최소 렌즈 초점거리인 20mm다.
static const float MinFocalLength = 20.0 * MmdUnitsPerMillimeter;

// ikBokeh가 허용하는 최대 렌즈 초점거리인 200mm다.
static const float MaxFocalLength = 200.0 * MmdUnitsPerMillimeter;

// ikBokeh의 기본 조리개값이다.
static const float DefaultFNumber = 4.0;

// Bokeh+와 Bokeh-를 조리개값에 반영하는 배율이다.
static const float FNumberScale = 4.0;

// Bokeh+와 Bokeh-를 렌즈 초점거리에 반영하는 50mm 기준 배율이다.
static const float BokehFocalLengthScale = 50.0 * MmdUnitsPerMillimeter;

// 장면 깊이 텍스처의 값을 안전한 0~1 범위로 읽는다.
float ReadDepth(float2 uv) {
    return saturate(SceneDepth.Sample(LinearClamp, uv).r);
}

// 현재 카메라의 수직 시야각 절반에 대한 탄젠트 값을 반환한다.
float GetTanHalfFov() {
    return tan(max(CameraVerticalFovRadians, 0.001) * 0.5);
}

// 하드웨어 깊이를 뷰 공간의 전방 거리로 변환한다.
float LinearizeDepth(float depth) {
    return CameraNearPlane * CameraFarPlane
        / max(CameraFarPlane - depth * (CameraFarPlane - CameraNearPlane), 0.0001);
}

// 하드웨어 깊이와 화면 위치를 카메라 원점에서의 실제 거리로 변환한다.
float DepthToCameraDistance(float depth, float2 uv) {
    if (depth >= 0.999999)
        return CameraFarPlane;

    float viewDepth = LinearizeDepth(depth);
    float aspectRatio = ViewportSize.x / max(ViewportSize.y, 1.0);
    float2 ndc = uv * 2.0 - 1.0;
    float2 rayOffset = ndc * float2(aspectRatio, 1.0) * GetTanHalfFov();
    return viewDepth * length(float3(rayOffset, 1.0));
}

// 장면 깊이 텍스처에서 현재 화면 위치의 카메라 거리를 읽는다.
float ReadCameraDistance(float2 uv) {
    return DepthToCameraDistance(ReadDepth(uv), uv);
}

// 화면 UV를 현재 카메라의 월드 공간 광선으로 변환한다.
float3 CalculateCameraRay(float2 uv) {
    float aspectRatio = ViewportSize.x / max(ViewportSize.y, 1.0);
    float2 ndc = uv * float2(2.0, -2.0) + float2(-1.0, 1.0);
    return normalize(CameraWorldDirection.xyz
        + CameraWorldRight.xyz * ndc.x * aspectRatio * GetTanHalfFov()
        + CameraWorldUp.xyz * ndc.y * GetTanHalfFov());
}

// 화면 UV와 카메라 거리를 월드 공간 위치로 복원한다.
float3 ReconstructWorldPosition(float2 uv, float cameraDistance) {
    return CameraWorldPosition.xyz + CalculateCameraRay(uv) * cameraDistance;
}

// ikBokeh 컨트롤러 값으로부터 자동 초점 측정 위치를 계산한다.
float2 CalculateMeasuringPosition() {
    float2 basePosition = float2(MeasuringPositionXPlus - MeasuringPositionXMinus,
        MeasuringPositionYPlus - MeasuringPositionYMinus) * 0.5 + 0.5;
    float2 controllerOffset = FocusControllerPosition.xy * float2(1.0, -1.0) * 0.1;
    return saturate(basePosition + controllerOffset);
}

// 0~1 UI 값을 ikBokeh의 네 가지 자동 초점 측정 모드로 변환한다.
int CalculateAutoFocusMode() {
    return (int)(saturate(AutoMeasuringMode) * 3.0 + 0.1);
}

// 자동 초점 모드가 사용하는 화면 측정 반경을 반환한다.
float MeasuringCircleRadius() {
    return CalculateAutoFocusMode() > 1 ? 0.2 : 0.05;
}

// 현재 측정 모드의 원시 자동 초점 거리를 읽는다.
float ReadAutoFocusDistance() {
    float2 focusUv = CalculateMeasuringPosition();
    if (CalculateAutoFocusMode() == 0)
        return ReadCameraDistance(focusUv);

    float r1 = MeasuringCircleRadius();
    float r2 = r1 * 0.714;
    float depth0 = ReadCameraDistance(saturate(focusUv + float2(-r2, -r2)));
    float depth1 = ReadCameraDistance(saturate(focusUv + float2(-r1, 0.0)));
    float depth2 = ReadCameraDistance(saturate(focusUv + float2(-r2, r2)));
    float depth3 = ReadCameraDistance(saturate(focusUv + float2(0.0, -r1)));
    float depth4 = ReadCameraDistance(focusUv);
    float depth5 = ReadCameraDistance(saturate(focusUv + float2(0.0, r1)));
    float depth6 = ReadCameraDistance(saturate(focusUv + float2(r2, -r2)));
    float depth7 = ReadCameraDistance(saturate(focusUv + float2(r1, 0.0)));
    float depth8 = ReadCameraDistance(saturate(focusUv + float2(r2, r2)));
    float depthMin0 = min(min(depth0, depth1), min(depth2, depth3));
    float depthMin1 = min(min(depth4, depth5), min(depth6, depth7));
    return max(min(min(depthMin0, depthMin1), depth8), MinFocusDistance);
}

// 수동 초점과 비선형 상대 초점 보정을 자동 초점 결과에 합성한다.
float ReadTargetFocusDistance() {
    float focusDistance = ReadAutoFocusDistance();
    float manualDistance = ManualFocusDistance * AbsoluteFocusScale;
    focusDistance = lerp(focusDistance, manualDistance, saturate(ManualFocusMode));
    float relativeDistance = RelativeFocusPlus - RelativeFocusMinus;
    relativeDistance = relativeDistance * relativeDistance * sign(relativeDistance);
    float adjustment = relativeDistance * RelativeFocusScale + FocusControllerPosition.z;
    return max(focusDistance + adjustment, MinFocusDistance);
}

// 히스토리에 저장된 지연 초점 거리를 읽는다.
float ReadDelayedFocusDistance() {
    float4 history = FocusHistory.Sample(LinearClamp, float2(0.5, 0.5));
    float focusDistance = ReadTargetFocusDistance();
    if (abs(history.b - FocusHistoryMarker) < 0.01)
        focusDistance = history.r / max(GetTanHalfFov(), 0.0001);
    return max(focusDistance, MinFocusDistance);
}

// Bokeh+, Bokeh- 값으로 조리개 F-number를 계산한다.
float CalculateFNumber() {
    float fNumber = DefaultFNumber + (BokehMinus + 0.5 - BokehPlus) * FNumberScale;
    return clamp(fNumber, 1.0, 16.0);
}

// 자동 또는 수동 렌즈 설정으로 실제 초점거리를 계산한다.
float CalculateFocalLength(float focusDistance) {
    float safeFocusDistance = max(focusDistance, MinFocusDistance);
    float halfFilmHeight = FilmHeight * 0.5;
    float automaticFocalLength = safeFocusDistance * halfFilmHeight
        / max(GetTanHalfFov() * safeFocusDistance + halfFilmHeight, 0.0001);
    float manualFocalLength = MinFocalLength + ManualFocalLength * ManualFocalLengthScale;
    float focalLength = lerp(automaticFocalLength, manualFocalLength, saturate(ManualFocusMode));
    focalLength += (BokehPlus - BokehMinus) * BokehFocalLengthScale;
    return clamp(focalLength, MinFocalLength, MaxFocalLength);
}

// ikBokeh의 Scheimpflug 방식 틸트 초점면에 맞춰 카메라 거리를 다시 계산한다.
float CalculateTiltedCameraDistance(float2 uv, float cameraDistance, float focusDistance) {
    float tiltAmount = saturate(max(TiltFocusDown, TiltFocusUp));
    if (tiltAmount <= BokehColorEpsilon)
        return cameraDistance;

    float3 tiltUp = normalize(TiltFocusDirection);
    if (dot(tiltUp, CameraWorldDirection.xyz) <= 0.0)
        tiltUp = -tiltUp;
    float3 perpendicularDirection = CameraWorldDirection.xyz
        - tiltUp * dot(CameraWorldDirection.xyz, tiltUp);
    float perpendicularLength = length(perpendicularDirection);
    float3 tiltDirection = perpendicularLength > BokehColorEpsilon
        ? perpendicularDirection / perpendicularLength : CameraWorldDirection.xyz;
    if (TiltFocusDown > TiltFocusUp)
        tiltDirection = tiltUp;

    float2 focusUv = CalculateMeasuringPosition();
    float3 focusPosition = ReconstructWorldPosition(focusUv, focusDistance);
    float3 virtualCameraPosition = focusPosition - tiltDirection * focusDistance;
    float3 worldPosition = ReconstructWorldPosition(uv, cameraDistance);
    float tiltedDistance = dot(worldPosition - virtualCameraPosition, tiltDirection);
    return lerp(cameraDistance, max(tiltedDistance, MinFocusDistance), tiltAmount);
}

// 현재 픽셀에서 CoC 계산에 사용할 틸트 보정 카메라 거리를 읽는다.
float ReadCircleOfConfusionCameraDistance(float2 uv, float focusDistance) {
    float cameraDistance = ReadCameraDistance(uv);
    return CalculateTiltedCameraDistance(uv, cameraDistance, focusDistance);
}

// 광학식 CoC를 화면 픽셀 반경으로 계산한다.
float CalculateCircleOfConfusionPixels(float cameraDistance, float focusDistance) {
    float safeFocusDistance = max(focusDistance, MinFocusDistance);
    float focalLength = CalculateFocalLength(safeFocusDistance);
    float apertureDiameter = focalLength / CalculateFNumber();
    float magnification = focalLength / max(safeFocusDistance - focalLength, 0.0001);
    float distanceToPixels = ViewportSize.y / max(FilmHeight, 0.0001);
    float coefficient1 = -safeFocusDistance * magnification * apertureDiameter * distanceToPixels;
    float coefficient2 = magnification * apertureDiameter * distanceToPixels;
    float coc = coefficient1 / max(cameraDistance, 0.0001) + coefficient2;
    float cocSign = sign(coc);
    coc = cocSign * max(abs(coc) - FocusRangeParam * 10.0, 0.0);
    if (coc < 0.0)
        coc *= 1.0 - FrontBokehMinus;

    float cocMagnitude = abs(coc);
    if (cocMagnitude >= 1.0)
        coc = sign(coc) * ((cocMagnitude - 1.0) * (CoCSize + 1.0) + 1.0);
    return clamp(coc, -MaxBlurPixels, MaxBlurPixels);
}

// 픽셀 단위 CoC를 중간 렌더 타깃의 -1~1 범위로 인코딩한다.
float EncodeCircleOfConfusion(float cocPixels) {
    return clamp(cocPixels / MaxBlurPixels, -1.0, 1.0);
}

// 중간 렌더 타깃에 저장된 CoC를 픽셀 반경으로 복원한다.
float DecodeCircleOfConfusion(float encodedCoc) {
    return clamp(encodedCoc, -1.0, 1.0) * MaxBlurPixels;
}

// 2x2 축소 영역에서 전경을 먼저 확장하고, 전경이 없으면 가장 강한 후경 CoC를 선택한다.
float ResolveDominantCircleOfConfusion(float coc0, float coc1, float coc2, float coc3) {
    float foregroundMagnitude = max(max(-coc0, -coc1), max(-coc2, -coc3));
    float backgroundMagnitude = max(max(coc0, coc1), max(coc2, coc3));
    return foregroundMagnitude > ForegroundDilationPixels ? -foregroundMagnitude : max(backgroundMagnitude, 0.0);
}

// 지배적인 CoC와 같은 깊이 층의 색상만 축소 필터에 참여시킨다.
float CalculateDownsampleLayerWeight(float coc, float dominantCoc) {
    if (abs(dominantCoc) <= BokehColorEpsilon)
        return 1.0;
    if (dominantCoc < 0.0)
        return coc < 0.0 ? abs(coc) : 0.0;
    return coc >= 0.0 ? abs(coc) : 0.0;
}

// 픽셀에 고정된 8단계 회전값으로 샘플 무늬를 분산하면서 프레임 간 깜빡임을 막는다.
float2 CalculateBokehKernelRotation(float2 pixelPosition) {
    float noise = frac(52.9829189 * frac(dot(floor(pixelPosition), float2(0.06711056, 0.00583715))));
    float angle = floor(noise * 8.0) * 0.7853981634;
    float sine;
    float cosine;
    sincos(angle, sine, cosine);
    return float2(cosine, sine);
}

// 원판 샘플에 둥근 8각 조리개 형태와 픽셀별 회전을 적용한다.
float2 TransformBokehKernelOffset(float2 offset, float2 rotation) {
    float radius = length(offset);
    if (radius <= BokehColorEpsilon)
        return 0.0;
    float2 direction = abs(offset) / radius;
    float octagonMetric = max(max(direction.x, direction.y), (direction.x + direction.y) * 0.7071067812);
    float apertureScale = 0.9238795325 / max(octagonMetric, BokehColorEpsilon);
    float2 shapedOffset = offset * lerp(1.0, apertureScale, ApertureShapeStrength);
    float rotatedX = shapedOffset.x * rotation.x - shapedOffset.y * rotation.y;
    float rotatedY = shapedOffset.x * rotation.y + shapedOffset.y * rotation.x;
    return float2(rotatedX, rotatedY);
}

// 보케 색에 적용할 강조 지수를 계산한다.
float CalculateEmphasizeRate() {
    if (Emphasize <= 0.0)
        return 0.0;
    return saturate(Emphasize) * EmphasizeRateScale + 1.0;
}

// 최종 합성에서 강조 색을 원래 범위로 되돌릴 역지수를 계산한다.
float CalculateDeemphasizeRate() {
    float emphasizeRate = CalculateEmphasizeRate();
    return emphasizeRate > 0.0 ? 1.0 / max(emphasizeRate, BokehColorEpsilon) : 0.0;
}

// 블러 전에 색을 선형화하고 밝은 색의 대비를 강조한다.
float3 PrepareBokehColor(float3 color) {
    float3 preparedColor = pow(max(color, BokehColorEpsilon), BokehColorGamma);
    float emphasizeRate = CalculateEmphasizeRate();
    return emphasizeRate > 0.0
        ? pow(max(preparedColor, BokehColorEpsilon), emphasizeRate) : preparedColor;
}

// 블러된 색의 강조를 되돌리고 화면 출력 감마로 복원한다.
float3 ResolveBokehColor(float3 color) {
    float deemphasizeRate = CalculateDeemphasizeRate();
    float3 resolvedColor = deemphasizeRate > 0.0
        ? pow(max(color, BokehColorEpsilon), deemphasizeRate) : color;
    return pow(max(resolvedColor, BokehColorEpsilon), 1.0 / BokehColorGamma);
}

// 보케가 넓어질수록 한 픽셀에 기여하는 밝기가 감소하도록 CoC 면적을 보정한다.
float CalculateCircleOfConfusionBrightness(float cocPixels) {
    float radius = max(abs(cocPixels), MinimumCoCRadius);
    return saturate(1.0 / (radius * radius));
}

// 선형 색상에서 밝은 점광원을 찾아 보케 누적 시 더 오래 보존한다.
float CalculateBokehHighlightWeight(float3 color) {
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    float highlight = smoothstep(BokehHighlightThreshold, 1.0, luminance);
    return 1.0 + highlight * BokehHighlightBias;
}

// 같은 표면은 유지하고 전경 보케만 깊이 경계를 넘어 후경 위로 번질 수 있게 한다.
float CalculateDepthAwareUpsampleWeight(float centerDistance, float sampleDistance, float foregroundCoverage) {
    float minimumDistance = max(min(centerDistance, sampleDistance), MinFocusDistance);
    float relativeDifference = abs(centerDistance - sampleDistance) / minimumDistance;
    float depthSimilarity = exp2(-relativeDifference * DepthEdgeFalloff);
    return max(depthSimilarity, saturate(foregroundCoverage));
}

#endif
