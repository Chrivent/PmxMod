void main() {
    gl_Position = vertexConstants.wvp * vec4(inPosition, 1.0);
    vsPos = (vertexConstants.wv * vec4(inPosition, 1.0)).xyz;
    vsNor = mat3(vertexConstants.wv) * inNormal;
    vsUv = vec2(inUv.x, 1.0 - inUv.y);
}
