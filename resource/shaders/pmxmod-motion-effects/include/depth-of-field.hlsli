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

// 2단계 보케 피라미드가 처리할 최대 흐림 반경이다.
static const float MaxBlurPixels = 16.0;

// 반해상도 보정 패스가 직접 처리할 최대 흐림 반경이다.
static const float MediumBlurPixels = 8.0;

// 첫 번째 CoC target과 중간 보케 보정 target의 해상도 비율이다.
static const float BokehHalfResolutionScale = 0.5;

// 광역 보케 target의 해상도 비율이다.
static const float BokehQuarterResolutionScale = 0.25;

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

// 광역 보케의 Vogel 원판 샘플 개수다.
static const int BroadBokehSampleCount = 32;

// 중간 반경 보케의 Vogel 원판 샘플 개수다.
static const int MediumBokehSampleCount = 20;

// 동심원 띠가 생기지 않도록 원판 전체에 고르게 분산한 광역 보케 커널이다.
static const float2 BroadBokehKernel[BroadBokehSampleCount] = {
    float2(0.000000, 0.000000),
    float2(0.127000, 0.000000),
    float2(-0.162200, 0.148588),
    float2(0.024827, -0.282894),
    float2(0.204442, 0.266658),
    float2(-0.375176, -0.066363),
    float2(0.355400, -0.226076),
    float2(-0.118874, 0.442206),
    float2(-0.226706, -0.436509),
    float2(0.491861, 0.179627),
    float2(-0.511700, 0.211222),
    float2(0.246673, -0.527126),
    float2(0.182285, 0.581154),
    float2(-0.549410, -0.318394),
    float2(0.644520, -0.141696),
    float2(-0.393341, 0.559486),
    float2(-0.090871, -0.701244),
    float2(0.557857, 0.470163),
    float2(-0.750701, 0.031044),
    float2(0.547579, -0.544914),
    float2(-0.036635, 0.792269),
    float2(-0.521023, -0.624360),
    float2(0.825358, 0.111051),
    float2(-0.699324, 0.486572),
    float2(0.191096, -0.849439),
    float2(0.441994, 0.771339),
    float2(-0.864056, -0.275658),
    float2(0.839321, -0.387788),
    float2(-0.363615, 0.868839),
    float2(-0.324518, -0.902243),
    float2(0.863508, 0.453836),
    float2(-0.959140, 0.252826)
};

// 반해상도에서 작은 보케를 균일하게 복원하는 중간 반경 커널이다.
static const float2 MediumBokehKernel[MediumBokehSampleCount] = {
    float2(0.000000, 0.000000),
    float2(0.162221, 0.000000),
    float2(-0.207183, 0.189796),
    float2(0.031713, -0.361349),
    float2(0.261140, 0.340611),
    float2(-0.479225, -0.084768),
    float2(0.453964, -0.288774),
    float2(-0.151842, 0.564844),
    float2(-0.289579, -0.557567),
    float2(0.628271, 0.229443),
    float2(-0.653611, 0.269801),
    float2(0.315084, -0.673316),
    float2(0.232839, 0.742327),
    float2(-0.701779, -0.406695),
    float2(0.823267, -0.180993),
    float2(-0.502427, 0.714650),
    float2(-0.116072, -0.895721),
    float2(0.712570, 0.600554),
    float2(-0.958895, 0.039653),
    float2(0.699441, -0.696037)
};

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
    float depthMin0 = min(min(depth0, depth1), min(depth2, depth3));
    float depthMin1 = min(min(depth4, depth5), min(depth6, depth7));
    return max(min(min(depthMin0, depthMin1), depth8), MinFocusDistance);
}

float ReadDelayedFocusDistance() {
    float4 history = FocusHistory.Sample(LinearClamp, float2(0.5, 0.5));
    float focusDistance = ReadAutoFocusDistance();
    if (abs(history.b - FocusHistoryMarker) < 0.01)
        focusDistance = history.r / max(GetTanHalfFov(), 0.0001);
    return max(focusDistance, MinFocusDistance);
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
