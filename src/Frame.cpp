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

#include "Frame.hpp"

#include <thread>
#include <utility>
#include "Constants.hpp"
#include "globals.hpp"
#include "util.hpp"

#include <condition_variable>

void PacketQueue::put(AVPacket* packet)
{
    if (abort_request)
        return;

    std::scoped_lock lk{ mtx };

    AVPacket* pkt_copy = av_packet_alloc();
    if (!pkt_copy)
    {
        av_packet_unref(packet);
        return;
    }
    av_packet_move_ref(pkt_copy, packet);

    queue.push(pkt_copy);

    nb_packets++;
    m_size += packet->size + static_cast<int>(sizeof(AVPacket*));
    m_duration += packet->duration;

    cv->notify_one();
}

bool PacketQueue::get(AVPacket& packet, bool block )
{
    if (abort_request)
        return false;

    std::unique_lock lk{ mtx };

    while (queue.empty() && !abort_request && block)
    {
        cv->wait(lk);
    }

    if (queue.empty())
        return false;

    auto* pkt = queue.front();
    packet = *pkt;

    queue.pop();

    m_size -= pkt->size + static_cast<int>(sizeof(AVPacket*));
    m_duration -= pkt->duration;
    nb_packets--;

    return true;
}

void PacketQueue::abort()
{
    std::lock_guard lk{ mtx };
    abort_request = true;
    cv->notify_all();
}

FrameQueue::FrameQueue(std::atomic<bool>* abReq)
    : abort_request(abReq)
    , max_size(Constants::FRAME_QUEUE_SIZE)
{
    for (int i = 0; i < max_size; i++)
    {
        queue[i].frame = av_frame_alloc();
        if (!queue[i].frame)
            throw std::runtime_error(fmt::format("Error occured: {}", AVERROR(ENOMEM)));
    }
}

FrameQueue::~FrameQueue()
{
    for (int i = 0; i < max_size; i++)
    {
        Frame* vp = &queue[i];
        av_frame_unref(vp->frame);
        av_frame_free(&vp->frame);
    }
}

void FrameQueue::signal()
{
    const std::lock_guard lk{ mtx };
    cv.notify_one();
}

Frame* FrameQueue::peek()
{
    return &queue[(rindex + rindex_shown) % max_size];
}

void FrameQueue::next()
{
    if (!rindex_shown)
    {
        rindex_shown = 1;
        return;
    }
    av_frame_unref(queue[rindex].frame);
    if (++rindex == max_size)
        rindex = 0;
    std::unique_lock lk{ mtx };
    size--;
    cv.notify_one();
}

Frame* FrameQueue::peek_writable()
{
    std::unique_lock lk{ mtx };

    while (size >= max_size && !*abort_request)
    {
        cv.wait(lk);
    }

    if (*abort_request)
        return nullptr;

    return &queue[windex];
}

const Frame* FrameQueue::peek_readable() const
{
    std::unique_lock lk{ mtx };

    while (size - rindex_shown <= 0 && !*abort_request)
    {
        cv.wait(lk);
    }

    if (*abort_request)
        return nullptr;

    return &queue[(rindex + rindex_shown) % max_size];
}

void FrameQueue::push()
{
    if (++windex == max_size)
        windex = 0;

    std::unique_lock lk{ mtx };
    size++;
    cv.notify_one();
}

const StatusViewData FrameQueue::getStatusViewData() const noexcept
{
    const std::lock_guard lk{ mtx };
    auto frame = &queue[(rindex + rindex_shown) % max_size];

    return {
        .pts         = frame->pts,
        .nb_samples  = frame->frame->nb_samples,
        .sample_rate = frame->frame->sample_rate
    };
}
