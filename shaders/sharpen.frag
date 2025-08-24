#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D tex;
uniform vec2 texSize;
uniform float strength;
uniform int mode;

void main() {
    vec2 texelSize = 1.0 / texSize;
    
    if (mode == 1) {
        // 🔧 边缘检测模式 - 使用Sobel算子
        vec3 sobelX = vec3(0.0);
        vec3 sobelY = vec3(0.0);
        
        // Sobel X 核心（检测垂直边缘）
        float kernelX[9] = float[](
            -1.0,  0.0,  1.0,
            -2.0,  0.0,  2.0,
            -1.0,  0.0,  1.0
        );
        
        // Sobel Y 核心（检测水平边缘）
        float kernelY[9] = float[](
            -1.0, -2.0, -1.0,
             0.0,  0.0,  0.0,
             1.0,  2.0,  1.0
        );
        
        int index = 0;
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                vec2 offset = vec2(float(x), float(y)) * texelSize;
                vec3 color = texture(tex, TexCoord + offset).rgb;
                
                sobelX += color * kernelX[index];
                sobelY += color * kernelY[index];
                index++;
            }
        }
        
        // 计算梯度幅度
        float magnitude = length(vec2(length(sobelX), length(sobelY)));
        
        // 🔧 应用强度控制边缘检测的敏感度
        magnitude = pow(magnitude, 1.0 / (strength + 0.1));
        
        // 输出边缘检测结果（白色边缘，黑色背景）
        FragColor = vec4(vec3(magnitude), 1.0);
        return;
    }
    
    if (mode == 2) {
        // 🔧 新增：拉普拉斯边缘检测（更细腻的边缘）
        float laplacianKernel[9] = float[](
             0.0, -1.0,  0.0,
            -1.0,  4.0, -1.0,
             0.0, -1.0,  0.0
        );
        
        vec3 result = vec3(0.0);
        int index = 0;
        
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                vec2 offset = vec2(float(x), float(y)) * texelSize;
                vec3 color = texture(tex, TexCoord + offset).rgb;
                result += color * laplacianKernel[index++];
            }
        }
        
        float magnitude = length(result);
        magnitude = pow(magnitude, 1.0 / (strength + 0.1));
        
        FragColor = vec4(vec3(magnitude), 1.0);
        return;
    }
    
    if (mode == 3) {
        // 🔧 新增：彩色边缘检测（保留颜色信息的边缘）
        vec3 sobelX = vec3(0.0);
        vec3 sobelY = vec3(0.0);
        
        float kernelX[9] = float[](-1, 0, 1, -2, 0, 2, -1, 0, 1);
        float kernelY[9] = float[](-1, -2, -1, 0, 0, 0, 1, 2, 1);
        
        int index = 0;
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                vec2 offset = vec2(float(x), float(y)) * texelSize;
                vec3 color = texture(tex, TexCoord + offset).rgb;
                
                sobelX += color * kernelX[index];
                sobelY += color * kernelY[index];
                index++;
            }
        }
        
        // 保留颜色信息的边缘
        vec3 edge = sqrt(sobelX * sobelX + sobelY * sobelY);
        edge = pow(edge, vec3(1.0 / (strength + 0.1)));
        
        FragColor = vec4(edge, 1.0);
        return;
    }
    
    // 🔧 默认模式：锐化处理
    vec4 center = texture(tex, TexCoord);
    
    // 使用更激进的锐化核
    float sharpenKernel[9] = float[](
        -1.0, -1.0, -1.0,
        -1.0,  9.0, -1.0,
        -1.0, -1.0, -1.0
    );
    
    vec4 sharpened = vec4(0.0);
    int index = 0;
    
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec4 sample = texture(tex, TexCoord + offset);
            sharpened += sample * sharpenKernel[index++];
        }
    }
    
    // 🔧 强度控制：在原图和锐化结果之间混合
    vec4 result = mix(center, sharpened, strength * 0.3);
    
    FragColor = clamp(result, 0.0, 1.0);
}