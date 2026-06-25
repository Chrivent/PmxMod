// DOF 초점 히스토리 갱신 패스:
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

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    float targetFocusDepth = ReadAutoFocusDepth();
    float4 previous = FocusHistory.Sample(LinearClamp, float2(0.5, 0.5));
    float focusDepth = targetFocusDepth;
    float velocity = 0.0;
    if (abs(previous.b - FocusHistoryMarker) < 0.01) {
        focusDepth = previous.r;
        velocity = previous.g;
    }

    velocity *= pow(max(0.8 * FocusSlip, 0.0001), FocusDeltaTime * 30.0);
    float remaining = targetFocusDepth - (focusDepth + velocity);
    float speedLimit = lerp(0.015, 0.12, saturate(1.0 - targetFocusDepth)) * FocusDeltaTime * 30.0;
    float speed = min(abs(remaining), speedLimit);
    velocity += sign(remaining) * speed * saturate(1.0 - FocusDelay);
    focusDepth = saturate(focusDepth + velocity);
    return float4(focusDepth, velocity, FocusHistoryMarker, 1.0);
}
