#include "../../include/model.hlsli"

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
    float3 lightDirection = normalize(-lightDir.xyz);
    float3 nor = normalize(vsOut.Nor);
    float ln = dot(nor, lightDirection);
    ln = clamp(ln + 0.5, 0.0, 1.0);
    float3 color = float3(0.0, 0.0, 0.0);
    float opacity = diffuseAlpha.a;
    float3 diffuseColor = diffuseAlpha.rgb * lightColor.rgb;
    color = diffuseColor;
    color += ambientSpecularPower.rgb;
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
    if (ambientSpecularPower.a > 0.0) {
        float3 halfVec = normalize(eyeDir + lightDirection);
        float3 specularColor = specular.rgb * lightColor.rgb;
        specularTerm += pow(max(0.0, dot(halfVec, nor)), ambientSpecularPower.a) * specularColor;
    }
    color += specularTerm;
    return float4(color, opacity);
}
