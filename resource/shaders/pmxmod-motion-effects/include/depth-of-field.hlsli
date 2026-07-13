// ikBokeh 계열 DOF에서 effect.hlsl과 focus-update.hlsl이 공유하는 임시 파라미터다.
// 나중에 UI와 모션 파라미터 시스템이 생기면 상수 버퍼나 패키지 파라미터로 교체한다.

// 보케를 강하게 만드는 ikBokeh의 "Bokeh+" 값이다.
static const float BokehPlus = 0.15;

// 보케를 약하게 만드는 ikBokeh의 "Bokeh-" 값이다.
static const float BokehMinus = 0.0;

// 전경 보케를 약하게 만드는 ikBokeh의 "Front Bokeh-" 값이다.
static const float FrontBokehMinus = 0.0;

// 계산된 CoC를 추가 확대하는 ikBokeh의 "CoC size" 값이다.
static const float CoCSize = 0.7;

// 자동 초점 측정 영역을 정하는 값이다. 0은 한 점, 0.5는 좁은 영역, 1은 넓은 영역을 사용한다.
static const float AutoMeasuringMode = 1.0;

// 초점이 맞는 범위를 픽셀 단위로 넓히는 ikBokeh의 "Focus range" 값이다.
static const float FocusRangeParam = 0.0;

// 밝은 보케 색을 강조하는 ikBokeh의 "emphasize" 값이다.
static const float Emphasize = 0.0;

// 초점 이동의 지연 강도다. 0이면 즉시 따라가고 1에 가까울수록 천천히 접근한다.
static const float FocusDelay = 0.5;

// 초점 이동 관성의 유지 비율이다. 0이면 속도가 빠르게 사라지고 1에 가까울수록 오래 유지된다.
static const float FocusSlip = 0.5;

// MMD 컨트롤러가 없는 동안 자동 초점을 측정할 화면 좌표다.
static const float2 FocusUv = float2(0.5, 0.48);

// 단일 패스 보케 커널이 처리할 최대 흐림 반경이다.
static const float MaxBlurPixels = 8.0;

// CoC와 보케를 계산하는 중간 target의 해상도 비율이다.
static const float BokehDownsampleScale = 0.5;

// 밝은 보케 강조를 시작할 휘도 기준이다.
static const float HighlightThreshold = 0.68;

// 초점 히스토리 텍스처에 유효한 데이터가 저장되었는지 판별하는 표식이다.
static const float FocusHistoryMarker = 0.5;

// MMD 좌표계에서 1미터에 해당하는 길이다.
static const float MmdUnitsPerMeter = 1.0 / 0.08;

// MMD 좌표계에서 1밀리미터에 해당하는 길이다.
static const float MmdUnitsPerMillimeter = MmdUnitsPerMeter * 0.001;

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

float ReadDepth(float2 uv) {
    return saturate(SceneDepth.Sample(LinearClamp, uv).r);
}

float GetTanHalfFov() {
    return tan(max(CameraVerticalFovRadians, 0.001) * 0.5);
}

float LinearizeDepth(float depth) {
    return CameraNearPlane * CameraFarPlane
        / max(CameraFarPlane - depth * (CameraFarPlane - CameraNearPlane), 0.0001);
}

float DepthToCameraDistance(float depth, float2 uv) {
    if (depth >= 0.999999)
        return CameraFarPlane;

    float viewDepth = LinearizeDepth(depth);
    float aspectRatio = ViewportSize.x / max(ViewportSize.y, 1.0);
    float2 ndc = uv * 2.0 - 1.0;
    float2 rayOffset = ndc * float2(aspectRatio, 1.0) * GetTanHalfFov();
    return viewDepth * length(float3(rayOffset, 1.0));
}

float ReadCameraDistance(float2 uv) {
    return DepthToCameraDistance(ReadDepth(uv), uv);
}

float MeasuringCircleRadius() {
    return AutoMeasuringMode > 0.75 ? 0.2 : 0.05;
}

float ReadAutoFocusDistance() {
    if (AutoMeasuringMode < 0.25)
        return ReadCameraDistance(FocusUv);

    float r1 = MeasuringCircleRadius();
    float r2 = r1 * 0.714;
    float depth0 = ReadCameraDistance(FocusUv + float2(-r2, -r2));
    float depth1 = ReadCameraDistance(FocusUv + float2(-r1, 0.0));
    float depth2 = ReadCameraDistance(FocusUv + float2(-r2, r2));
    float depth3 = ReadCameraDistance(FocusUv + float2(0.0, -r1));
    float depth4 = ReadCameraDistance(FocusUv);
    float depth5 = ReadCameraDistance(FocusUv + float2(0.0, r1));
    float depth6 = ReadCameraDistance(FocusUv + float2(r2, -r2));
    float depth7 = ReadCameraDistance(FocusUv + float2(r1, 0.0));
    float depth8 = ReadCameraDistance(FocusUv + float2(r2, r2));
    float4 depthMin = min(float4(depth0, depth1, depth2, depth3), float4(depth4, depth5, depth6, depth7));
    depthMin.xy = min(depthMin.xy, depthMin.zw);
    return max(min(min(depthMin.x, depthMin.y), depth8), MinFocusDistance);
}

float ReadDelayedFocusDistance() {
    float4 history = FocusHistory.Sample(LinearClamp, float2(0.5, 0.5));
    if (abs(history.b - FocusHistoryMarker) >= 0.01)
        return ReadAutoFocusDistance();
    return max(history.r / max(GetTanHalfFov(), 0.0001), MinFocusDistance);
}

float CalculateFNumber() {
    float fNumber = DefaultFNumber + (BokehMinus + 0.5 - BokehPlus) * FNumberScale;
    return clamp(fNumber, 1.0, 16.0);
}

float CalculateFocalLength(float focusDistance) {
    float safeFocusDistance = max(focusDistance, MinFocusDistance);
    float halfFilmHeight = FilmHeight * 0.5;
    float focalLength = safeFocusDistance * halfFilmHeight
        / max(GetTanHalfFov() * safeFocusDistance + halfFilmHeight, 0.0001);
    focalLength += (BokehPlus - BokehMinus) * BokehFocalLengthScale;
    return clamp(focalLength, MinFocalLength, MaxFocalLength);
}

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

float EncodeCircleOfConfusion(float cocPixels) {
    return clamp(cocPixels / MaxBlurPixels, -1.0, 1.0);
}

float DecodeCircleOfConfusion(float encodedCoc) {
    return clamp(encodedCoc, -1.0, 1.0) * MaxBlurPixels;
}
