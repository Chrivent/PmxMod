layout(location = 0) in vec3 inPosition;

PMX_LAYOUT_UBO(0, 0) uniform GroundShadowVertexConstants {
    mat4 wvp;
} shadowConstants;

void main() {
    gl_Position = shadowConstants.wvp * vec4(inPosition, 1.0);
}
