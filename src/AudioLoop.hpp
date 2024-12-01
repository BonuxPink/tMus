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
#include "AudioSettings.hpp"
#include "Pipewire.hpp"
#include "util.hpp"

#include <filesystem>
#include <thread>
#include <stop_token>
#include <utility>

extern "C"
{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswresample/swresample.h>
}

inline const auto handle_error = [](int ret)
{
    if (ret != AVERROR(EAGAIN))
    {
        constexpr int bufSize{ 64 };
        std::array<char, bufSize> errBuff { 0 };
        int r = av_strerror(ret, errBuff.data(), bufSize);

        if (r < 0)
            throw std::runtime_error("av_strerror failed or did not find a description for error\n");
        else
            throw std::runtime_error(std::format("Error {}\n", errBuff.data()));
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

class Resample
{
public:
    Resample(const Resample&) = default;
    Resample(Resample&&) = delete;
    Resample& operator=(const Resample&) = default;
    Resample& operator=(Resample&&) = delete;

    Resample(AVCodecContext &cc)
        : m_audioSettings{ std::make_shared<AudioSettings>() }
    {
        // Conveniance func for printing
        auto ConvertFmtToStr = [](AVSampleFormat fmt)
        {
            switch (fmt)
            {
            case AV_SAMPLE_FMT_U8:
                return "U8";
            case AV_SAMPLE_FMT_S16:
                return "S16";
            case AV_SAMPLE_FMT_S32:
                return "S32";
            case AV_SAMPLE_FMT_FLT:
                return "FLT";
            case AV_SAMPLE_FMT_DBL:
                return "DBL";

            case AV_SAMPLE_FMT_U8P:
                return "U8P";
            case AV_SAMPLE_FMT_S16P:
                return "S16P";
            case AV_SAMPLE_FMT_S32P:
                return "S32P";
            case AV_SAMPLE_FMT_FLTP:
                return "FLTP";
            case AV_SAMPLE_FMT_DBLP:
                return "DBLP";
            case AV_SAMPLE_FMT_S64:
                return "S64";
            case AV_SAMPLE_FMT_S64P:
                return "S64P";

            default:
                std::unreachable();
            };
        };

        if (not DoesPipewireSupportFormat(cc.sample_fmt))
        {
            util::Log(color::aqua, "Codec: {}, Desired format: {}, but going to use: {}, sample: {}\n", avcodec_get_name(cc.codec_id),
                                                                                                        ConvertFmtToStr(cc.sample_fmt),
                                                                                                        ConvertFmtToStr(AV_SAMPLE_FMT_S16),
                                                                                                        cc.sample_rate);

            int ret = swr_alloc_set_opts2(&m_swr_ctx,
                                       /* out_ch_layout  out_sample_fmt     out_sample_rate */
                                          &cc.ch_layout, AV_SAMPLE_FMT_S16, 48'000,
                                       /* in_ch_layout   in_sample_fmt      in_sample_rate */
                                          &cc.ch_layout, cc.sample_fmt,     cc.sample_rate,
                                          0, nullptr);
            if (ret != 0)
            {
                handle_error(ret);
            }

            ret = swr_init(m_swr_ctx);
            if (ret < 0)
            {
                swr_free(&m_swr_ctx);
                util::Log("swr_init error\n");
                handle_error(ret);
            }

            m_audioSettings->freq      = 48'000;
            m_audioSettings->fmt       = AV_SAMPLE_FMT_S16;
            m_audioSettings->ch_layout = cc.ch_layout;
        }
        else
        {
            util::Log(color::aqua, "Codec: {}, format: {}, sample: {}, Not initializing SWR\n", avcodec_get_name(cc.codec_id),
                                                                                                ConvertFmtToStr(cc.sample_fmt),
                                                                                                cc.sample_rate);

            m_audioSettings->freq      = cc.sample_rate;
            m_audioSettings->fmt       = cc.sample_fmt;
            m_audioSettings->ch_layout = cc.ch_layout;
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

    ~Resample()
    {
        swr_free(&m_swr_ctx);
    }

    operator bool() const noexcept
    {
        return m_swr_ctx;
    }

    operator SwrContext*() const noexcept { return m_swr_ctx; }

    [[nodiscard]] std::shared_ptr<AudioSettings> getAudioSettings() const noexcept
    { return m_audioSettings; }

    [[nodiscard]] AVSampleFormat getAudioFormat() const noexcept
    { return m_audioSettings->fmt; }

private:
    std::shared_ptr<AudioSettings> m_audioSettings;
    SwrContext* m_swr_ctx{};
};

class AudioFileManager
{
public:
    explicit AudioFileManager(const std::filesystem::path& filename, ContextData&);

    [[nodiscard]] int getStreamIndex() const noexcept
    { return m_streamIndex; }

private:
    void open_and_setup(const std::filesystem::path& filename);
    void stream_open();
    void find_stream();

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
    Resample swr;
    StatusView m_statusView;
    Pipewire m_pipewire;
    std::size_t m_position_in_bytes = 0uz;
    std::vector<std::uint8_t> m_buffer{};

    bool m_paused{};
    std::atomic<bool> m_eof_reached{};
};

inline std::jthread playbackThread;
