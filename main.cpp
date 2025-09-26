#include "player_app.hpp"
#include <iostream>

int main(int argc, char* argv[]) 
{
    try 
    {
        std::string filename;
        
        // 如果命令行提供了文件名则使用，否则空启动
        if (argc > 1) {
            filename = argv[1];
        }
        
        PlayerApp app(filename); // 支持空字符串启动
        if (!app.init()) 
        {
            std::cerr << "Failed to initialize player" << std::endl;
            return 1;
        }
        app.run();
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}