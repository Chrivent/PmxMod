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
    vec3 lightDirection = normalize(-pixelConstants.lightDir.xyz);
    vec3 nor = normalize(vsNor);
    float ln = dot(nor, lightDirection);
    ln = clamp(ln + 0.5, 0.0, 1.0);
    vec3 color = pixelConstants.diffuseAlpha.rgb * pixelConstants.lightColor.rgb;
    color += pixelConstants.ambientSpecularPower.rgb;
    color = clamp(color, 0.0, 1.0);
    float opacity = pixelConstants.diffuseAlpha.a;
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
        spUv.y = nor.y * 0.5 + 0.5;
        vec3 spColor = texture(sphereTex, spUv).rgb;
        spColor = ComputeTexMulFactor(spColor, pixelConstants.sphereTexMulFactor);
        spColor = ComputeTexAddFactor(spColor, pixelConstants.sphereTexAddFactor);
        if (sphereTexMode == 1)
            color *= spColor;
        else if (sphereTexMode == 2)
            color += spColor;
    }
    if (toonTexMode != 0) {
        vec3 toonColor = texture(toonTex, vec2(0.0, 1.0 - ln)).rgb;
        toonColor = ComputeTexMulFactor(toonColor, pixelConstants.toonTexMulFactor);
        toonColor = ComputeTexAddFactor(toonColor, pixelConstants.toonTexAddFactor);
        color *= toonColor;
    }
    if (pixelConstants.ambientSpecularPower.a > 0.0) {
        vec3 halfVec = normalize(eyeDir + lightDirection);
        vec3 specularColor = pixelConstants.specular.rgb * pixelConstants.lightColor.rgb;
        color += pow(max(0.0, dot(halfVec, nor)), pixelConstants.ambientSpecularPower.a) * specularColor;
    }
    outColor = vec4(color, opacity);
}
