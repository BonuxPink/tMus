#pragma once

extern "C"
{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

#include <memory>

struct ContextData
{
    std::shared_ptr<AVFormatContext> format_ctx;
    std::shared_ptr<AVCodecContext> codec_ctx;
};
