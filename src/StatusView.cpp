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
#include "util.hpp"

#include <fmt/format.h>
#include <ncpp/Utilities.hh>

StatusView::StatusView(ContextData& ctx_data,
                       constructor_tag)
    : m_ctx_data{ &ctx_data }
    , m_url{ m_ctx_data->format_ctx->url }
{
    const auto getBytesPerSecond = [this]
    {
        const auto cc = m_ctx_data->codec_ctx;

        const auto bitsPerSample = av_get_bytes_per_sample(cc->sample_fmt) * 8;
        const auto sampleRate    = cc->sample_rate;
        const auto channels      = cc->ch_layout.nb_channels;

        return (bitsPerSample * sampleRate * channels) / 8;
    };

    m_bytes_per_second = getBytesPerSecond();

    auto pos = m_url.find_last_of('/');
    m_url.remove_prefix(pos + 1);
}

void StatusView::Create(ContextData& ctx_data)
{
    instance = std::make_unique<StatusView>(ctx_data, constructor_tag{});
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
        int hours = seconds / 3600;
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

    const auto seconds          = static_cast<int>(time) / m_bytes_per_second;
    const auto durationStr      = secondsToTime(static_cast<int>(m_ctx_data->format_ctx->duration / AV_TIME_BASE));
    const auto currentSecondStr = secondsToTime(static_cast<int>(seconds));

    const auto filename = fmt::format("{} > {} / {}", m_url, currentSecondStr, durationStr);
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
