layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec3 vsPos;
layout(location = 1) out vec3 vsNor;
layout(location = 2) out vec2 vsUv;

PMX_LAYOUT_UBO(0, 0) uniform ModelVertexConstants {
    mat4 wv;
    mat4 wvp;
} vertexConstants;

void main() {
    gl_Position = vertexConstants.wvp * vec4(inPosition, 1.0);
    vsPos = (vertexConstants.wv * vec4(inPosition, 1.0)).xyz;
    vsNor = mat3(vertexConstants.wv) * inNormal;
    vsUv = vec2(inUv.x, 1.0 - inUv.y);
}
