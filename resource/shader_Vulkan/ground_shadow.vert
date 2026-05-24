#version 450

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform GroundShadowVertexConstants {
    mat4 wvp;
} shadowConstants;

void main() {
    gl_Position = shadowConstants.wvp * vec4(inPosition, 1.0);
}
