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

extern "C"
{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

#include <ncpp/NotCurses.hh>
#include <ncpp/Widget.hh>

#include <memory>
#include <thread>

class StatusView
{
    struct constructor_tag { explicit constructor_tag() = default; };
public:

    StatusView(std::shared_ptr<AVFormatContext>,
               std::shared_ptr<AVCodecContext>,
               constructor_tag);

    static std::unique_ptr<StatusView> instance;

    static void Create(std::shared_ptr<AVFormatContext>,
                std::shared_ptr<AVCodecContext>);

    static void draw(std::size_t override = 0);
    static void DestroyStatusPlane();
    bool handle_input(const ncinput&) noexcept;
private:

    void internal_draw(std::size_t override);

    mutable std::mutex mtx;
    ncpp::Plane* m_ncp{};
    std::shared_ptr<AVFormatContext> m_formatCtx;
    std::shared_ptr<AVCodecContext> m_avctx;
};

inline std::unique_ptr<StatusView> StatusView::instance;
