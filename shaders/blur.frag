#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D tex;
uniform vec2 texSize;
uniform float blurRadius;
uniform int mode;

void main() {
    if (mode == 1) {
        // 边缘检测模式（兼容原有功能）
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
    
    // 高斯模糊效果
    vec2 texelSize = 1.0 / texSize;
    vec4 result = vec4(0.0);
    float totalWeight = 0.0;
    
    int radius = max(1, int(blurRadius));
    float sigma = blurRadius / 3.0;
    
    for (int x = -radius; x <= radius; ++x) {
        for (int y = -radius; y <= radius; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            float distance = sqrt(float(x*x + y*y));
            float weight = exp(-(distance * distance) / (2.0 * sigma * sigma));
            
            result += texture(tex, TexCoord + offset) * weight;
            totalWeight += weight;
        }
    }
    
    FragColor = result / totalWeight;
}