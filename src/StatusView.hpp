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

#include <ncpp/NotCurses.hh>
#include <ncpp/Widget.hh>

#include <memory>

#include "AudioSettings.hpp"
#include "ContextData.hpp"
#include "Factories.hpp"

class StatusView
{
public:
    explicit StatusView(ContextData& ctx_data, std::shared_ptr<AudioSettings> audioSettings)
        : m_audioSettings    { std::move(audioSettings) }
        , m_ctx_data         { &ctx_data }
        , m_bytes_per_second { getBytesPerSecond() }
        , m_url              { m_ctx_data->format_ctx->url }
    {
        m_ncp = MakeStatusPlane();

        auto pos = m_url.find_last_of('/');
        m_url.remove_prefix(pos + 1);
    }

    void draw(std::size_t override = 0) const;

private:
    std::size_t getBytesPerSecond();

    mutable std::mutex mtx;
    std::shared_ptr<AudioSettings> m_audioSettings;
    std::unique_ptr<ncpp::Plane> m_ncp{};
    ContextData* m_ctx_data{};
    std::size_t m_bytes_per_second{};
    std::string_view m_url{};
};
