layout(location = 0) out vec4 outColor;

PMX_LAYOUT_UBO(1, 0) uniform EdgePixelConstants {
    vec4 edgeColor;
} edgeConstants;

void main() {
    outColor = edgeConstants.edgeColor;
}
