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

#include <condition_variable>
#include <stdexcept>
#include <thread>
#include <functional>

#include "Frame.hpp"
#include "Wrapper.hpp"

extern "C"
{
    #include <libavcodec/packet.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/rational.h>
}

class Decoder
{
    Wrap::UniquePtr<AVPacket> pkt{};
    std::shared_ptr<PacketQueue> pq;
    std::shared_ptr<AVCodecContext> avctx{};
public:
    std::condition_variable* EmptyQueue{};
    std::int64_t start_pts{ AV_NOPTS_VALUE };
    AVRational start_pts_tb{};
    std::int64_t next_pts{};
    AVRational next_pts_tb{};
    std::jthread audioTh{};

    Decoder(const auto) = delete;

    Decoder(std::shared_ptr<AVCodecContext> avctx, std::shared_ptr<PacketQueue> in_queue, std::condition_variable* in_cv);
    Decoder(const auto&) = delete;
    Decoder& operator=(const auto&) = delete;

    void start(FrameQueue*);
    int decode_frame(const std::atomic<std::shared_ptr<AVFrame>> frame);
    void abort(FrameQueue& fq);
};
