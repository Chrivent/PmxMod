// DOF 초점 히스토리 갱신 패스:
// t1 = SceneDepth, t2 = FocusHistory, s0 = LinearClamp.
Texture2D SceneDepth : register(t1);
Texture2D FocusHistory : register(t2);
SamplerState LinearClamp : register(s0);

#include "../../include/depth-of-field.hlsli"

float4 PSMain(FullscreenVertexOutput input) : SV_Target {
    float focusDeltaTime = clamp(FrameDeltaTime, 0.0, 1.0 / 15.0);
    float tanHalfFov = GetTanHalfFov();
    float minFocusState = MinFocusDistance * tanHalfFov;
    float targetFocusState = max(ReadTargetFocusDistance() * tanHalfFov, minFocusState);
    float4 previous = FocusHistory.Sample(LinearClamp, float2(0.5, 0.5));
    float focusState = targetFocusState;
    float velocity = 0.0;
    if (FrameHistoryReset <= 0.5 && abs(previous.b - FocusHistoryMarker) < 0.01) {
        focusState = previous.r;
        velocity = previous.g;
    }

    if (focusDeltaTime > 0.0) {
        velocity *= pow(max(0.8 * FocusSlip, 0.0001), focusDeltaTime * 30.0);
        float remaining = targetFocusState - (focusState + velocity * focusDeltaTime);
        float response = saturate(1.0 - FocusDelay);
        float acceleration = remaining * 900.0 * response;
        float speedLimit = clamp(35000.0 / max(targetFocusState, 0.0001), 50.0, 1000.0) * 30.0;
        velocity = clamp(velocity + acceleration * focusDeltaTime, -speedLimit, speedLimit);
        focusState += velocity * focusDeltaTime;
        if (focusState < minFocusState) {
            focusState = minFocusState;
            velocity = max(velocity, 0.0);
        }
    }
    return float4(focusState, velocity, FocusHistoryMarker, 1.0);
}
