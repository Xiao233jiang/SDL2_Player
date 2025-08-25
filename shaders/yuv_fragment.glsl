#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D y_texture;
uniform sampler2D u_texture;
uniform sampler2D v_texture;

// YUV到RGB的转换矩阵
const mat3 yuv2rgb = mat3(
    1.164383,  1.164383, 1.164383,
    0.0,       -0.391762, 2.017232,
    1.596027, -0.812968, 0.0
);

void main()
{
    // 从纹理中采样Y、U、V分量
    float y = texture(y_texture, TexCoord).r;
    float u = texture(u_texture, TexCoord).r;
    float v = texture(v_texture, TexCoord).r;
    
    // 调整YUV值范围
    y = 1.1643 * (y - 0.0625);
    u = u - 0.5;
    v = v - 0.5;
    
    // 转换为RGB
    vec3 rgb = yuv2rgb * vec3(y, u, v);
    
    FragColor = vec4(rgb, 1.0);
}