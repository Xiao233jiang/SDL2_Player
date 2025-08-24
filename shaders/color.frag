#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D tex;
uniform vec2 texSize;
uniform float brightness;
uniform float contrast;
uniform float saturation;
uniform int mode;

void main() {
    vec4 color = texture(tex, TexCoord);
    
    if (mode == 1) {
        // 边缘检测模式（兼容）
        vec2 texelSize = 1.0 / texSize;
        vec3 sobelX = vec3(0.0);
        vec3 sobelY = vec3(0.0);
        
        float kernelX[9] = float[](-1, 0, 1, -2, 0, 2, -1, 0, 1);
        float kernelY[9] = float[](-1, -2, -1, 0, 0, 0, 1, 2, 1);
        
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                vec2 offset = vec2(float(x), float(y)) * texelSize;
                vec3 rgb = texture(tex, TexCoord + offset).rgb;
                int index = (x + 1) * 3 + (y + 1);
                
                sobelX += rgb * kernelX[index];
                sobelY += rgb * kernelY[index];
            }
        }
        
        float magnitude = length(vec2(length(sobelX), length(sobelY)));
        FragColor = vec4(vec3(magnitude), 1.0);
        return;
    }
    
    // 色彩调整处理
    
    // 1. 亮度调整
    color.rgb *= brightness;
    
    // 2. 对比度调整（更平滑的算法）
    color.rgb = (color.rgb - 0.5) * contrast + 0.5;
    
    // 3. 饱和度调整
    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114)); // 使用标准亮度系数
    color.rgb = mix(vec3(gray), color.rgb, saturation);
    
    // 4. 色调调整（可选，这里实现轻微暖色调）
    // color.r *= 1.02;
    // color.b *= 0.98;
    
    // 5. Gamma 校正（可选）
    // color.rgb = pow(color.rgb, vec3(1.0/2.2));
    
    FragColor = clamp(color, 0.0, 1.0);
}