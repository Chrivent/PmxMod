cbuffer VSData : register(b0) {
    float4x4 wv;
    float4x4 wvp;
};

cbuffer PSData : register(b1) {
    float4 texMulFactor;
    float4 texAddFactor;
    float4 toonTexMulFactor;
    float4 toonTexAddFactor;
    float4 sphereTexMulFactor;
    float4 sphereTexAddFactor;
    int4 textureModes;
    float4 diffuseAlpha;
    float4 ambientSpecularPower;
    float4 specular;
    float4 lightColor;
    float4 lightDir;
};

Texture2D tex : register(t0);
Texture2D toonTex : register(t1);
Texture2D sphereTex : register(t2);
SamplerState texSampler : register(s0);
SamplerState toonTexSampler : register(s1);
SamplerState sphereTexSampler : register(s2);

struct VSInput {
    float3 Pos : POSITION;
    float3 Nor : NORMAL;
    float2 UV : UV;
    float2 SubUV : TEXCOORD1;
};

struct VSOutput {
    float4 Position : SV_POSITION;
    float3 Pos : VIEWPOS;
    float3 Nor : VIEWNORMAL;
    float2 UV : UV;
    float2 SubUV : TEXCOORD1;
};

VSOutput VSMain(VSInput input) {
    VSOutput vsOut;
    vsOut.Position = mul(wvp, float4(input.Pos, 1.0));
    vsOut.Pos = mul(wv, float4(input.Pos, 1.0)).xyz;
    vsOut.Nor = mul((float3x3)wv, input.Nor);
    vsOut.UV = float2(input.UV.x, 1.0 - input.UV.y);
    vsOut.SubUV = input.SubUV;
    return vsOut;
}

float3 ComputeTexMulFactor(float3 texColor, float4 factor) {
    float3 ret = texColor * factor.rgb;
    return lerp(float3(1.0, 1.0, 1.0), ret, factor.a);
}

float3 ComputeTexAddFactor(float3 texColor, float4 factor) {
    float3 ret = texColor + (texColor - (float3)1.0) * factor.a;
    ret = clamp(ret, (float3)0.0, (float3)1.0)+ factor.rgb;
    return ret;
}

float4 PSMain(VSOutput vsOut) : SV_TARGET0 {
    float3 eyeDir = normalize(-vsOut.Pos);
    float3 lightDirection = normalize(-lightDir.xyz);
    float3 nor = normalize(vsOut.Nor);
    float opacity = diffuseAlpha.a;
    // MMD 모델은 기본 재질색에 툰을 곱한다. 미지정 툰은 흰색이므로 별도의 확산 명암을 넣지 않는다.
    float3 color = saturate(ambientSpecularPower.rgb + diffuseAlpha.rgb * lightColor.rgb);
    int texMode = textureModes.x;
    int toonTexMode = textureModes.y;
    int sphereTexMode = textureModes.z;
    if (texMode != 0) {
        float4 texColor = tex.Sample(texSampler, vsOut.UV);
        texColor.rgb = ComputeTexMulFactor(texColor.rgb, texMulFactor);
        texColor.rgb = ComputeTexAddFactor(texColor.rgb, texAddFactor);
        color *= texColor.rgb;
        if (texMode == 2)
            opacity *= texColor.a;
    }
    if (sphereTexMode != 0) {
        float2 spUV = nor.xy * float2(0.5, -0.5) + 0.5;
        float4 sphereSample = sphereTex.Sample(sphereTexSampler, sphereTexMode == 3 ? vsOut.SubUV : spUV);
        opacity *= sphereSample.a;
        float3 spColor = sphereSample.rgb;
        spColor = ComputeTexMulFactor(spColor, sphereTexMulFactor);
        spColor = ComputeTexAddFactor(spColor, sphereTexAddFactor);
        if (sphereTexMode == 1 || sphereTexMode == 3)
            color *= spColor;
        else if (sphereTexMode == 2)
            color += spColor;
    }
    if (opacity == 0.0)
        discard;
    if (toonTexMode != 0) {
        float toonV = saturate(0.5 - 0.5 * dot(nor, lightDirection));
        float3 toonColor = toonTex.SampleLevel(toonTexSampler, float2(0.0, toonV), 0.0).rgb;
        toonColor = ComputeTexMulFactor(toonColor, toonTexMulFactor);
        toonColor = ComputeTexAddFactor(toonColor, toonTexAddFactor);
        color *= toonColor;
    }
    float3 specularTerm = (float3)0.0;
    if (ambientSpecularPower.a > 0.0) {
        float3 halfVec = normalize(eyeDir + lightDirection);
        float3 specularColor = specular.rgb * lightColor.rgb;
        specularTerm += pow(max(0.0, dot(halfVec, nor)), ambientSpecularPower.a) * specularColor;
    }
    return float4(color + specularTerm, opacity);
}
