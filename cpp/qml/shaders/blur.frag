#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(binding = 0) uniform sampler2D src;
layout(std140, binding = 1) uniform buf {
    mat4 qt_Matrix;
    float texelW;
    float texelH;
    float radius;
};
void main() {
    vec2 off = vec2(texelW, texelH) * radius;
    vec4 s = texture(src, qt_TexCoord0);
    vec4 sum =
        texture(src, qt_TexCoord0 + vec2(-1.0,-1.0)*off) * 0.0778 +
        texture(src, qt_TexCoord0 + vec2( 0.0,-1.0)*off) * 0.1232 +
        texture(src, qt_TexCoord0 + vec2( 1.0,-1.0)*off) * 0.0778 +
        texture(src, qt_TexCoord0 + vec2(-1.0, 0.0)*off) * 0.1232 +
        s * 0.1958 +
        texture(src, qt_TexCoord0 + vec2( 1.0, 0.0)*off) * 0.1232 +
        texture(src, qt_TexCoord0 + vec2(-1.0, 1.0)*off) * 0.0778 +
        texture(src, qt_TexCoord0 + vec2( 0.0, 1.0)*off) * 0.1232 +
        texture(src, qt_TexCoord0 + vec2( 1.0, 1.0)*off) * 0.0778;
    fragColor = sum;
}
