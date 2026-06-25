// 포스트 프로세스 입력 규격:
// t0 = SceneColor, t1 = SceneDepth, s0 = LinearClamp.
Texture2D SceneColor : register(t0);
Texture2D SceneDepth : register(t1);
SamplerState LinearClamp : register(s0);

static const float DepthPower = 0.35;

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
    const float depth = saturate(SceneDepth.Sample(LinearClamp, input.uv).r);
    const float visibleDepth = pow(saturate(1.0 - depth), DepthPower);
    return float4(visibleDepth.xxx, 1.0);
}
