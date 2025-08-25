#include "player_app.hpp"
#include <iostream>

int main(int argc, char* argv[]) 
{
    try 
    {
        PlayerApp app("data/video.mp4");
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