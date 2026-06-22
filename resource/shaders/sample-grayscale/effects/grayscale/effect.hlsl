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
    const float4 sceneColor = SceneColor.Sample(LinearClamp, input.uv);
    const float luminance = dot(sceneColor.rgb, float3(0.2126, 0.7152, 0.0722));
    return float4(luminance.xxx, sceneColor.a);
}
