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

#include "StatusView.hpp"
#include "Wrapper.hpp"
#include "ContextData.hpp"
#include "Pipewire.hpp"
#include "util.hpp"

#include <filesystem>
#include <stop_token>

extern "C"
{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswresample/swresample.h>
}

struct AudioSettings
{
    int freq{};
    AVChannelLayout ch_layout{};
    enum AVSampleFormat fmt{};
};

inline const auto handle_error = [](int ret)
{
    if (ret != AVERROR(EAGAIN))
    {
        constexpr int bufSize{ 4096 };
        std::array<char, bufSize> errBuff { 0 };
        int r = av_strerror(ret, errBuff.data(), bufSize);

        if (r < 0)
            throw std::runtime_error("av_strerror failed or did not find a description for error\n");
        else
            throw std::runtime_error(fmt::format("Error {}\n", errBuff.data()));
    }
};

// This function comaprers wheather ffmpeg's format is the same as pipewires
inline bool DoesPipewireSupportFormat(AVSampleFormat fmt) noexcept
{
    switch (fmt)
    {
    case AV_SAMPLE_FMT_U8:  [[fallthrough]];
    case AV_SAMPLE_FMT_S16: [[fallthrough]];
    case AV_SAMPLE_FMT_FLT:
        return true;

    case AV_SAMPLE_FMT_S32: [[fallthrough]];
    case AV_SAMPLE_FMT_NONE: [[fallthrough]];
    default:
        return false;
    }
}

class Swr
{
public:
    Swr(const Swr&) = default;
    Swr(Swr&&) = delete;
    Swr& operator=(const Swr&) = default;
    Swr& operator=(Swr&&) = delete;

    Swr(AVCodecContext &cc, const AudioSettings &audio_settings)
    {
        if (not DoesPipewireSupportFormat(cc.sample_fmt) or
            cc.ch_layout.nb_channels != audio_settings.ch_layout.nb_channels or
            cc.sample_rate != audio_settings.freq)
        {
            int ret = swr_alloc_set_opts2(&m_swr_ctx, &audio_settings.ch_layout,
                                        AV_SAMPLE_FMT_S16, audio_settings.freq,
                                        &cc.ch_layout, cc.sample_fmt, cc.sample_rate,
                                        0, nullptr);

            util::Log(fg(fmt::color::crimson), "Target fmt: {}, freq: {}\n",
                    static_cast<int>(audio_settings.fmt), audio_settings.freq);
            util::Log(fg(fmt::color::deep_pink), "Codec fmt: {}, freq: {}\n",
                    static_cast<int>(cc.sample_fmt), cc.sample_rate);

            if (ret != 0)
                throw std::runtime_error(fmt::format(fg(fmt::color::red),
                                                    "Error swr_alloc_set_opts2 retruned: {}"));

            ret = swr_init(m_swr_ctx);
            if (ret < 0)
            {
                swr_free(&m_swr_ctx);
                util::Log("swr_init error\n");
                handle_error(ret);
            }
        }
        else
        {
            util::Log("Not initializing SWR\n");
        }
    }

    int convert(std::uint8_t** out, int out_count, std::uint8_t** in, int in_count)
    {
        if (int ret = swr_convert(m_swr_ctx, out, out_count, const_cast<const std::uint8_t**>(in), in_count); ret >= 0)
        {
            return ret;
        }

        return 0;
    }

    ~Swr()
    {
        swr_free(&m_swr_ctx);
    }

    operator bool() const noexcept
    {
        return m_swr_ctx;
    }

    operator SwrContext*() const noexcept { return m_swr_ctx; }

private:
    SwrContext* m_swr_ctx{};
};

class AudioFileManager
{
public:
    explicit AudioFileManager(const std::filesystem::path& filename, ContextData&);

    [[nodiscard]] AudioSettings getAudioSettings() const noexcept
    { return m_audioSettings; }

    [[nodiscard]] int getStreamIndex() const noexcept
    { return m_streamIndex; }

    // [[nodiscard]] SDL_AudioDeviceID getAudioDeviceID() const noexcept
    // { return m_audio_deviceID; }

    [[nodiscard]] AVSampleFormat getFormat() const noexcept
    {
        return m_audioSettings.fmt;
    }

    [[nodiscard]] int getFreq() const noexcept
    {
        return m_audioSettings.freq;
    }

    [[nodiscard]] int getChannels() const noexcept
    {
        return m_audioSettings.ch_layout.nb_channels;
    }

private:
    void open_and_setup(const std::filesystem::path& filename);
    void stream_open();
    void find_stream();
    void format_fill();

    AudioSettings m_audioSettings{};
    ContextData* m_ctx_data{};
    int m_streamIndex{};
};

class AudioLoop
{
public:
    explicit AudioLoop(const std::filesystem::path& path);
    ~AudioLoop();

    AudioLoop(const AudioLoop&)            = delete;
    AudioLoop(AudioLoop&&)                 = delete;
    AudioLoop& operator=(const AudioLoop&) = delete;
    AudioLoop& operator=(AudioLoop&&)      = delete;

    void consumer_loop(std::stop_token& st);
    [[nodiscard]] ContextData& getContextData() noexcept
    { return m_ctx_data; }

private:
    void producer_loop(std::stop_token st);
    int FillAudioBuffer();
    int Fill();

    void HandleEvent();
    void handleSeekRequest(std::int64_t offset);

    std::mutex m_buffer_mtx{};
    std::mutex m_format_mtx{};

    Wrap::align_buf_t m_produced_buf{};

    std::jthread th_producer_loop{};

    ContextData m_ctx_data{};
    AudioFileManager manager;
    Pipewire m_pipewire;
    StatusView m_statusView;
    Swr swr;
    std::size_t m_position_in_bytes = 0uz;
    std::vector<std::uint8_t> m_buffer{};

    bool m_paused{};
};

inline std::jthread playbackThread;
