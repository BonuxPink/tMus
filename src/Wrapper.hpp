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

#include "util.hpp"

#include <memory>
#include <stdexcept>
#include <new>

#include <ncpp/NotCurses.hh>

extern "C"
{
    #include <libavcodec/packet.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/channel_layout.h>
    #include <libavformat/avformat.h>
    #include <libavutil/dict.h>
}

/*
 * Collection of simple RAII wrappers
 */

namespace Wrap
{
    template <typename T> struct DeleterOf;

    template <> struct DeleterOf<::AVPacket>        { void operator()(::AVPacket* p)        { ::av_packet_free(&p); } };
    template <> struct DeleterOf<::AVFrame>         { void operator()(::AVFrame* f)         { ::av_frame_free(&f); } };
    template <> struct DeleterOf<::AVDictionary>    { void operator()(::AVDictionary* p)    { ::av_dict_free(&p); } };

    template <typename T> using UniquePtr = std::unique_ptr<T, DeleterOf<T>>;

    inline auto make_packet()
    {
        auto packet = UniquePtr<AVPacket>{ av_packet_alloc() };
        if (!packet)
        {
            util::Log("Failed to allocate packet\n");
            throw std::runtime_error(std::format("Error occurred while allocating packet {}\n", AVERROR(ENOMEM)));
        }

        return packet;
    }

    inline constexpr auto deleter = [](auto* ptr) { operator delete[](ptr, std::align_val_t(16)); };
    using align_buf_t = std::unique_ptr<std::uint8_t, decltype(deleter)>;

    inline auto make_aligned_buffer()
    {
        return align_buf_t
        {
            std::bit_cast<std::uint8_t*>(operator new[](32'000 + 16, std::align_val_t(16)))
        };
    }

    struct AvPacket
    {
        AvPacket(std::size_t size)
        {
            if (av_new_packet(&pkt, static_cast<int>(size)) != 0)
            {
                throw std::runtime_error("av_new_packet failed");
            }
        }

        AvPacket(const AvPacket&) = default;
        AvPacket(AvPacket&&) = delete;
        AvPacket& operator=(const AvPacket &) = default;
        AvPacket& operator=(AvPacket &&) = delete;

        ~AvPacket()
        {
            av_packet_unref(&pkt);
        }

        void reset()
        {
            av_packet_unref(&pkt);
        }

        operator AVPacket*() { return &pkt; }

        const AVPacket* operator->() const noexcept
        {
            return &pkt;
        }

        AVPacket* operator->() noexcept
        {
            return &pkt;
        }

    private:
        AVPacket pkt{};
    };

    struct AvFrame
    {
        AvFrame()
            : frame{ av_frame_alloc() }
        {
            if (frame == nullptr)
            {
                throw std::runtime_error("Failed to alloc avframe");
            }
        }

        AvFrame(const AvFrame&)            = delete;
        AvFrame(AvFrame&&)                 = delete;
        AvFrame& operator=(const AvFrame&) = delete;
        AvFrame& operator=(AvFrame&&)      = delete;

        ~AvFrame()
        {
            av_frame_free(&frame);
        }

        operator AVFrame*() const { return frame; }

        const AVFrame* operator->() noexcept
        {
            return frame;
        }

    private:
        AVFrame* frame;
    };
}
