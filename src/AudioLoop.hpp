/*
 * Copyright (C) 2023 Dāniels Ponamarjovs <bonux@duck.com>
 *
 * This file is part of tMus.
 *
 * tMus is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * tMus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tMus. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "Frame.hpp"
#include "AudioParams.hpp"

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <vector>
#include <stop_token>

extern "C"
{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswresample/swresample.h>
}

class AudioLoop
{
public:
    void loop(std::stop_token& t);
    void stop() noexcept;
    void FillAudioBuffer(std::uint8_t* stream, int len);
    void togglePause();
    int decodeFrame();
    int synchronize_audio(int nb_samples);
    [[nodiscard]] std::shared_ptr<AVCodecContext> streamOpen();
    void streamClose() noexcept;

    AudioLoop(const auto) = delete;
    explicit AudioLoop(const std::filesystem::path&, std::shared_ptr<PacketQueue> audioq, std::shared_ptr<FrameQueue> fq);
    ~AudioLoop();

    [[nodiscard]] std::condition_variable* getCv() { return &cv; }
    [[nodiscard]] std::shared_ptr<AVFormatContext> getFormatCtx() const { return m_format_ctx; }
    [[nodiscard]] AVStream* getStream() const { return m_format_ctx->streams[m_streamIndex]; }
private:
    std::condition_variable cv{};

    std::shared_ptr<AVFormatContext> m_format_ctx;
    std::shared_ptr<PacketQueue> m_packetQ;
    std::shared_ptr<FrameQueue> m_frameQ;

    AVDictionary* opt{};
    std::uint8_t* m_audioBuf{};
    std::uint8_t* m_audioBuf1{};
    SwrContext* swr_ctx{};

    std::atomic_bool paused{};

public:
    AudioParams audioSrc{};
    AudioParams audioTgt{};
private:

    std::int64_t seekPos{};
    std::int64_t seekRel{};

    double m_audioClock{};

    int m_audioBufSize{};
    unsigned m_audioBuf1Size{};
    int m_seekFlags{};
    int m_streamIndex{};
    std::atomic_int m_audioVolume{ 5 };
    int m_audioBufIndex{};
    int m_audiohwBufSize{};

    bool seekRequest{};
    bool m_muted{};

    void openFile(const std::filesystem::path&);
    bool findStream();
    void handleSeekRequest();
};
