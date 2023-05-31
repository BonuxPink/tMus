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

#include "Decoder.hpp"
#include "util.hpp"
#include <exception>
#include <pthread.h>
#include <span>
#include <stop_token>
#include <utility>

extern "C"
{
    #include <libavutil/avutil.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/rational.h>
}

void audioThread(std::stop_token, Decoder*, FrameQueue*);
void Decoder::start(FrameQueue* fq)
{
    audioTh = std::jthread{ audioThread, this, fq };
    pthread_setname_np(audioTh.native_handle(), "audioThread");
}

int Decoder::decode_frame(const std::atomic<std::shared_ptr<AVFrame>> frame)
{
    int ret = AVERROR(EAGAIN);

    for (;;)
    {
        do
        {
            if (pq->abort_request || avctx->codec_type != AVMEDIA_TYPE_AUDIO)
            {
                util::Log(fg(fmt::color::red), "Abort request or codec_type is not audio\n");
                return -1;
            }

            ret = avcodec_receive_frame(avctx.get(), frame.load().get());
            if (ret >= 0)
            {
                AVRational tb { .num = 1, .den = frame.load()->sample_rate };

                if (frame.load()->pts != AV_NOPTS_VALUE)
                {
                    frame.load()->pts = av_rescale_q(frame.load()->pts, avctx->pkt_timebase, tb);
                    next_pts = frame.load()->pts + frame.load()->nb_samples;
                    next_pts_tb = tb;
                }
                else if (next_pts != AV_NOPTS_VALUE)
                {
                    frame.load()->pts = av_rescale_q(next_pts, next_pts_tb, tb);
                }
            }

            if (ret == AVERROR_EOF)
            {
                util::Log(fmt::fg(fmt::color::blue_violet), "Flushing\n");
                avcodec_flush_buffers(avctx.get());
                return 0;
            }

            if (ret >= 0)
                return 1;

        } while (ret != AVERROR(EAGAIN));

        if (pq->nb_packets == 0)
        {
            EmptyQueue->notify_one();
        }

        if (!pq->get(*pkt))
            return -1;

        avcodec_send_packet(avctx.get(), pkt.get());
        av_packet_unref(pkt.get());
    }

    util::Log(fg(fmt::color::blue_violet), "Exiting the loop\n");
}

void Decoder::abort(FrameQueue& fq)
{
    pq->abort();
    fq.signal();

    util::Log("Requesting stop\n");
    if (audioTh.joinable())
    {
        audioTh.request_stop();
        audioTh.join();
    }
}

Decoder::Decoder(std::shared_ptr<AVCodecContext> in_avctx, std::shared_ptr<PacketQueue> in_queue, std::condition_variable* in_cv)
    : pkt{ av_packet_alloc() }
    , pq{ std::move(in_queue) }
    , avctx{ std::move(in_avctx) }
    , EmptyQueue{in_cv}
{
    if (!pkt)
        throw std::runtime_error(fmt::format("Error occurred with code: {}\n", AVERROR(ENOMEM)));
}
