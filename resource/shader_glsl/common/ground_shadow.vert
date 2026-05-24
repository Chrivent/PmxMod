void main() {
    gl_Position = shadowConstants.wvp * vec4(inPosition, 1.0);
}
