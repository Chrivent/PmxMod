// 포스트 프로세스 입력 규격:
// t0 = SceneColor, t1 = SceneDepth, t2 = FocusHistory, s0 = LinearClamp.
Texture2D SceneColor : register(t0);
Texture2D SceneDepth : register(t1);
Texture2D FocusHistory : register(t2);
SamplerState LinearClamp : register(s0);

#include "dof-parameters.hlsli"

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

float ReadDelayedFocusDepth(float targetFocusDepth) {
    float4 history = FocusHistory.Sample(LinearClamp, float2(0.5, 0.5));
    if (abs(history.b - FocusHistoryMarker) >= 0.01)
        return targetFocusDepth;
    return history.r;
}

float LinearizeDepth(float depth) {
    return NearPlane * FarPlane / max(FarPlane - depth * (FarPlane - NearPlane), 0.0001);
}

float CalculateCircleOfConfusion(float depth, float focusDepth) {
    float linearDepth = LinearizeDepth(depth);
    float linearFocusDepth = LinearizeDepth(focusDepth);
    float depthDelta = (linearDepth - linearFocusDepth) / max(linearFocusDepth, 0.0001);
    float sideScale = depthDelta < 0.0 ? 1.0 - FrontBokehMinus : 1.0;
    float focusDeadZone = FocusRangeParam * 0.1;
    float cocScale = (1.0 + CoCSize) * (1.0 + BokehPlus - BokehMinus);
    float coc = max(abs(depthDelta) - focusDeadZone, 0.0) / max(FocusRangeBase, 0.0001);
    return saturate(coc * cocScale) * sideScale;
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
        float highlightWeight = 1.0 + saturate(luminance - HighlightThreshold) * Emphasize;
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
    float targetFocusDepth = ReadAutoFocusDepth();
    float focusDepth = ReadDelayedFocusDepth(targetFocusDepth);
    float coc = CalculateCircleOfConfusion(depth, focusDepth);
    float blurPixels = coc * MaxBlurPixels;
    float3 bokehColor = SampleBokeh(input.uv, texelSize, blurPixels);
    float blendAmount = smoothstep(0.02, 1.0, coc);
    return float4(lerp(sceneColor.rgb, bokehColor, blendAmount), sceneColor.a);
}
