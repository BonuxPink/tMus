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

#include "StatusView.hpp"
#include "Colors.hpp"
#include "Renderer.hpp"

#include <fmt/format.h>
#include <ncpp/Utilities.hh>

StatusView::StatusView(std::shared_ptr<AVFormatContext> formatCtx,
                       std::shared_ptr<AVCodecContext> avctx,
                       constructor_tag)
    : m_formatCtx(std::move(formatCtx))
    , m_avctx(std::move(avctx))
{ }

void StatusView::Create(std::shared_ptr<AVFormatContext> formatCtx, std::shared_ptr<AVCodecContext> avCodecCtx)
{
    instance = std::make_unique<StatusView>(formatCtx, avCodecCtx, constructor_tag{});
    instance->m_ncp = Globals::statusPlane.get();
}

bool StatusView::handle_input([[maybe_unused]] const ncinput& ni) noexcept
{
    return false;
}

void StatusView::draw(std::size_t time)
{
    std::scoped_lock lk{ instance->mtx };
    instance->internal_draw(time);
}

void StatusView::internal_draw(std::size_t time)
{
    unsigned dimy = 0, dimx = 0;
    m_ncp->get_dim(dimy, dimx);
    m_ncp->set_channels(Colors::DefaultBackground);
    m_ncp->cursor_move(dimy - 1, 0);

    ncpp::Cell transchar{};
    m_ncp->hline(transchar, dimx - 1);

    m_ncp->cursor_move(dimy - 1, 0);

    auto secondsToTime = [](int seconds)
    {
        int hours = seconds / 360;
        int minutes = (seconds / 60) % 60;
        int secs = seconds % 60;

        if (hours < 1)
        {
            return fmt::format("{:02}:{:02}", minutes, secs);
        }
        else
        {
            return fmt::format("{:02}:{:02}:{:02}", hours, minutes, secs);
        }
    };

    const auto bytesPerSecond = 176400;
    const auto seconds        = static_cast<int>(time) / bytesPerSecond;

    const auto durationStr = secondsToTime(static_cast<int>(m_formatCtx->duration / AV_TIME_BASE));
    const auto currentSecondStr = secondsToTime(static_cast<int>(seconds));

    std::string_view url{ m_formatCtx->url };
    auto pos = url.find_last_of('/');

    std::string filename = fmt::format("{} > {} / {} {}", m_formatCtx->url, currentSecondStr, durationStr, std::floor(seconds));
    filename.erase(0, pos + 1);

    const auto* cStr = filename.c_str();

    std::size_t sizeInBytes{ 0 };
    while (*cStr)
    {
        int cols = ncplane_putegc_yx(m_ncp->to_ncplane(), -1, -1, cStr, &sizeInBytes);
        cStr += sizeInBytes;

        if (cols < 0)
            break;

        if (sizeInBytes == 0)
            break;
    }

    Renderer::Render();
}
