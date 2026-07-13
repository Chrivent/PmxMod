#include "post-process-frame.hlsli"

// ikBokeh 계열 DOF에서 effect.hlsl과 focus-update.hlsl이 공유하는 임시 파라미터다.
// 나중에 UI/모션 키 파라미터 시스템이 생기면 이 값들을 상수 버퍼나 패키지 파라미터로 옮긴다.

// 보케를 강하게 만드는 ikBokeh의 "Bokeh+" 값이다.
// 현재 단일 패스 버전에서는 CoC 스케일로 반영한다.
static const float BokehPlus = 0.15;

// 보케를 약하게 만드는 ikBokeh의 "Bokeh-" 값이다.
// 사용자가 별도로 지정하지 않은 값이므로 원본 컨트롤러 기본값인 0으로 둔다.
static const float BokehMinus = 0.0;

// 전경 보케를 약하게 만드는 ikBokeh의 "Front Bokeh-" 값이다.
// 1에 가까울수록 초점보다 앞에 있는 물체의 흐림을 줄인다.
static const float FrontBokehMinus = 0.0;

// 계산된 CoC를 추가 확대하는 ikBokeh의 "CoC size" 값이다.
// 0이면 기본 크기, 1이면 대략 2배 쪽으로 커진다.
static const float CoCSize = 0.7;

// 자동 초점 측정 모드다.
// 0.0은 고정 초점, 0.5는 좁은 측정 영역, 1.0은 넓은 측정 영역을 의미한다.
static const float AutoMeasuringMode = 1.0;

// 초점이 맞는 범위를 넓히는 ikBokeh의 "Focus range" 값이다.
// 사용자가 별도로 지정하지 않은 값이므로 원본 컨트롤러 기본값인 0으로 둔다.
static const float FocusRangeParam = 0.0;

// 밝은 보케 색을 강조하는 ikBokeh의 emphasize 값이다.
// 사용자가 별도로 지정하지 않은 값이므로 원본 컨트롤러 기본값인 0으로 둔다.
static const float Emphasize = 0.0;

// 초점 지연 강도다. 0이면 즉시 따라가고, 1에 가까울수록 목표 초점에 천천히 접근한다.
static const float FocusDelay = 0.5;

// 초점 이동 관성 유지량이다. 0이면 이전 속도가 거의 사라지고, 1에 가까울수록 이전 속도가 오래 유지된다.
static const float FocusSlip = 0.5;

// 현재는 MMD 컨트롤러가 없으므로 측정 중심을 화면 중앙 근처로 둔다.
static const float2 FocusUv = float2(0.5, 0.48);

// 현재 단일 패스 DOF에서 CoC를 정규화하는 기준 폭이다.
static const float FocusRangeBase = 0.18;

// 최대 blur 반경이다. ikBokeh 원본의 기본 blur 단위와 비슷한 크기로 둔다.
static const float MaxBlurPixels = 8.0;

// 밝은 보케 강조가 시작되는 밝기 기준이다.
static const float HighlightThreshold = 0.68;

// 히스토리 텍스처가 유효한 초점 데이터인지 구분하기 위한 표식이다.
static const float FocusHistoryMarker = 0.5;

float ReadDepth(float2 uv) {
    return saturate(SceneDepth.Sample(LinearClamp, uv).r);
}

float MeasuringCircleRadius() {
    return AutoMeasuringMode > 0.75 ? 0.2 : 0.05;
}

float ReadAutoFocusDepth() {
    if (AutoMeasuringMode < 0.25)
        return ReadDepth(FocusUv);

    float r1 = MeasuringCircleRadius();
    float r2 = r1 * 0.714;
    float depth0 = ReadDepth(FocusUv + float2(-r2, -r2));
    float depth1 = ReadDepth(FocusUv + float2(-r1, 0.0));
    float depth2 = ReadDepth(FocusUv + float2(-r2, r2));
    float depth3 = ReadDepth(FocusUv + float2(0.0, -r1));
    float depth4 = ReadDepth(FocusUv);
    float depth5 = ReadDepth(FocusUv + float2(0.0, r1));
    float depth6 = ReadDepth(FocusUv + float2(r2, -r2));
    float depth7 = ReadDepth(FocusUv + float2(r1, 0.0));
    float depth8 = ReadDepth(FocusUv + float2(r2, r2));
    float4 depthMin = min(float4(depth0, depth1, depth2, depth3), float4(depth4, depth5, depth6, depth7));
    depthMin.xy = min(depthMin.xy, depthMin.zw);
    return min(min(depthMin.x, depthMin.y), depth8);
}
