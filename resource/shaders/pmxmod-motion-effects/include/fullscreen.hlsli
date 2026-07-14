#ifndef PMXMOD_FULLSCREEN_HLSLI
#define PMXMOD_FULLSCREEN_HLSLI

struct FullscreenVertexOutput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// 정점 버퍼 없이 전체 화면 삼각형을 생성한다.
FullscreenVertexOutput VSMain(uint vertexId : SV_VertexID) {
    FullscreenVertexOutput output;
    output.uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

#endif
