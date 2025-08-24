#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D tex;
uniform vec2 texSize;
uniform int mode; // 0=原图, 1=边缘检测, 2=叠加模式

void main()
{
    // 修复Y轴翻转
    vec2 flippedTexCoord = vec2(TexCoord.x, 1.0 - TexCoord.y);
    
    if (mode == 0) {
        // 原图模式
        FragColor = texture(tex, flippedTexCoord);
        return;
    }
    
    // 边缘检测计算
    vec2 texel = 1.0 / texSize;
    
    // Sobel卷积核
    mat3 sobelX = mat3(
        -1.0, 0.0, 1.0,
        -2.0, 0.0, 2.0,
        -1.0, 0.0, 1.0
    );
    
    mat3 sobelY = mat3(
        -1.0, -2.0, -1.0,
         0.0,  0.0,  0.0,
         1.0,  2.0,  1.0
    );
    
    // 采样周围像素
    vec3 gradX = vec3(0.0);
    vec3 gradY = vec3(0.0);
    
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 offset = vec2(float(x), float(y)) * texel;
            vec3 sample = texture(tex, flippedTexCoord + offset).rgb;
            
            gradX += sample * sobelX[y + 1][x + 1];
            gradY += sample * sobelY[y + 1][x + 1];
        }
    }
    
    // 计算梯度幅值
    vec3 gradient = sqrt(gradX * gradX + gradY * gradY);
    float edge = dot(gradient, vec3(0.299, 0.587, 0.114));
    edge = smoothstep(0.1, 0.4, edge);
    
    if (mode == 1) {
        // 纯边缘检测模式
        FragColor = vec4(edge, edge, edge, 1.0);
    } else if (mode == 2) {
        // 叠加模式：原图+红色边缘
        vec3 original = texture(tex, flippedTexCoord).rgb;
        vec3 edgeColor = vec3(1.0, 0.2, 0.2);
        vec3 result = mix(original, edgeColor, edge * 0.6);
        FragColor = vec4(result, 1.0);
    }
}