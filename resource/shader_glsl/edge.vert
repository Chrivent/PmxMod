layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

PMX_EDGE_VERTEX_CONSTANTS

void main() {
    vec3 nor = mat3(edgeConstants.wv) * inNormal;
    vec4 pos = edgeConstants.wvp * vec4(inPosition, 1.0);
    vec2 screenNor = normalize(vec2(nor));
    pos.xy += screenNor * vec2(1.0) / (edgeConstants.screenSize * 0.5) * edgeConstants.edgeSize * pos.w;
    gl_Position = pos;
}
