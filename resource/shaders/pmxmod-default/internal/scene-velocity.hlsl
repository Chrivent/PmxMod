// 장면 속도 프리패스 입력 규격:
// POSITION0 = 현재 스키닝 위치, POSITION1 = 직전 프레임 스키닝 위치, b0 = 장면 속도 상수.
cbuffer SceneVelocityVertexConstants : register(b0) {
    float4x4 CurrentWvp;
    float4x4 PreviousWvp;
}

struct SceneVelocityVertexInput {
    float3 position : POSITION0;
    float3 previousPosition : POSITION1;
};

struct SceneVelocityVertexOutput {
    float4 position : SV_POSITION;
    float4 currentClipPosition : TEXCOORD0;
    float4 previousClipPosition : TEXCOORD1;
};

SceneVelocityVertexOutput VSMain(SceneVelocityVertexInput input) {
    SceneVelocityVertexOutput output;
    output.currentClipPosition = mul(CurrentWvp, float4(input.position, 1.0));
    output.previousClipPosition = mul(PreviousWvp, float4(input.previousPosition, 1.0));
    output.position = output.currentClipPosition;
    return output;
}

float2 CalculateVelocity(SceneVelocityVertexOutput input, float2 clipToUvScale) {
    const float2 currentNdc = input.currentClipPosition.xy / max(abs(input.currentClipPosition.w), 0.00001);
    const float2 previousNdc = input.previousClipPosition.xy / max(abs(input.previousClipPosition.w), 0.00001);
    return (currentNdc - previousNdc) * clipToUvScale;
}

float2 PSMain(SceneVelocityVertexOutput input) : SV_Target0 {
    return CalculateVelocity(input, float2(0.5, 0.5));
}

float2 PSMainInvertedY(SceneVelocityVertexOutput input) : SV_Target0 {
    return CalculateVelocity(input, float2(0.5, -0.5));
}
