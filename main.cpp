#include "libavutil/pixfmt.h"
#define SDL_MAIN_HANDLED

#include <cstddef>
#include <cstdint>
#include <iostream>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
    #include <libavutil/avutil.h>
    #include <inttypes.h>
}

#include <SDL2/SDL.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>

int main(int argc, char **argv)
{
    AVFormatContext *p_fmt_ctx = NULL;      // 格式上下文
    avformat_open_input(&p_fmt_ctx, argv[1], NULL, NULL);   // 打开输入文件
    avformat_find_stream_info(p_fmt_ctx, NULL);     // 查找流信息
    av_dump_format(p_fmt_ctx, 0, argv[1], 0);   // 打印格式信息

    int video_idx = -1;     // 视频流索引
    int frame_rate = -1;    // 视频帧率
    for (int i = 0; i < p_fmt_ctx->nb_streams; i++)
    {
        if (p_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)  // 找到视频流
        {
            video_idx = i;      // 找到视频流
            std::cout << "Find a video stream, index " << video_idx << std::endl;   
            frame_rate = p_fmt_ctx->streams[i]->avg_frame_rate.num /
                         p_fmt_ctx->streams[i]->avg_frame_rate.den;     // 获取视频帧率
            break;
        }
    }

    AVCodecParameters *p_codec_par =
        p_fmt_ctx->streams[video_idx]->codecpar; // 获取视频流的参数
    const AVCodec *p_codec =
        avcodec_find_decoder(p_codec_par->codec_id); // 查找解码器
    AVCodecContext *p_codec_ctx =
        avcodec_alloc_context3(p_codec); // 创建解码器上下文
    avcodec_parameters_to_context(p_codec_ctx, p_codec_par);    // 将参数复制到上下文
    avcodec_open2(p_codec_ctx, p_codec, NULL);  // 打开解码器

    AVFrame *p_frm_raw = av_frame_alloc();  // 原始视频帧
    AVFrame *p_frm_yuv = av_frame_alloc();  // YUV视频帧
    int buf_size = av_image_get_buffer_size(
        AV_PIX_FMT_YUV420P, p_codec_ctx->width, p_codec_ctx->height, 1);    // 计算缓冲区大小
    uint8_t *p_buffer = (uint8_t *)av_malloc(buf_size); // 分配缓冲区
    av_image_fill_arrays(p_frm_yuv->data, p_frm_yuv->linesize, p_buffer,
                         AV_PIX_FMT_YUV420P, p_codec_ctx->width,
                         p_codec_ctx->height, 1); // 初始化YUV视频帧

    AVPacket *p_packet = (AVPacket *)av_malloc(sizeof(AVPacket));   // 创建AVPacket, 用于解码

    // 初始化 SWS context，用于后续图像转换
    // 此处第 6 个参数使用的是 FFmpeg 中的像素格式，对比参考注释 B4
    // 如果解码后得到图像格式不被 SDL 支持，不进行格式转换的话，SDL 是无法正常显示图像的
    // 如果解码后得到图像格式能被 SDL 支持，则不必进行格式转换
    // 这里为了编写代码简便，不管解码后输出帧是什么格式，统一转换为 AV_PIX_FMT_YUV420P 格式，
    // FFmpeg 中的 AV_PIX_FMT_YUV420P 格式实际就是 SDL 中的 SDL_PIXELFORMAT_IYUV 格式
    struct SwsContext *p_sws_ctx = sws_getContext(
        p_codec_ctx->width, p_codec_ctx->height, p_codec_ctx->pix_fmt,
        p_codec_ctx->width / 2, p_codec_ctx->height / 2, AV_PIX_FMT_YUV420P,
        SWS_BICUBIC, NULL, NULL, NULL);     // 创建缩放上下文

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
    SDL_Window* p_sdl_screen = 
        SDL_CreateWindow("simple ffplayer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                         p_codec_ctx->width / 2, p_codec_ctx->height / 2, SDL_WINDOW_OPENGL);
    SDL_Renderer *p_sdl_renderer = SDL_CreateRenderer(p_sdl_screen, -1, 0);
    // 创建 SDL_Texture
    // 一个 SDL_Texture 对应一帧 YUV 数据，同 SDL 1.x 中的 SDL_Overlay
    // 此处第 2 个参数使用的是 SDL 中的像素格式，对比参考注释 A7
    // SDL 中的 SDL_PIXELFORMAT_IYUV 格式实际就是FFmpeg 中的 AV_PIX_FMT_YUV420P 格式
    SDL_Texture *p_sdl_texture = SDL_CreateTexture(
        p_sdl_renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
        p_codec_ctx->width, p_codec_ctx->height);

    SDL_Event sdl_event;
    SDL_Rect sdl_rect;
    sdl_rect.x = 0;
    sdl_rect.y = 0;
    sdl_rect.w = p_codec_ctx->width;
    sdl_rect.h = p_codec_ctx->height;

    // 从视频文件中读取一个 packet
    // packet 可能是视频帧、音频帧或其他数据，解码器只会解码视频帧或音频帧
    // 对于视频来说，一个 packet 只包含一个 frame
    // 对于音频来说，若是帧长固定的格式则一个 packet 可包含多个 frame，
    //              若是帧长可变的格式则一个 packet 只包含一个 frame
    while (av_read_frame(p_fmt_ctx, p_packet) == 0)
    {
        if (p_packet->stream_index == video_idx)
        {
            avcodec_send_packet(p_codec_ctx, p_packet);     // 发送AVPacket到解码器
            avcodec_receive_frame(p_codec_ctx, p_frm_raw); // 接收解码后的原始帧

            // 图像转换：p_frm_raw->data ==> p_frm_yuv->data
            // 将源图像中一片连续的区域经过处理后更新到目标图像对应区域，处理的图像区域必须逐行连续
            // plane: 如 YUV420P 像素格式有 Y、U、V 三个 plane，NV12 像素格式有 Y 和 UV 两个 plane
            // slice: 图像中一片连续的行，必须是连续的，顺序由顶部到底部或由底部到顶部
            // stride/pitch: 一行图像所占的字节数，Stride=BytesPerPixel*Width+Padding，注意对齐
            // AVFrame.*data[]: 每个数组元素指向对应 plane
            // AVFrame.linesize[]: 每个数组元素表示对应 plane 中一行图像所占的字节数
            sws_scale(p_sws_ctx, (const uint8_t *const *)p_frm_raw->data,
                      p_frm_raw->linesize, 0, p_codec_ctx->height,
                      p_frm_yuv->data,
                      p_frm_yuv->linesize); // 将原始帧转换为YUV格式

            // 使用新的 YUV 像素数据更新 SDL_Rect
            SDL_UpdateYUVTexture(p_sdl_texture, &sdl_rect, 
                         p_frm_yuv->data[0], p_frm_yuv->linesize[0], 
                         p_frm_yuv->data[1], p_frm_yuv->linesize[1],
                         p_frm_yuv->data[2], p_frm_yuv->linesize[2]); // 更新SDL纹理
            SDL_RenderClear(p_sdl_renderer);    // 清除渲染器
            // 使用图像数据(texture)更新当前渲染目标
            SDL_RenderCopy(p_sdl_renderer, p_sdl_texture, NULL, &sdl_rect);     // 绘制视频帧
            SDL_RenderPresent(p_sdl_renderer);      // 显示视频帧

            SDL_Delay(1000 / frame_rate); // 控制播放速度
        }
    }

    SDL_Quit();
    sws_freeContext(p_sws_ctx);
    av_packet_unref(p_packet);
    av_free(p_buffer);
    av_frame_free(&p_frm_yuv);
    av_frame_free(&p_frm_raw);
    avcodec_free_context(&p_codec_ctx);
    avformat_close_input(&p_fmt_ctx);
    
    return 0;
}
