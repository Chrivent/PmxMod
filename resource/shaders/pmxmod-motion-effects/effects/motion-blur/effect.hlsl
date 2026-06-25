// 포스트 프로세스 입력 규격:
// t0 = SceneColor, t1 = SceneDepth, s0 = LinearClamp.
Texture2D SceneColor : register(t0);
SamplerState LinearClamp : register(s0);

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
    // TODO: MotionBlur3 계열처럼 이전 프레임 뷰-프로젝션 행렬과 현재 깊이로 화면 공간 속도를 계산한다.
    // TODO: 오브젝트/카메라 이동량에 따른 샘플 개수와 셔터 강도를 패키지 파라미터로 노출한다.
    // TODO: 깊이 불연속 영역에서 번짐이 새지 않도록 depth-aware clamp를 추가한다.
    // TODO: t1 SceneDepth를 실제 depth texture에 연결한 뒤 depth/previous matrix 기반 velocity를 계산한다.
    return SceneColor.Sample(LinearClamp, input.uv);
}
