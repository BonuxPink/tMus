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

#include <ncpp/Utilities.hh>

void StatusView::draw(std::size_t time) const
{
    std::scoped_lock lk{ mtx };

    unsigned dimy = 0, dimx = 0;
    m_ncp->get_dim(dimy, dimx);
    m_ncp->set_channels(Colors::DefaultBackground);
    m_ncp->cursor_move(dimy - 1, 0);

    ncpp::Cell transchar{};
    m_ncp->hline(transchar, dimx - 1);

    m_ncp->cursor_move(dimy - 1, 0);

    if (m_url.empty())
        return;

    auto secondsToTime = [](int seconds)
    {
        int hours = seconds / 3600;
        int minutes = (seconds / 60) % 60;
        int secs = seconds % 60;

        if (hours < 1)
        {
            return std::format("{:02}:{:02}", minutes, secs);
        }
        else
        {
            return std::format("{:02}:{:02}:{:02}", hours, minutes, secs);
        }
    };

    const auto seconds          = static_cast<int>(time) / m_bytes_per_second;
    const auto durationStr      = secondsToTime(static_cast<int>(m_ctx_data->format_ctx->duration / AV_TIME_BASE));
    const auto currentSecondStr = secondsToTime(static_cast<int>(seconds));

    const auto filename = std::format("{} > {} / {}", m_url, currentSecondStr, durationStr);
    const auto* cStr    = filename.c_str();

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
