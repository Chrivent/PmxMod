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
    // TODO: ikBokeh 계열처럼 depth에서 circle of confusion을 계산하고 초점 거리/조리개/블러 반경을 파라미터화한다.
    // TODO: 전경 번짐과 배경 번짐을 분리하고, 밝은 픽셀에 보케 가중치를 줄 수 있는 downsample pass를 추가한다.
    // TODO: 큰 반경 블러를 위해 단일 패스가 아니라 downsample/blur/composite 다중 패스 구조를 패키지 포맷에 반영한다.
    // TODO: 현재 포스트 프로세스 입력이 SceneColor뿐이라 depth texture와 카메라 초점 파라미터 입력을 셰이더 시스템에 추가해야 한다.
    return SceneColor.Sample(LinearClamp, input.uv);
}
