#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D tex;
uniform vec2 texSize;
uniform float beautyLevel;
uniform int mode;

void main() {
    vec4 originalColor = texture(tex, TexCoord);
    
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
                vec3 color = texture(tex, TexCoord + offset).rgb;
                int index = (x + 1) * 3 + (y + 1);
                
                sobelX += color * kernelX[index];
                sobelY += color * kernelY[index];
            }
        }
        
        float magnitude = length(vec2(length(sobelX), length(sobelY)));
        FragColor = vec4(vec3(magnitude), 1.0);
        return;
    }
    
    // 美颜处理
    vec2 texelSize = 1.0 / texSize;
    vec4 blurred = vec4(0.0);
    
    // 高斯模糊核（用于磨皮）
    float kernel[9] = float[](
        1.0/16.0, 2.0/16.0, 1.0/16.0,
        2.0/16.0, 4.0/16.0, 2.0/16.0,
        1.0/16.0, 2.0/16.0, 1.0/16.0
    );
    
    int index = 0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            blurred += texture(tex, TexCoord + offset) * kernel[index++];
        }
    }
    
    // 保留细节的选择性模糊
    vec4 highPass = originalColor - blurred;
    float edgeStrength = length(highPass.rgb);
    float blurFactor = smoothstep(0.0, 0.3, edgeStrength);
    
    // 混合原图和模糊图实现磨皮
    vec4 beautyColor = mix(blurred, originalColor, blurFactor);
    beautyColor = mix(originalColor, beautyColor, beautyLevel * 0.6);
    
    // 轻微提亮
    beautyColor.rgb *= (1.0 + beautyLevel * 0.15);
    
    // 增加饱和度
    float gray = dot(beautyColor.rgb, vec3(0.299, 0.587, 0.114));
    beautyColor.rgb = mix(vec3(gray), beautyColor.rgb, 1.0 + beautyLevel * 0.25);
    
    // 轻微暖色调
    beautyColor.r *= (1.0 + beautyLevel * 0.05);
    beautyColor.g *= (1.0 + beautyLevel * 0.02);
    
    FragColor = clamp(beautyColor, 0.0, 1.0);
}