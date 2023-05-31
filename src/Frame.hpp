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

#include "Constants.hpp"
#include <cstdint>
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <stdexcept>
#include <queue>

#include <fmt/format.h>

extern "C"
{
    #include <libavutil/frame.h>
    #include <libavutil/fifo.h>
    #include <libavcodec/packet.h>
    #include <libavcodec/avcodec.h>
}

struct Frame
{
    AVFrame* frame;
    double pts;       /* presentation timestamp for the frame */
    double duration;
    std::int64_t pos; /* byte position of the frame in the input file */
};

class PacketQueue
{
public:

    void put(AVPacket* packet);
    bool get(AVPacket& packet, bool block = true);
    void abort();

public:
    std::queue<AVPacket*> queue{};
    std::mutex mtx;
    std::atomic<std::int64_t> m_duration{};
    std::condition_variable* cv;

    std::atomic<int> m_size{};
    std::atomic<int> nb_packets{};

    std::atomic<bool> abort_request{};
};

struct StatusViewData
{
    double pts;
    int nb_samples;
    int sample_rate;
};

struct FrameQueue
{
private:
    Frame queue[Constants::FRAME_QUEUE_SIZE]{};
    std::atomic_bool* abort_request{};
    std::atomic_int rindex{};
    int windex{};
    std::atomic<int> rindex_shown{};
    int size{};
    int max_size{};
    int serial{};
    mutable std::mutex mtx{};
public:
    mutable std::condition_variable cv{};

    FrameQueue(const FrameQueue&) = delete;
    FrameQueue& operator=(const FrameQueue&) = delete;

    FrameQueue(std::atomic<bool>*);
    ~FrameQueue();

    void signal();
    [[nodiscard]] Frame* peek();
    void next();
    [[nodiscard]] Frame* peek_writable();
    [[nodiscard]] const Frame* peek_readable() const;
    [[nodiscard]] const StatusViewData getStatusViewData() const noexcept;
    void push();
};
