layout(location = 0) in vec3 inPosition;

PMX_GROUND_SHADOW_VERTEX_CONSTANTS

void main() {
    gl_Position = shadowConstants.wvp * vec4(inPosition, 1.0);
}
