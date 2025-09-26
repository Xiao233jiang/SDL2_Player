#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D y_texture;
uniform sampler2D u_texture;
uniform sampler2D v_texture;

void main() {
    // 获取YUV分量
    float y = texture(y_texture, TexCoord).r;
    float u = texture(u_texture, TexCoord).r - 0.5;
    float v = texture(v_texture, TexCoord).r - 0.5;
    
    // 使用BT.601标准YUV到RGB转换矩阵
    float r = y + 1.402 * v;
    float g = y - 0.344136 * u - 0.714136 * v;
    float b = y + 1.772 * u;
    
    // 确保颜色值在[0,1]范围内
    r = clamp(r, 0.0, 1.0);
    g = clamp(g, 0.0, 1.0);
    b = clamp(b, 0.0, 1.0);
    
    FragColor = vec4(r, g, b, 1.0);
}