layout(location = 0) out vec4 outColor;

PMX_EDGE_PIXEL_CONSTANTS

void main() {
    outColor = edgeConstants.edgeColor;
}
