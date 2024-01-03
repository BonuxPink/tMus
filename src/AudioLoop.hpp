/*
 * Copyright (C) 2023-2024 Dāniels Ponamarjovs <bonux@duck.com>
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

#include "AudioParams.hpp"
#include "LoopComponent.hpp"
#include "Wrapper.hpp"

#include <SDL2/SDL.h>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <thread>
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

    void consumer_loop(std::stop_token& t);

    [[nodiscard]] std::shared_ptr<AVFormatContext> getFormatCtx() const { return m_format_ctx; }
    [[nodiscard]] std::shared_ptr<AVCodecContext> getCodecContext() const { return m_codec_ctx; }

    explicit AudioLoop(const std::filesystem::path&);
    ~AudioLoop();

    AudioLoop(const AudioLoop&)            = delete;
    AudioLoop(AudioLoop&&)                 = delete;
    AudioLoop& operator=(const AudioLoop&) = delete;
    AudioLoop& operator=(AudioLoop&&)      = delete;

private:
    std::shared_ptr<AVFormatContext> m_format_ctx;
    std::shared_ptr<AVCodecContext> m_codec_ctx;

    SwrContext* m_swr_ctx{};

    bool m_paused{};

    AudioParams m_audioTgt{};

    int m_streamIndex{};
    std::atomic_int m_audioVolume{ 5 };

    std::jthread th_producer_loop{};

    SDL_AudioSpec obtained_AudioSpec{};
    SDL_AudioDeviceID m_audio_deviceID{};

    std::size_t m_position_in_bytes{};

    Wrap::align_buf_t m_produced_buf{};

    int curr_pkt_size{};
    std::uint8_t* m_curr_pkt_buf{};
    int m_buffer_used_len{};

    std::mutex m_buffer_mtx{};
    std::mutex m_format_mtx{};
    std::vector<std::uint8_t> m_buffer{};

    [[nodiscard]] int FillAudioBuffer();

    void HandleEvent();
    void InitSwr();
    void findStream();
    void deviceOpen();
    void handleSeekRequest(std::int64_t);
    void openFile(const std::filesystem::path&);
    void producer_loop(std::stop_token);
    void setupAudio();
    void streamOpen();
};
