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
#include <thread>

#include "ContextData.hpp"

class StatusView
{
    struct constructor_tag { explicit constructor_tag() = default; };
public:

    StatusView(ContextData&,
               constructor_tag);

    static std::unique_ptr<StatusView> instance;

    static void Create(ContextData& ctx_data);

    static void draw(std::size_t override = 0);
    static void DestroyStatusPlane();
    bool handle_input(const ncinput&) noexcept;
private:

    void internal_draw(std::size_t override);

    mutable std::mutex mtx;
    ncpp::Plane* m_ncp{};
    ContextData* m_ctx_data{};
    std::size_t m_bytes_per_second{};
    std::string_view m_url{};
};

inline std::unique_ptr<StatusView> StatusView::instance;
