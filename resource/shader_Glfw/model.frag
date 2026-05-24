#version 140

in vec3 vs_Pos;
in vec3 vs_Nor;
in vec2 vs_UV;

out vec4 out_Color;

uniform float alpha;
uniform vec3 diffuse;
uniform vec3 ambient;
uniform vec3 specular;
uniform float specularPower;
uniform vec3 lightColor;
uniform vec3 lightDir;

uniform int texMode;
uniform sampler2D tex;
uniform vec4 texMulFactor;
uniform vec4 texAddFactor;

uniform int toonTexMode;
uniform sampler2D toonTex;
uniform vec4 toonTexMulFactor;
uniform vec4 toonTexAddFactor;

uniform int sphereTexMode;
uniform sampler2D sphereTex;
uniform vec4 sphereTexMulFactor;
uniform vec4 sphereTexAddFactor;

vec3 ComputeTexMulFactor(vec3 texColor, vec4 factor) {
    vec3 ret = texColor * factor.rgb;
    return mix(vec3(1.0, 1.0, 1.0), ret, factor.a);
}

vec3 ComputeTexAddFactor(vec3 texColor, vec4 factor) {
    vec3 ret = texColor + (texColor - vec3(1.0)) * factor.a;
    ret = clamp(ret, vec3(0.0), vec3(1.0)) + factor.rgb;
    return ret;
}

void main() {
    vec3 eyeDir = normalize(vs_Pos);
    vec3 lightDirection = normalize(-lightDir);
    vec3 nor = normalize(vs_Nor);
    float ln = dot(nor, lightDirection);
    ln = clamp(ln + 0.5, 0.0, 1.0);
    vec3 color = vec3(0.0, 0.0, 0.0);
    float opacity = alpha;
    vec3 diffuseColor = diffuse * lightColor;
    color = diffuseColor;
    color += ambient;
    color = clamp(color, 0.0, 1.0);
    if (texMode != 0) {
        vec4 texColor = texture(tex, vs_UV);
        texColor.rgb = ComputeTexMulFactor(texColor.rgb, texMulFactor);
        texColor.rgb = ComputeTexAddFactor(texColor.rgb, texAddFactor);
        color *= texColor.rgb;
        if (texMode == 2)
            opacity *= texColor.a;
    }
    if (opacity == 0.0)
        discard;
    if (sphereTexMode != 0) {
        vec2 spUV = vec2(0.0);
        spUV.x = nor.x * 0.5 + 0.5;
        spUV.y = nor.y * 0.5 + 0.5;
        vec3 spColor = texture(sphereTex, spUV).rgb;
        spColor = ComputeTexMulFactor(spColor, sphereTexMulFactor);
        spColor = ComputeTexAddFactor(spColor, sphereTexAddFactor);
        if (sphereTexMode == 1)
            color *= spColor;
        else if (sphereTexMode == 2)
            color += spColor;
    }
    if (toonTexMode != 0) {
        vec3 toonColor = texture(toonTex, vec2(0.0, 1.0 - ln)).rgb;
        toonColor = ComputeTexMulFactor(toonColor, toonTexMulFactor);
        toonColor = ComputeTexAddFactor(toonColor, toonTexAddFactor);
        color *= toonColor;
    }
    vec3 specularTerm = vec3(0.0);
    if (specularPower > 0) {
        vec3 halfVec = normalize(eyeDir + lightDirection);
        vec3 specularColor = specular * lightColor;
        specularTerm += pow(max(0.0, dot(halfVec, nor)), specularPower) * specularColor;
    }
    color += specularTerm;
    out_Color = vec4(color, opacity);
}
