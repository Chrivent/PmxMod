layout(location = 0) out vec4 outColor;

PMX_GROUND_SHADOW_PIXEL_CONSTANTS

void main() {
    outColor = shadowConstants.shadowColor;
}
