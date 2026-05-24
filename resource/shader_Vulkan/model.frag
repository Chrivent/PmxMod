#version 450

layout(location = 0) in vec3 vsPos;
layout(location = 1) in vec3 vsNor;
layout(location = 2) in vec2 vsUv;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform ModelPixelConstants {
    float alpha;
    vec3 diffuse;
    vec3 ambient;
    float specularPower;
    vec3 specular;
    vec3 lightColor;
    vec3 lightDir;
    vec4 texMulFactor;
    vec4 texAddFactor;
    vec4 toonTexMulFactor;
    vec4 toonTexAddFactor;
    vec4 sphereTexMulFactor;
    vec4 sphereTexAddFactor;
    ivec4 textureModes;
} pixelConstants;

layout(set = 2, binding = 0) uniform sampler2D tex;
layout(set = 2, binding = 1) uniform sampler2D toonTex;
layout(set = 2, binding = 2) uniform sampler2D sphereTex;

vec3 ComputeTexMulFactor(vec3 texColor, vec4 factor) {
    vec3 ret = texColor * factor.rgb;
    return mix(vec3(1.0), ret, factor.a);
}

vec3 ComputeTexAddFactor(vec3 texColor, vec4 factor) {
    vec3 ret = texColor + (texColor - vec3(1.0)) * factor.a;
    ret = clamp(ret, vec3(0.0), vec3(1.0)) + factor.rgb;
    return ret;
}

void main() {
    int texMode = pixelConstants.textureModes.x;
    int toonTexMode = pixelConstants.textureModes.y;
    int sphereTexMode = pixelConstants.textureModes.z;
    vec3 eyeDir = normalize(vsPos);
    vec3 lightDirection = normalize(-pixelConstants.lightDir);
    vec3 nor = normalize(vsNor);
    float ln = dot(nor, lightDirection);
    ln = clamp(ln + 0.5, 0.0, 1.0);
    vec3 color = pixelConstants.diffuse * pixelConstants.lightColor;
    color += pixelConstants.ambient;
    color = clamp(color, 0.0, 1.0);
    float opacity = pixelConstants.alpha;
    if (texMode != 0) {
        vec4 texColor = texture(tex, vsUv);
        texColor.rgb = ComputeTexMulFactor(texColor.rgb, pixelConstants.texMulFactor);
        texColor.rgb = ComputeTexAddFactor(texColor.rgb, pixelConstants.texAddFactor);
        color *= texColor.rgb;
        if (texMode == 2)
            opacity *= texColor.a;
    }
    if (opacity == 0.0)
        discard;
    if (sphereTexMode != 0) {
        vec2 spUv = vec2(0.0);
        spUv.x = nor.x * 0.5 + 0.5;
        spUv.y = 1.0 - (nor.y * 0.5 + 0.5);
        vec3 spColor = texture(sphereTex, spUv).rgb;
        spColor = ComputeTexMulFactor(spColor, pixelConstants.sphereTexMulFactor);
        spColor = ComputeTexAddFactor(spColor, pixelConstants.sphereTexAddFactor);
        if (sphereTexMode == 1)
            color *= spColor;
        else if (sphereTexMode == 2)
            color += spColor;
    }
    if (toonTexMode != 0) {
        vec3 toonColor = texture(toonTex, vec2(0.0, ln)).rgb;
        toonColor = ComputeTexMulFactor(toonColor, pixelConstants.toonTexMulFactor);
        toonColor = ComputeTexAddFactor(toonColor, pixelConstants.toonTexAddFactor);
        color *= toonColor;
    }
    if (pixelConstants.specularPower > 0.0) {
        vec3 halfVec = normalize(eyeDir + lightDirection);
        vec3 specularColor = pixelConstants.specular * pixelConstants.lightColor;
        color += pow(max(0.0, dot(halfVec, nor)), pixelConstants.specularPower) * specularColor;
    }
    outColor = vec4(color, opacity);
}
