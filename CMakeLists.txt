cmake_minimum_required(VERSION 3.28)
project(sdl2_player)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置 FFmpeg 路径
set(FFMPEG_PATH "${CMAKE_SOURCE_DIR}/third_party/ffmpeg-n4.4")
include_directories(${FFMPEG_PATH}/include)
link_directories(${FFMPEG_PATH}/lib)

# 设置 OpenCV 路径
# find_package(OpenCV REQUIRED)
# include_directories(${OpenCV_INCLUDE_DIRS})

# 设置 SDL2 路径
set(SDL2_PATH "${CMAKE_SOURCE_DIR}/third_party/SDL2-2.26.0-allinone/x86_64-w64-mingw32")
include_directories(${SDL2_PATH}/include)
link_directories(${SDL2_PATH}/lib)

# 设置 GLAD 路径
set(GLAD_PATH "${CMAKE_SOURCE_DIR}/third_party/glad")
include_directories(${GLAD_PATH}/include)
set(GLAD_SRC "${GLAD_PATH}/src/glad.c")

# 设置 stb_image 路径
set(STB_PATH "${CMAKE_SOURCE_DIR}/third_party/stb")
include_directories(${STB_PATH})

# 设置 kissfft 路径
set(KISSFFT_PATH "${CMAKE_SOURCE_DIR}/third_party/kissfft-131.1.0")
include_directories(${KISSFFT_PATH})
file(GLOB_RECURSE KISSFFT_SRC "${KISSFFT_PATH}/*.c")

# 生成 root_directory.h 
configure_file(configuration/root_directory.h.in configuration/root_directory.h)
include_directories(${CMAKE_BINARY_DIR}/configuration)

# 项目头文件添加
include_directories(
    ${CMAKE_SOURCE_DIR}/include

    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/tool
)

# 添加源代码
set(SOURCES 
    # "${CMAKE_SOURCE_DIR}/main.cpp"
    "${CMAKE_SOURCE_DIR}/src/app.cpp"

    "${CMAKE_SOURCE_DIR}/src/ffmpeg_utils/ffmpeg_headers.hpp"

    "${CMAKE_SOURCE_DIR}/src/player_core/demux.hpp"
    "${CMAKE_SOURCE_DIR}/src/player_core/demux.cpp"
    "${CMAKE_SOURCE_DIR}/src/player_core/decode.hpp"
    "${CMAKE_SOURCE_DIR}/src/player_core/decode.cpp"
    "${CMAKE_SOURCE_DIR}/src/player_core/player_state.hpp"
    "${CMAKE_SOURCE_DIR}/src/player_core/player_state.cpp"
    "${CMAKE_SOURCE_DIR}/src/player_core/audio_resampler.hpp"
    "${CMAKE_SOURCE_DIR}/src/player_core/audio_resampler.cpp"

    "${CMAKE_SOURCE_DIR}/src/player_core/decode/audio_decode.hpp"
    "${CMAKE_SOURCE_DIR}/src/player_core/decode/audio_decode.cpp"
    "${CMAKE_SOURCE_DIR}/src/player_core/decode/video_decode.hpp"
    "${CMAKE_SOURCE_DIR}/src/player_core/decode/video_decode.cpp"

    "${CMAKE_SOURCE_DIR}/src/player_core/utils/player_constants.hpp"
    "${CMAKE_SOURCE_DIR}/src/player_core/utils/safe_queue.hpp"
    "${CMAKE_SOURCE_DIR}/src/player_core/utils/timestamp_utils.hpp"
    "${CMAKE_SOURCE_DIR}/src/player_core/utils/timestamp_utils.cpp"

    "${CMAKE_SOURCE_DIR}/src/player_thread/audio_player.hpp"
    "${CMAKE_SOURCE_DIR}/src/player_thread/audio_player.cpp"
    "${CMAKE_SOURCE_DIR}/src/player_thread/decode_thread.hpp"
    "${CMAKE_SOURCE_DIR}/src/player_thread/decode_thread.cpp"
    "${CMAKE_SOURCE_DIR}/src/player_thread/demux_thread.hpp"
    "${CMAKE_SOURCE_DIR}/src/player_thread/demux_thread.cpp"
    "${CMAKE_SOURCE_DIR}/src/player_thread/player_app.hpp"
    "${CMAKE_SOURCE_DIR}/src/player_thread/player_app.cpp"
    "${CMAKE_SOURCE_DIR}/src/player_thread/video_refresh_timer.hpp"
    "${CMAKE_SOURCE_DIR}/src/player_thread/video_refresh_timer.cpp"
    "${CMAKE_SOURCE_DIR}/src/player_thread/thread_utils.hpp"
    "${CMAKE_SOURCE_DIR}/src/player_thread/thread_utils.cpp"

    "${CMAKE_SOURCE_DIR}/src/renderer/renderer.hpp"
    "${CMAKE_SOURCE_DIR}/src/renderer/renderer.cpp"

)
add_executable(main ${SOURCES} ${GLAD_SRC} ${KISSFFT_SRC})

# 链接 FFmpeg 静态库
target_link_libraries(main PRIVATE
    avdevice
    avfilter
    avformat
    avcodec
    swresample
    swscale
    avutil
)

# 连接 SDL2 库
target_link_libraries(main PRIVATE 
    SDL2main
    SDL2
    SDL2_image
)

# 添加 WIN32 特定的库
if(WIN32)
target_link_libraries(main PRIVATE
        opengl32
        imm32
        version
        setupapi
        winmm
        cfgmgr32
        ws2_32
        secur32
        bcrypt
    )
# 添加静态链接选项
target_link_options(main PRIVATE -static-libgcc -static-libstdc++)
endif()

# 添加 FFmpeg 动态库
FILE(COPY
        ${CMAKE_SOURCE_DIR}/third_party/ffmpeg-n4.4/bin/avcodec-58.dll
        ${CMAKE_SOURCE_DIR}/third_party/ffmpeg-n4.4/bin/avdevice-58.dll
        ${CMAKE_SOURCE_DIR}/third_party/ffmpeg-n4.4/bin/avfilter-7.dll
        ${CMAKE_SOURCE_DIR}/third_party/ffmpeg-n4.4/bin/avformat-58.dll
        ${CMAKE_SOURCE_DIR}/third_party/ffmpeg-n4.4/bin/avutil-56.dll
        ${CMAKE_SOURCE_DIR}/third_party/ffmpeg-n4.4/bin/swresample-3.dll
        ${CMAKE_SOURCE_DIR}/third_party/ffmpeg-n4.4/bin/swscale-5.dll
        DESTINATION
        ${CMAKE_CURRENT_BINARY_DIR}/
)

# 添加 SDL2 动态库
FILE(COPY
        ${CMAKE_SOURCE_DIR}/third_party/SDL2-2.26.0-allinone/x86_64-w64-mingw32/bin/SDL2.dll
        DESTINATION
        ${CMAKE_CURRENT_BINARY_DIR}/
)

# 复制数据文件到输出目录
FILE(COPY 
    ${PROJECT_SOURCE_DIR}/data
    ${PROJECT_SOURCE_DIR}/assets
    ${PROJECT_SOURCE_DIR}/shaders
    DESTINATION 
    ${CMAKE_CURRENT_BINARY_DIR}/
)