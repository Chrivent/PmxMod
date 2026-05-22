cbuffer VSData : register(b0) {
    float4x4 wv;
    float4x4 wvp;
};

cbuffer PSData : register(b1) {
    float   alpha;
    float3  diffuse;
    float3  ambient;
    float3  specular;
    float   specularPower;
    float3  lightColor;
    float3  lightDir;
    float4  texMulFactor;
    float4  texAddFactor;
    float4  toonTexMulFactor;
    float4  toonTexAddFactor;
    float4  sphereTexMulFactor;
    float4  sphereTexAddFactor;
    int4    textureModes;
}

Texture2D tex : register(t0);
Texture2D toonTex : register(t1);
Texture2D sphereTex : register(t2);
sampler texSampler : register(s0);
sampler toonTexSampler : register(s1);
sampler sphereTexSampler : register(s2);

struct VSInput {
    float3 Pos : POSITION;
    float3 Nor : NORMAL;
    float2 UV : UV;
};

struct VSOutput {
    float4 Position : SV_POSITION;
    float3 Pos : VIEWPOS;
    float3 Nor : VIEWNORMAL;
    float2 UV : UV;
};

VSOutput VSMain(VSInput input) {
    VSOutput vsOut;
    vsOut.Position = mul(wvp, float4(input.Pos, 1.0));
    vsOut.Pos = mul(wv, float4(input.Pos, 1.0)).xyz;
    vsOut.Nor = mul((float3x3)wv, input.Nor);
    vsOut.UV = float2(input.UV.x, 1.0 - input.UV.y);
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
    float3 eyeDir = normalize(vsOut.Pos);
    float3 lightDirection = normalize(-lightDir);
    float3 nor = normalize(vsOut.Nor);
    float ln = dot(nor, lightDirection);
    ln = clamp(ln + 0.5, 0.0, 1.0);
    float3 color = float3(0.0, 0.0, 0.0);
    float opacity = alpha;
    float3 diffuseColor = diffuse * lightColor;
    color = diffuseColor;
    color += ambient;
    color = clamp(color, 0.0, 1.0);
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
    if (opacity == 0.0)
        discard;
    if (sphereTexMode != 0) {
        float2 spUV = (float2)0.0;
        spUV.x = nor.x * 0.5 + 0.5;
        spUV.y = nor.y * 0.5 + 0.5;
        float3 spColor = sphereTex.Sample(sphereTexSampler, spUV).rgb;
        spColor = ComputeTexMulFactor(spColor, sphereTexMulFactor);
        spColor = ComputeTexAddFactor(spColor, sphereTexAddFactor);
        if (sphereTexMode == 1)
            color *= spColor;
        else if (sphereTexMode == 2)
            color += spColor;
    }
    if (toonTexMode != 0) {
        float3 toonColor = toonTex.Sample(toonTexSampler, float2(0.0, 1.0 - ln)).rgb;
        toonColor = ComputeTexMulFactor(toonColor, toonTexMulFactor);
        toonColor = ComputeTexAddFactor(toonColor, toonTexAddFactor);
        color *= toonColor;
    }
    float3 specularTerm = (float3)0.0;
    if (specularPower > 0.0) {
        float3 halfVec = normalize(eyeDir + lightDirection);
        float3 specularColor = specular * lightColor;
        specularTerm += pow(max(0.0, dot(halfVec, nor)), specularPower) * specularColor;
    }
    color += specularTerm;
    return float4(color, opacity);
}
