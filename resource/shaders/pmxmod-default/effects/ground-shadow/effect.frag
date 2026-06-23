layout(location = 0) out vec4 outColor;

PMX_LAYOUT_UBO(1, 0) uniform GroundShadowPixelConstants {
    vec4 shadowColor;
} shadowConstants;

void main() {
    outColor = shadowConstants.shadowColor;
}
