// 포스트 프로세스 입력 규격:
// t0 = SceneColor, t1 = SceneDepth, s0 = LinearClamp.
Texture2D SceneColor : register(t0);
SamplerState LinearClamp : register(s0);

static const float2 FocusCenter = float2(0.5, 0.48);
static const float ScreenFocusRadius = 0.18;
static const float ScreenFocusFeather = 0.42;
static const float MaxBlurPixels = 5.0;
static const float HighlightBoost = 0.28;

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

float CalculateScreenBlur(float2 uv) {
    float2 aspectUv = uv - FocusCenter;
    aspectUv.x *= 1.35;
    float distanceFromFocus = length(aspectUv);
    return saturate((distanceFromFocus - ScreenFocusRadius) / max(ScreenFocusFeather, 0.0001));
}

float3 SampleBokeh(float2 uv, float2 texelSize, float blurPixels) {
    static const float2 offsets[16] = {
        float2(0.0000, 0.0000),
        float2(0.3536, 0.3536),
        float2(-0.5000, 0.0000),
        float2(0.3536, -0.3536),
        float2(0.0000, 0.6500),
        float2(-0.4596, -0.4596),
        float2(0.7500, 0.0000),
        float2(-0.4596, 0.4596),
        float2(0.0000, -0.9000),
        float2(0.7071, 0.7071),
        float2(-1.0000, 0.0000),
        float2(0.7071, -0.7071),
        float2(-0.3827, 0.9239),
        float2(-0.9239, -0.3827),
        float2(0.3827, -0.9239),
        float2(0.9239, 0.3827)
    };
    float3 accumulatedColor = 0.0;
    float accumulatedWeight = 0.0;
    for (int index = 0; index < 16; index++) {
        float2 sampleUv = uv + offsets[index] * texelSize * blurPixels;
        float3 color = SceneColor.Sample(LinearClamp, sampleUv).rgb;
        float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
        float highlightWeight = 1.0 + saturate(luminance - 0.65) * HighlightBoost;
        accumulatedColor += color * highlightWeight;
        accumulatedWeight += highlightWeight;
    }
    return accumulatedColor / max(accumulatedWeight, 0.0001);
}

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    float2 texelSize = max(abs(ddx(input.uv)), abs(ddy(input.uv)));
    float4 sceneColor = SceneColor.Sample(LinearClamp, input.uv);
    float blurAmount = CalculateScreenBlur(input.uv);
    float blurPixels = blurAmount * MaxBlurPixels;
    float3 bokehColor = SampleBokeh(input.uv, texelSize, blurPixels);
    float3 color = lerp(sceneColor.rgb, bokehColor, smoothstep(0.0, 1.0, blurAmount));
    return float4(color, sceneColor.a);
}
