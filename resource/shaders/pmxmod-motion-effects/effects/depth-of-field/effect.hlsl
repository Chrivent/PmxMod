// 포스트 프로세스 입력 규격:
// t0 = SceneColor, t1 = SceneDepth, s0 = LinearClamp.
Texture2D SceneColor : register(t0);
Texture2D SceneDepth : register(t1);
SamplerState LinearClamp : register(s0);

// TODO: UI/모션 키가 생기면 아래 임시 파라미터들을 effect parameter로 이동한다.
static const float2 FocusUv = float2(0.5, 0.48);
static const float NearPlane = 0.1;
static const float FarPlane = 1000.0;
static const float FocusRange = 0.18;
static const float ForegroundBlurScale = 0.75;
static const float BackgroundBlurScale = 1.0;
static const float MaxBlurPixels = 8.0;
static const float HighlightThreshold = 0.68;
static const float HighlightBoost = 0.65;

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

float ReadDepth(float2 uv) {
    return saturate(SceneDepth.Sample(LinearClamp, uv).r);
}

float LinearizeDepth(float depth) {
    return NearPlane * FarPlane / max(FarPlane - depth * (FarPlane - NearPlane), 0.0001);
}

float CalculateCircleOfConfusion(float depth, float focusDepth) {
    float linearDepth = LinearizeDepth(depth);
    float linearFocusDepth = LinearizeDepth(focusDepth);
    float depthDelta = (linearDepth - linearFocusDepth) / max(linearFocusDepth, 0.0001);
    float sideScale = depthDelta < 0.0 ? ForegroundBlurScale : BackgroundBlurScale;
    return saturate(abs(depthDelta) / max(FocusRange, 0.0001)) * sideScale;
}

float3 SampleBokeh(float2 uv, float2 texelSize, float blurPixels) {
    static const float2 offsets[24] = {
        float2(0.0000, 0.0000),
        float2(0.3827, 0.9239),
        float2(-0.7071, 0.7071),
        float2(-0.9239, -0.3827),
        float2(0.0000, -1.0000),
        float2(0.9239, -0.3827),
        float2(0.7071, 0.7071),
        float2(-0.3827, 0.9239),
        float2(0.7500, 0.0000),
        float2(0.5303, 0.5303),
        float2(0.0000, 0.7500),
        float2(-0.5303, 0.5303),
        float2(-0.7500, 0.0000),
        float2(-0.5303, -0.5303),
        float2(0.0000, -0.7500),
        float2(0.5303, -0.5303),
        float2(1.2500, 0.0000),
        float2(0.8839, 0.8839),
        float2(0.0000, 1.2500),
        float2(-0.8839, 0.8839),
        float2(-1.2500, 0.0000),
        float2(-0.8839, -0.8839),
        float2(0.0000, -1.2500),
        float2(0.8839, -0.8839)
    };

    float3 accumulatedColor = 0.0;
    float accumulatedWeight = 0.0;
    for (int index = 0; index < 24; index++) {
        float2 sampleUv = uv + offsets[index] * texelSize * blurPixels;
        float3 color = SceneColor.Sample(LinearClamp, sampleUv).rgb;
        float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
        float highlightWeight = 1.0 + saturate(luminance - HighlightThreshold) * HighlightBoost;
        float centerWeight = index == 0 ? 1.5 : 1.0;
        float weight = highlightWeight * centerWeight;
        accumulatedColor += color * weight;
        accumulatedWeight += weight;
    }
    return accumulatedColor / max(accumulatedWeight, 0.0001);
}

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    float2 texelSize = max(abs(ddx(input.uv)), abs(ddy(input.uv)));
    float4 sceneColor = SceneColor.Sample(LinearClamp, input.uv);
    float depth = ReadDepth(input.uv);
    float focusDepth = ReadDepth(FocusUv);
    float coc = CalculateCircleOfConfusion(depth, focusDepth);
    float blurPixels = coc * MaxBlurPixels;
    float3 bokehColor = SampleBokeh(input.uv, texelSize, blurPixels);
    float blendAmount = smoothstep(0.02, 1.0, coc);
    return float4(lerp(sceneColor.rgb, bokehColor, blendAmount), sceneColor.a);
}
