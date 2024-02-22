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

#include "AudioLoop.hpp"
#include "Factories.hpp"
#include "StatusView.hpp"
#include "globals.hpp"

AudioFileManager::AudioFileManager(const std::filesystem::path& filename, ContextData& ctx_data)
    : m_ctx_data(&ctx_data)
{
    open_and_setup(filename);
    find_stream();
    stream_open();
    device_open();
}

void AudioFileManager::open_and_setup(const std::filesystem::path& filename)
{
    AVDictionary* opt{};
    av_dict_set(&opt, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);

    AVFormatContext* ctx{};
    int err = avformat_open_input(&ctx, filename.c_str(), nullptr, &opt);
    if (err < 0)
    {
        av_dict_free(&opt);
        std::array<char, AV_ERROR_MAX_STRING_SIZE> errBuff { 0 };

        util::Log(fg(fmt::color::yellow), "avformat_open_input: error code: {}, filename: {}, error msg: {}, errno: {}\n",
                  err, filename.string(), av_make_error_string(errBuff.data(), AV_ERROR_MAX_STRING_SIZE, err), strerror(errno));

        throw std::runtime_error(fmt::format("Erorr avformat_open_input, with code: {}, filename: {}, errmsg: {}", err, filename.string(), errBuff.data()));
    }
    av_dict_free(&opt);

    m_ctx_data->format_ctx = std::shared_ptr<AVFormatContext>(ctx, [](AVFormatContext* ptr) { avformat_close_input(&ptr); });

    m_ctx_data->format_ctx->interrupt_callback.callback = +[]([[maybe_unused]] void*)
    {
        return static_cast<int>(Globals::stop_request);
    };
    m_ctx_data->format_ctx->interrupt_callback.opaque = nullptr;

    av_format_inject_global_side_data(m_ctx_data->format_ctx.get());
}

void AudioFileManager::stream_open()
{
    m_ctx_data->codec_ctx = std::shared_ptr<AVCodecContext> { avcodec_alloc_context3(nullptr), [](::AVCodecContext* p) { ::avcodec_free_context(&p); } };

    int ret = avcodec_parameters_to_context(m_ctx_data->codec_ctx.get(), m_ctx_data->format_ctx->streams[m_streamIndex]->codecpar);
    if (ret < 0)
    {
        throw std::runtime_error(fmt::format("avcodec_parameters_to_context failed with code: {}", ret));
    }

    m_ctx_data->codec_ctx->pkt_timebase = m_ctx_data->format_ctx->streams[m_streamIndex]->time_base;
    const auto* codec = avcodec_find_decoder(m_ctx_data->codec_ctx->codec_id);

    if (!codec)
    {
        throw std::runtime_error("Failed to find codec");
    }

    m_ctx_data->codec_ctx->codec_id = codec->id;

    ret = avcodec_open2(m_ctx_data->codec_ctx.get(), codec, nullptr);
    if (ret < 0)
    {
        throw std::runtime_error("Failed to open codec");
    }

    if (m_ctx_data->codec_ctx->codec_type != AVMEDIA_TYPE_AUDIO)
    {
        throw std::runtime_error("Codec is of wrong type");
    }

    util::Log("Avctx fmt: {}\n", static_cast<int>(m_ctx_data->codec_ctx->sample_fmt));

    m_ctx_data->format_ctx->streams[m_streamIndex]->discard = AVDISCARD_DEFAULT;
}

void AudioFileManager::find_stream()
{
    int err = avformat_find_stream_info(m_ctx_data->format_ctx.get(), nullptr);
    if (err < 0)
    {
        throw std::runtime_error("Failed: avformat_find_stream_info");
    }

    if (m_ctx_data->format_ctx->pb)
        m_ctx_data->format_ctx->pb->eof_reached = 0;

    m_streamIndex = av_find_best_stream(m_ctx_data->format_ctx.get(), AVMEDIA_TYPE_AUDIO, m_streamIndex - 1, m_streamIndex, nullptr, 0);

#ifdef DEBUG
    av_dump_format(m_ctx_data->format_ctx.get(), m_streamIndex, m_ctx_data->format_ctx->url, 0);
#endif

    if (AVERROR_STREAM_NOT_FOUND == m_streamIndex)
    {
        throw std::runtime_error(fmt::format("No streams found in {}\n", m_ctx_data->format_ctx->url));
    }
    else if (AVERROR_DECODER_NOT_FOUND == m_streamIndex)
    {
        throw std::runtime_error(fmt::format("Decoder not found in {}\n", m_ctx_data->format_ctx->url));
    }

    if (m_streamIndex < 0)
    {
        throw std::runtime_error("Stream index < 0\n");
    }
}

void AudioFileManager::device_open()
{
    AVChannelLayout wantedLayout{};
    SDL_AudioSpec obtained_spec{};
    int ret = av_channel_layout_copy(&wantedLayout, &m_ctx_data->codec_ctx->ch_layout);
    if (ret != 0)
    {
        throw std::runtime_error("Failed to copy layout with ret");
    }

    auto wanted = MakeWantedSpec(wantedLayout.nb_channels);

    for (std::array next_sample_rate{ 44'100, 48'000 }; const auto sample_rate : next_sample_rate)
    {
        wanted->freq = sample_rate;
        m_audio_deviceID = SDL_OpenAudioDevice(nullptr, 0, wanted.get(), &obtained_spec, 0);
    }

    if (obtained_spec.format != AUDIO_S16SYS)
    {
        throw std::runtime_error(fmt::format("SDL advised audio format {} is currently not supported", obtained_spec.format));
    }

    if (obtained_spec.channels != wanted->channels)
    {
        av_channel_layout_uninit(&wantedLayout);
        av_channel_layout_default(&wantedLayout, obtained_spec.channels);
        if (wantedLayout.order != AV_CHANNEL_ORDER_NATIVE)
        {
            throw std::runtime_error(fmt::format("SDL advised channel count {} is currently not supported", obtained_spec.channels));
        }
    }

    util::Log(fg(fmt::color::teal),
            "SDL spec we got Ch: {}, Freq: {}, Format: {}, padding: {}, silence: {}, size: {}\n",
            obtained_spec.channels, obtained_spec.freq, obtained_spec.format, obtained_spec.padding, obtained_spec.silence, obtained_spec.size);

    SDL_PauseAudioDevice(m_audio_deviceID, 0);

    m_audioSettings.fmt  = AV_SAMPLE_FMT_S16;
    m_audioSettings.freq = obtained_spec.freq;

    if (av_channel_layout_copy(&m_audioSettings.ch_layout, &wantedLayout))
    {
        throw std::runtime_error("Failed to copy layout");
    }

    m_audioSettings.frame_size = av_samples_get_buffer_size(nullptr, m_audioSettings.ch_layout.nb_channels, 1, m_audioSettings.fmt, 1);
    m_audioSettings.bytes_per_sec = av_samples_get_buffer_size(nullptr, m_audioSettings.ch_layout.nb_channels, 1, m_audioSettings.fmt, 1);

    if (m_audioSettings.bytes_per_sec <= 0 || m_audioSettings.frame_size <= 0)
    {
        throw std::runtime_error(fmt::format("bytes_per_sec: {} or frame_size: {} are dubious",
                                            m_audioSettings.bytes_per_sec, m_audioSettings.frame_size));
    }
}

AudioLoop::AudioLoop(const std::filesystem::path& path)
    : m_produced_buf{ Wrap::make_aligned_buffer() }
    , m_ctx_data{}
    , manager{ path, m_ctx_data }
    , m_statusView{ m_ctx_data }
    , swr{ *m_ctx_data.codec_ctx, manager.getAudioSettings() }
{
    th_producer_loop = std::jthread{ [this](std::stop_token st) { this->producer_loop(st); } };
    pthread_setname_np(th_producer_loop.native_handle(), "Producer");
}

AudioLoop::~AudioLoop()
{
    th_producer_loop.request_stop();

    const auto id = manager.getAudioDeviceID();
    SDL_PauseAudioDevice(id, 1);
    SDL_CloseAudioDevice(id);
}

int AudioLoop::FillAudioBuffer()
{
    auto send_packet = [this](AVCodecContext* cc, AVPacket* pkt)
    {
        std::scoped_lock lk{ m_format_mtx };

        int ret = avcodec_send_packet(cc, pkt);
        if (ret < 0)
        {
            util::Log("send_packet error {}\n", ret);
            handle_error(ret);
        }

        return ret;
    };

    auto read_packet = [this](AVFormatContext* format_ctx, Wrap::AvPacket& pkt)
    {
        std::scoped_lock lk{ m_format_mtx };

        auto ret = av_read_frame(format_ctx, pkt);
        return ret;
    };

    auto receive_frame = [](AVCodecContext* cc, AVFrame* frame)
    {
        return !avcodec_receive_frame(cc, frame);
    };

    Wrap::AvPacket pkt{};
    Wrap::AvFrame frame{};

    int len = 0;
    std::vector<std::uint8_t> buffer;
    while (true)
    {
        if (curr_pkt_size <= 0)
        {
            av_packet_unref(pkt);

            if (auto ret = read_packet(m_ctx_data.format_ctx.get(), pkt); ret < 0)
                return 0;

            if (pkt->stream_index == manager.getStreamIndex())
            {
                curr_pkt_size = pkt->size;
                buffer.assign(pkt->data, pkt->data + pkt->size);
            }
        }

        for (auto& byte : buffer)
        {
            Wrap::AvPacket avpkt{ curr_pkt_size };
            const auto cc = m_ctx_data.codec_ctx.get();

            memcpy(avpkt->data, &byte, curr_pkt_size);

            if (int ret = send_packet(cc, avpkt); ret == 0)
            {
                len = curr_pkt_size;
            }
            else
            {
                util::Log("len = 0 error\n");
                handle_error(ret);
                len = 0;
            }

            if (len < 0)
            {
                curr_pkt_size = 0;
            }
            else
            {
                curr_pkt_size -= len;
                // buffer.erase(buffer.begin(), buffer.begin() + len);

                int ret = receive_frame(cc, frame);
                if (ret)
                {
                    const auto nb_samples = frame->nb_samples;
                    auto buf_ptr = m_produced_buf.get();

                    ret = swr.convert(&buf_ptr, nb_samples, frame->extended_data, nb_samples);

                    m_buffer_used_len = ret * cc->ch_layout.nb_channels * static_cast<int>(sizeof(int16_t));
                    return m_buffer_used_len;
                }
            }
        }
    }
}

void AudioLoop::producer_loop(std::stop_token st)
{
    while (!Globals::stop_request && !st.stop_requested())
    {
        if (m_paused)
        {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(50ms);
        }

        int nr_read = 0;
        try
        {
            nr_read = FillAudioBuffer();
        }
        catch (const std::runtime_error& er)
        {
            util::Log(fg(fmt::color::yellow), "Runtime error: {}\n", er.what());
        }

        if (nr_read < 0)
        {
            if (nr_read != -1 || errno != EAGAIN)
            {
                nr_read = 0;
                /* TODO: handle EOF */
            }
            else
            {
                // Sleep and try again while data is not available
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(50ms);
            }
        }
        else
        {
            std::scoped_lock lk{ m_buffer_mtx };
            if (m_buffer_used_len > 0)
                std::ranges::copy(m_produced_buf.get(), std::next(m_produced_buf.get(), m_buffer_used_len),
                                  std::back_inserter(m_buffer));

            m_buffer_used_len -= m_buffer_used_len;

            using namespace std::chrono_literals;
            std::this_thread::sleep_for(10ms);
        }
    }
}

void AudioLoop::handleSeekRequest(std::int64_t offset)
{
    {
        std::scoped_lock lk{ m_format_mtx };
        const auto cc                          = m_ctx_data.codec_ctx.get();
        const auto format                      = manager.getAudioSettings().fmt;
        const auto bytes_per_sample            = av_get_bytes_per_sample(static_cast<AVSampleFormat>(format));
        const auto bytes_per_second            = cc->sample_rate * bytes_per_sample * cc->ch_layout.nb_channels;
        const auto current_position_in_seconds = static_cast<std::int64_t>(m_position_in_bytes / bytes_per_second);

        std::int64_t seek_target{ 0 };

        if (offset < 0 && (current_position_in_seconds - std::abs(offset)) < 0)
        {
            seek_target = 0;
        }
        else
        {
            seek_target = current_position_in_seconds + offset;
        }

        avcodec_flush_buffers(cc);

        const auto seek_min = std::numeric_limits<std::int64_t>::min();
        const auto seek_max = std::numeric_limits<std::int64_t>::max();
        int ret = avformat_seek_file(m_ctx_data.format_ctx.get(), -1, seek_min, seek_target * AV_TIME_BASE, seek_max, 0);
        if (ret < 0)
        {
            util::Log(fg(fmt::color::red), "Seek failed\n");
        }

        if (seek_target == 0)
            m_position_in_bytes = 0;
        else
            m_position_in_bytes += bytes_per_second * offset;

        m_statusView.draw(seek_target);
    }

    std::scoped_lock lk{ m_buffer_mtx };
    SDL_ClearQueuedAudio(manager.getAudioDeviceID());
    m_buffer.clear();
}

void AudioLoop::HandleEvent()
{
    if (Globals::event.m_EventHappened)
    {
        switch (Globals::event.act)
        {
        using enum Event::Action;

        case UP_VOLUME:
            if (m_audioVolume < 100)
            {
                m_audioVolume += 1;
            }
            break;
        case DOWN_VOLUME:
            if (m_audioVolume > 0)
            {
                m_audioVolume -= 1;
            }
            break;
        case SEEK_FORWARDS:
            handleSeekRequest(10l);
            break;
        case SEEK_BACKWARDS:
            handleSeekRequest(-10l);
            break;
        case PAUSE:
            m_paused = !m_paused;
            SDL_PauseAudioDevice(manager.getAudioDeviceID(), m_paused);
            break;
        }
        Globals::event.m_EventHappened = false;
    }

    m_statusView.draw(m_position_in_bytes);
}


void AudioLoop::consumer_loop(std::stop_token& t)
{
    const auto sample_rate = static_cast<int>(m_ctx_data.codec_ctx.get()->sample_rate);

    while (!t.stop_requested() && !Globals::stop_request)
    {
        HandleEvent();
        if (m_paused == false)
        {
            std::scoped_lock lk{ m_buffer_mtx };

            if (static_cast<int>(m_buffer.size()) >= sample_rate && static_cast<int>(SDL_GetQueuedAudioSize(2)) < sample_rate)
            {
                const auto min = std::min(sample_rate, static_cast<int>(m_buffer.size()));
                std::vector<std::uint8_t> dst(min);

                const auto vol = m_audioVolume.load();

                SDL_MixAudioFormat(dst.data(), m_buffer.data(), AUDIO_S16SYS, min, vol);
                SDL_QueueAudio(manager.getAudioDeviceID(), dst.data(), static_cast<unsigned>(dst.size()));

                m_buffer.erase(m_buffer.begin(), std::next(m_buffer.begin(), min));
                m_position_in_bytes += min;
            }
        }
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(10ms);
    }
}
