#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform vec4 display_rect; // x: left, y: top, z: width, w: height (normalized)

void main()
{
    // 将 [-1,1] 坐标转换为 [0,1] 坐标
    vec2 normalizedPos = (aPos + 1.0) * 0.5;
    
    // 应用显示区域变换
    vec2 displayPos;
    displayPos.x = display_rect.x + normalizedPos.x * display_rect.z;
    displayPos.y = display_rect.y + normalizedPos.y * display_rect.w;
    
    // 转换回 [-1,1] 坐标系统
    vec2 clipPos = displayPos * 2.0 - 1.0;
    
    // OpenGL的Y轴是向上的，所以需要翻转Y坐标
    clipPos.y = -clipPos.y;
    
    gl_Position = vec4(clipPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}