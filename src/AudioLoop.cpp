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
#include "StatusView.hpp"
#include "globals.hpp"
#include "util.hpp"

AudioFileManager::AudioFileManager(const std::filesystem::path& filename, ContextData& ctx_data)
    : m_ctx_data      { &ctx_data }
{
    open_and_setup(filename);
    find_stream();
    stream_open();
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

        util::Log(color::yellow, "avformat_open_input: error code: {}, filename: {}, error msg: {}, errno: {}\n",
                  err, filename.string(), av_make_error_string(errBuff.data(), AV_ERROR_MAX_STRING_SIZE, err), strerror(errno));

        throw std::runtime_error(std::format("Error avformat_open_input, with code: {}, filename: {}, errmsg: {}", err, filename.string(), errBuff.data()));
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
        throw std::runtime_error(std::format("avcodec_parameters_to_context failed with code: {}", ret));
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
        throw std::runtime_error(std::format("No streams found in {}\n", m_ctx_data->format_ctx->url));
    }
    else if (AVERROR_DECODER_NOT_FOUND == m_streamIndex)
    {
        throw std::runtime_error(std::format("Decoder not found in {}\n", m_ctx_data->format_ctx->url));
    }

    if (m_streamIndex < 0)
    {
        throw std::runtime_error("Stream index < 0\n");
    }
}

AudioLoop::AudioLoop(const std::filesystem::path &path )
    : m_produced_buf { Wrap::make_aligned_buffer() }
    , m_ctx_data     {}
    , manager        { path, m_ctx_data }
    , swr            { *m_ctx_data.codec_ctx }
    , m_statusView   { m_ctx_data, swr.getAudioSettings() }
    , m_pipewire     { swr.getAudioSettings() }
{
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

    const auto& audioSettings = swr.getAudioSettings();
    util::Log(color::green, "Audio loop init init [fmt][freq][nb_ch]: [{}][{}][{}]\n", ConvertFmtToStr(audioSettings->fmt), audioSettings->freq, audioSettings->ch_layout.nb_channels);

    th_producer_loop = std::jthread{ [this](std::stop_token st) { this->producer_loop(st); } };
    pthread_setname_np(th_producer_loop.native_handle(), "Producer");
}

AudioLoop::~AudioLoop()
{
    th_producer_loop.request_stop();
    if (th_producer_loop.joinable())
    {
        th_producer_loop.join();
    }
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

    auto read_frame = [this](AVFormatContext* format_ctx, AVPacket* pkt)
    {
        std::scoped_lock lk{ m_format_mtx };

        auto ret = av_read_frame(format_ctx, pkt);
        if (ret < 0)
        {
            if (ret == AVERROR_EOF)
            {
                util::Log("av_read_frame: EOF\n");
                return -2;
            }

            std::array<char, 128> buf{};
            int error_ret = av_strerror(ret, buf.data(), buf.size());
            if (error_ret == 0)
                throw std::runtime_error(std::format("Error: {}\n", buf.data()));
            else
                throw std::runtime_error("Failed abtain erorr from av_strerorr");
        }

        return ret;
    };

    Wrap::AvPacket pkt{ 16'000 };
    while (true)
    {
        if (pkt->data)
        {
            pkt.reset();

            int ret = read_frame(m_ctx_data.format_ctx.get(), pkt);
            if (ret < 0)
            {
                if (ret == -2)
                    return ret;

                return 0;
            }

            // read_frame() likes to return other stream's
            // like mp3's stream which contains album art
            if (pkt->stream_index != manager.getStreamIndex())
                continue;
        }

        const auto cc = m_ctx_data.codec_ctx.get();

        if (int ret = send_packet(cc, pkt); ret != 0)
        {
            handle_error(ret);
        }

        Wrap::AvFrame frame{};
        int ret = avcodec_receive_frame(cc, frame);
        if (ret != 0)
        {
            if (ret == AVERROR_EOF)
            {
                util::Log(color::beige, "End of file reached\n");
                return -2;
            }

            std::array<char, 128> error_buf{};
            av_strerror(ret, error_buf.data(), error_buf.size());

            if (ret == AVERROR(EINVAL))
            {
                throw std::runtime_error(std::format("Codec is not open: {}", error_buf.data()));
            }

            // Output is not available in this state.
            util::Log("avcodec_receive_frame: {}\n", error_buf.data());
            continue;
        }

        if (swr)
        {
            const auto nb_samples = frame->nb_samples;
            auto buf_ptr = m_produced_buf.get();

            ret = swr.convert(&buf_ptr, nb_samples, frame->extended_data, nb_samples);

            int buffer_used_len = ret * cc->ch_layout.nb_channels * static_cast<int>(sizeof(int16_t));
            return buffer_used_len;
        }
        else
        {
            int buffer_used_len = av_samples_get_buffer_size(nullptr,
                                                             cc->ch_layout.nb_channels,
                                                             frame->nb_samples,
                                                             swr.getAudioFormat(), 1);
            if (buffer_used_len < 0)
            {
                std::array<char, 128> errbuf{};
                av_strerror(buffer_used_len, errbuf.data(), errbuf.size());
                util::Log("av_samples_get_buffer_size failed with: {}\n", errbuf.data());
                return 0;
            }

            memcpy(m_produced_buf.get(), frame->data[0], buffer_used_len);
            return buffer_used_len;
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

        if (int nr_read = FillAudioBuffer(); nr_read < 0)
        {
            if (nr_read != -1 || errno != EAGAIN)
            {
                nr_read = 0;
                /* TODO: handle EOF */
            }
            else
            {
                // Sleep and try again while data is not available
                util::Log("Sleep and try again\n");
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(50ms);
            }
        }
        else
        {
            std::scoped_lock lk{ m_buffer_mtx };

            if (auto ptr = m_produced_buf.get(); ptr)
            {
                std::ranges::copy(ptr,
                                std::next(ptr, nr_read),
                                std::back_inserter(m_buffer));
            }
        }
    }
}

void AudioLoop::handleSeekRequest(std::int64_t offset)
{
    {
        std::scoped_lock lk{ m_format_mtx };
        const auto cc                          = m_ctx_data.codec_ctx.get();
        const auto format                      = swr.getAudioFormat();
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
            util::Log(color::red, "Seek failed\n");
        }

        if (seek_target == 0)
            m_position_in_bytes = 0;
        else
            m_position_in_bytes += bytes_per_second * offset;

        m_statusView.draw(seek_target);
    }

    std::scoped_lock lk{ m_buffer_mtx };
    m_buffer.clear();
}

void AudioLoop::HandleEvent()
{
    if (Globals::event.m_EventHappened)
    {
        switch (Globals::event.act.load())
        {
        using enum Event::Action;

        case UP_VOLUME:
            if (Globals::m_audioVolume <= .99f)
            {
                Globals::m_audioVolume += 0.01f;
                m_pipewire.set_volume(Globals::m_audioVolume);
            }
            break;
        case DOWN_VOLUME:
            if (Globals::m_audioVolume > 0.f)
            {
                Globals::m_audioVolume -= 0.01f;
                m_pipewire.set_volume(Globals::m_audioVolume);
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
            break;
        }
        Globals::event.m_EventHappened = false;
    }
}

void AudioLoop::consumer_loop(std::stop_token& t)
{
    while (!t.stop_requested() && !Globals::stop_request)
    {
        HandleEvent();

        m_statusView.draw(m_position_in_bytes);

        if (m_paused == false)
        {
            std::scoped_lock lk{ m_buffer_mtx };

            auto ret = m_pipewire.write_audio(m_buffer.data(), m_buffer.size());

            const auto min = std::min(ret, m_buffer.size());

            m_buffer.erase(m_buffer.begin(), std::next(m_buffer.begin(), static_cast<long long>(min)));
            m_position_in_bytes += min;

            m_pipewire.period_wait();
        }
    }
}
