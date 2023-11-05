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

#include "AudioLoop.hpp"
#include "Constants.hpp"
#include "Frame.hpp"
#include "Wrapper.hpp"
#include "Decoder.hpp"
#include "globals.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <span>
#include <thread>

#include <SDL2/SDL_audio.h>

extern "C"
{
    #include <libavutil/frame.h>
    #include <libavcodec/packet.h>
    #include <libavutil/avutil.h>
    #include <libavcodec/avcodec.h>
    #include <libavcodec/codec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/dict.h>
}

static int decode_interrput_cb([[maybe_unused]] void* ctx)
{
    return Globals::abort_request;
};

void audioThread(std::stop_token stop_tkn, Decoder* d, FrameQueue* queue)
{
    std::shared_ptr<AVFrame> alloc { av_frame_alloc(), [](AVFrame *f) { av_frame_free(&f); } };
    if (!alloc)
    {
        throw std::runtime_error("Failed to allocate frame");
    }

    std::atomic<std::shared_ptr<AVFrame>> frame { alloc };

    do
    {
        int gotFrame = d->decode_frame(frame.load());
        if (gotFrame < 0)
        {
            return;
        }

        if (gotFrame)
        {
            auto tb  = AVRational{ .num = 1, .den = frame.load()->sample_rate };
            auto af = queue->peek_writable();
            if (af == nullptr)
            {
                util::Log(fg(fmt::color::yellow), "af is null\n");
                return;
            }

            af->pts = (frame.load()->pts == AV_NOPTS_VALUE) ? double(NAN) :
                static_cast<double>(frame.load()->pts) * av_q2d(tb);

            af->pos = frame.load()->pkt_pos;
            af->duration = av_q2d(AVRational{ frame.load()->nb_samples, frame.load()->sample_rate });

            av_frame_move_ref(af->frame, frame.load().get());
            queue->push();
        }

    } while (!stop_tkn.stop_requested() || !Globals::abort_request);

    util::Log(fg(fmt::color::yellow), "Finished audioThread\n");
}

AudioLoop::AudioLoop(const std::filesystem::path& path, std::shared_ptr<PacketQueue> audioq, std::shared_ptr<FrameQueue> fq)
    : m_format_ctx(Wrap::make_format_context())
    , m_packetQ(std::move(audioq))
    , m_frameQ(std::move(fq))
{
    m_format_ctx->interrupt_callback.callback = decode_interrput_cb;
    m_format_ctx->interrupt_callback.opaque = nullptr;

    m_packetQ->cv = &cv;

    try
    {
        openFile(path);
        findStream();
    }
    catch (const std::runtime_error& e)
    {
        util::Log("Err: {}\n", e.what());
        throw;
    }
    catch (...)
    {
        util::Log("Unknown error caught\n");
        throw;
    }
}

AudioLoop::~AudioLoop()
{
    streamClose();
}

void AudioLoop::streamClose() noexcept
{
    if (m_streamIndex < 0 || static_cast<unsigned>(m_streamIndex) >= m_format_ctx->nb_streams)
        return;

    auto* codecpar = m_format_ctx->streams[m_streamIndex]->codecpar;

    if (codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
    {
        util::Log("Somehow codec_type != AVMEDIA_TYPE_AUDIO\n");
    }

    av_freep(&m_audioBuf1);
    m_audioBuf1 = nullptr;
    m_audioBuf = nullptr;

    m_format_ctx->streams[m_streamIndex]->discard = AVDISCARD_ALL;

    m_streamIndex = -1;
}

static const constexpr int bufSize{ 128 };
static std::array<char, bufSize> errBuff { 0 };

void AudioLoop::handleSeekRequest()
{
    util::Log(fg(fmt::color::red), "Seek currently unsopported\n");
    return;
    std::int64_t seekTarget = seekPos;
    std::int64_t seekMin = seekRel > 0 ? seekTarget - seekRel + 2 : std::numeric_limits<std::int64_t>::min();
    std::int64_t seekMax = seekRel < 0 ? seekTarget - seekPos - 2 : std::numeric_limits<std::int64_t>::max();

    int ret = avformat_seek_file(m_format_ctx.get(), -1, seekMin, seekTarget, seekMax, m_seekFlags);
    if (ret < 0)
    {
        throw std::runtime_error(fmt::format("Error while seeking {}", m_format_ctx->url));
    }
    else
    {
        // if (audioStream >= 0)
        // {

        // }
    }
}

void AudioLoop::togglePause()
{
    if (paused)
    {
    }

}

int AudioLoop::decodeFrame()
{
    if (paused)
        return -1;

    const Frame* af{ m_frameQ->peek_readable() };
    if (!af)
    {
        util::Log(fg(fmt::color::red), "af is null\n");
        return -1;
    }
    m_frameQ->next();

    int resampledDataSize{};
    int data_size = av_samples_get_buffer_size(nullptr, af->frame->ch_layout.nb_channels,
                                               af->frame->nb_samples,
                                               static_cast<enum AVSampleFormat>(af->frame->format), 1);

    if (af->frame->format      != audioSrc.fmt
    || av_channel_layout_compare(&af->frame->ch_layout, &audioTgt.ch_layout)
    || !swr_ctx
    || af->frame->sample_rate  != audioSrc.freq)
    {
        swr_free(&swr_ctx);
        util::Log("fmt {}, freq {}", audioTgt.fmt, audioTgt.freq);
        swr_alloc_set_opts2(&swr_ctx,
                            &audioTgt.ch_layout, audioTgt.fmt, audioTgt.freq,
                            &af->frame->ch_layout,
                            static_cast<enum AVSampleFormat>(af->frame->format),
                            af->frame->sample_rate, 0, nullptr);
        if (!swr_ctx || swr_init(swr_ctx) < 0)
        {
            util::Log(fg(fmt::color::red), "Cannot create sample rate converter for conversion of {} Hz {} {} channels to {} Hz {} {} channels!\n",
                af->frame->sample_rate, av_get_sample_fmt_name(static_cast<enum AVSampleFormat>(af->frame->format)), af->frame->ch_layout.nb_channels,
                audioTgt.freq, av_get_sample_fmt_name(audioTgt.fmt), audioTgt.ch_layout.nb_channels);
            return -1;
        }
        if (av_channel_layout_copy(&audioSrc.ch_layout, &af->frame->ch_layout) < 0)
            return -1;

        audioSrc.freq = af->frame->sample_rate;
        audioSrc.fmt = static_cast<enum AVSampleFormat>(af->frame->format);
    }

    if (swr_ctx)
    {
        const auto** in = const_cast<const std::uint8_t**>(af->frame->extended_data);
        auto** out = &m_audioBuf1;
        auto out_count = af->frame->nb_samples * audioTgt.freq / af->frame->sample_rate + 256;
        auto out_size = av_samples_get_buffer_size(nullptr, audioTgt.ch_layout.nb_channels, out_count, audioTgt.fmt, 0);

        if (out_size < 0)
        {
            util::Log(fg(fmt::color::red), "av_samples_get_buffer_size() failed\n");
            return -1;
        }

        av_fast_malloc(&m_audioBuf1, &m_audioBuf1Size, out_size);
        if (!m_audioBuf1)
        {
            throw std::runtime_error("Out of memory\n");
        }

        int len2 = swr_convert(swr_ctx, out, out_count, in, af->frame->nb_samples);
        if (len2 < 0)
        {
            util::Log(fg(fmt::color::red), "swr_convert() failed\n");
            return -1;
        }

        if (len2 == out_count)
        {
            util::Log("audio buffer is probably too small\n");
            if (swr_init(swr_ctx) < 0)
                swr_free(&swr_ctx);
        }
        m_audioBuf = m_audioBuf1;
        resampledDataSize = len2 * audioTgt.ch_layout.nb_channels * av_get_bytes_per_sample(audioTgt.fmt);
    }
    else
    {
        m_audioBuf = af->frame->data[0];
        resampledDataSize = data_size;
    }

    /* update the audio clock with the pts */
    if (!std::isnan(af->pts))
        m_audioClock = af->pts + static_cast<double>(af->frame->nb_samples) / af->frame->sample_rate;
    else
        m_audioClock = NAN;

    return resampledDataSize;
}

void AudioLoop::FillAudioBuffer(std::uint8_t* stream, int len)
{
    using namespace std::chrono;
    const auto count = duration_cast<microseconds>(high_resolution_clock::now().time_since_epoch()).count();
    Globals::audio_callback_time = count / 100'000;

    while (len > 0)
    {
        if (m_audioBufIndex >= static_cast<int>(m_audioBufSize))
        {
           int audioSize = decodeFrame();
           if (audioSize < 0)
           {
               // if error, just output silence
               m_audioBuf = nullptr;
               m_audioBufSize = Constants::SDL_AUDIO_MIN_BUFFER_SIZE / audioTgt.frame_size * audioTgt.frame_size;
           }
           else
           {
               m_audioBufSize = audioSize;
           }

           m_audioBufIndex = 0;
        }

        int len1 = m_audioBufSize - m_audioBufIndex;

        if (len1 > len)
            len1 = len;

        if (!m_muted && m_audioBuf && m_audioVolume == SDL_MIX_MAXVOLUME)
        {
            memcpy(stream, (uint8_t *)m_audioBuf + m_audioBufIndex, len1);
        }
        else
        {
            memset(stream, 0, len1);
            if (!m_muted && m_audioBuf)
            {
                SDL_MixAudioFormat(stream, (uint8_t *)m_audioBuf + m_audioBufIndex, AUDIO_S16SYS, len1, m_audioVolume);
            }
        }
        len -= len1;
        stream += len1;
        m_audioBufIndex += len1;
    }

    if (!std::isnan(m_audioClock))
    {
        const double audioWriteBufSize = m_audioBufSize - m_audioBufIndex;
        const double pts = m_audioClock - (2 * m_audiohwBufSize + audioWriteBufSize) / audioTgt.bytes_per_sec;
    }
}

static bool stream_has_enough_packets(AVStream& st, int stream_id, const PacketQueue& queue)
{
    return stream_id < 0
        || queue.abort_request
        || (st.disposition & AV_DISPOSITION_ATTACHED_PIC)
        || (queue.nb_packets > Constants::MIN_FRAMES && (!queue.m_duration || av_q2d(st.time_base) * static_cast<double>(queue.m_duration) > 1.0));
}

void AudioLoop::loop(std::stop_token& t)
{
    using namespace std::chrono_literals;

    while (!t.stop_requested() && !Globals::abort_request)
    {
        auto event = [&]()
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
                    case PASUE:
                        togglePause();
                        SDL_PauseAudioDevice(2, paused);
                        break;
                }
                Globals::event.m_EventHappened = false;
            }
        };

        event();

        auto audioSt = m_format_ctx->streams[m_streamIndex];
        bool hasEnought = stream_has_enough_packets(*audioSt, m_streamIndex, *m_packetQ);
        std::mutex wait_mutex{};
        if (m_packetQ->m_size > Constants::MAX_QUEUE_SIZE || hasEnought) // Queue full
        {
            std::unique_lock lk{ wait_mutex };
            cv.wait_for(lk, 10ms);
            continue;
        }

        auto packet{ Wrap::make_packet() };
        int ret = av_read_frame(m_format_ctx.get(), packet.get());
        if (ret < 0)
        {
            if (ret == AVERROR_EOF || avio_feof(m_format_ctx->pb))
            {
                packet->stream_index = m_streamIndex;
                m_packetQ->put(packet.get());
            }

            if (m_format_ctx->pb && m_format_ctx->pb->error)
                break;

            std::unique_lock lk{ wait_mutex };
            cv.wait_for(lk, 10ms);
            continue;
        }

        auto check_packet_in_range = [&]()
        {
            if (Globals::duration == AV_NOPTS_VALUE)
            {
                return true;
            }

            auto time_base  = static_cast<std::int64_t>(av_q2d(m_format_ctx->streams[packet->stream_index]->time_base));

            auto start_time = m_format_ctx->streams[packet->stream_index]->start_time ;
            auto ts         = packet->pts == AV_NOPTS_VALUE ? packet->dts : packet->pts;
            auto duration   = Globals::duration / 1000000;
            auto offset     = Globals::start_time != AV_NOPTS_VALUE ? Globals::start_time : 0 / 1000000;

            return ts - (start_time != AV_NOPTS_VALUE ? start_time : 0) * time_base - offset <= duration;
        };

        if (packet->stream_index == m_streamIndex && check_packet_in_range())
        {
            m_packetQ->put(packet.get());
        }
        else
        {
            av_packet_unref(packet.get());
        }
    }

    cv.notify_all();

    if (Globals::abort_request)
    {
        util::Log("stop_requested() or abort requested\n");
    }
}

void AudioLoop::stop() noexcept
{
    util::Log(fg(fmt::color::yellow), "Notifying: {}\n", Globals::abort_request);
    m_packetQ->cv->notify_all();
}

void AudioLoop::openFile(const std::filesystem::path& filename)
{
    av_dict_set(&opt, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
    auto ptr = m_format_ctx.get();

    int err = avformat_open_input(&ptr, filename.c_str(), nullptr, &opt);
    if (err < 0)
    {
        util::Log(fmt::fg(fmt::color::red), "Error opening 'avformat_open_input'\n");

        av_strerror(err, errBuff.data(), bufSize);
        throw std::runtime_error(fmt::format("Erorr could not open input err_code: {}, filename: {}, errmsg: {}", err, filename.string(), errBuff.data()));
    }
    av_dict_set(&opt, "scan_all_pmts", nullptr, AV_DICT_MATCH_CASE);
    av_format_inject_global_side_data(m_format_ctx.get());
}

bool AudioLoop::findStream()
{
    int err = avformat_find_stream_info(m_format_ctx.get(), nullptr);
    if (err < 0)
    {
        throw std::runtime_error("Failed: avformat_find_stream_info");
    }

    if (m_format_ctx->pb)
        m_format_ctx->pb->eof_reached = 0;

    m_streamIndex = av_find_best_stream(m_format_ctx.get(), AVMEDIA_TYPE_AUDIO, m_streamIndex - 1, m_streamIndex, nullptr, 0);
    av_dump_format(m_format_ctx.get(), m_streamIndex, m_format_ctx->url, 0);

    if (AVERROR_STREAM_NOT_FOUND == m_streamIndex)
    {
        throw std::runtime_error(fmt::format("No streams found in {}\n", m_format_ctx->url));
    }
    else if (AVERROR_DECODER_NOT_FOUND == m_streamIndex)
    {
        throw std::runtime_error(fmt::format("Decoder not found in {}\n", m_format_ctx->url));
    }

    util::Log("st_index: {}\n", m_streamIndex);

    if (m_streamIndex < 0)
    {
        throw std::runtime_error("Stream index < 0\n");
    }

    return true;
}

std::shared_ptr<AVCodecContext> AudioLoop::streamOpen()
{
    auto codecContext = std::shared_ptr<AVCodecContext> { avcodec_alloc_context3(nullptr), [](::AVCodecContext* p) { ::avcodec_free_context(&p); } };

    int ret = avcodec_parameters_to_context(codecContext.get(), m_format_ctx->streams[m_streamIndex]->codecpar);
    if (ret < 0)
    {
        throw std::runtime_error(fmt::format("avcodec_parameters_to_context failed with code: {}", ret));
    }
    codecContext->pkt_timebase = m_format_ctx->streams[m_streamIndex]->time_base;

    if (codecContext->codec_type != AVMEDIA_TYPE_AUDIO)
    {
        throw std::runtime_error("Codec type is not audio!!");
    }

    const auto* codec = avcodec_find_decoder(codecContext->codec_id);
    if (!codec)
    {
        throw std::runtime_error("Filed in search of codec");
    }

    codecContext->codec_id = codec->id;

    av_dict_free(&opt);

    auto free_dict = [](AVDictionary* dict) { util::Log("Freeing dict\n"); av_dict_free(&dict); };
    using dictPtr = std::unique_ptr<AVDictionary, decltype(free_dict)>;
    dictPtr opts { opt };

    av_dict_set(&opt, "threads", "auto", 0);

    ret = avcodec_open2(codecContext.get(), codec, &opt);
    if (ret < 0)
    {
        throw std::runtime_error("Failed to open codec");
    }

    if (codecContext->codec_type != AVMEDIA_TYPE_AUDIO)
    {
        throw std::runtime_error("Codec is of wrong type");
    }

    util::Log("Avctx fmt: {}\n", codecContext->sample_fmt);

    m_audiohwBufSize = 0;
    m_format_ctx->streams[m_streamIndex]->discard = AVDISCARD_DEFAULT;

    return codecContext;
}
