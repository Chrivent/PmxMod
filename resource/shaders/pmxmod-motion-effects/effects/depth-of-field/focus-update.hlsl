// DOF 초점 히스토리 갱신 패스:
// t0 = SceneColor, t1 = SceneDepth, t2 = FocusHistory, s0 = LinearClamp.
Texture2D SceneColor : register(t0);
Texture2D SceneDepth : register(t1);
Texture2D FocusHistory : register(t2);
SamplerState LinearClamp : register(s0);

#include "../../include/post-process-frame.hlsli"
#include "../../include/depth-of-field.hlsli"

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

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    float focusDeltaTime = FrameDeltaTime <= 0.0 ? 0.0 : clamp(FrameDeltaTime, 1.0 / 120.0, 1.0 / 15.0);
    float tanHalfFov = GetTanHalfFov();
    float minFocusState = MinFocusDistance * tanHalfFov;
    float targetFocusState = max(ReadAutoFocusDistance() * tanHalfFov, minFocusState);
    float4 previous = FocusHistory.Sample(LinearClamp, float2(0.5, 0.5));
    float focusState = targetFocusState;
    float velocity = 0.0;
    if (abs(previous.b - FocusHistoryMarker) < 0.01) {
        focusState = previous.r;
        velocity = previous.g;
    }

    velocity *= pow(max(0.8 * FocusSlip, 0.0001), focusDeltaTime * 30.0);
    float remaining = targetFocusState - (focusState + velocity);
    float speedLimit = clamp(35000.0 / max(targetFocusState, 0.0001), 50.0, 1000.0) * 30.0 * focusDeltaTime;
    float speed = min(abs(remaining), speedLimit);
    velocity += sign(remaining) * speed * saturate(1.0 - FocusDelay);
    focusState = max(focusState + velocity, minFocusState);
    return float4(focusState, velocity, FocusHistoryMarker, 1.0);
}
