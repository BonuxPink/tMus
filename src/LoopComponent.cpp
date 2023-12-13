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

#include "LoopComponent.hpp"
#include "Renderer.hpp"
#include "util.hpp"
#include "globals.hpp"

#include <chrono>
#include <ncpp/NotCurses.hh>
#include <notcurses/notcurses.h>
#include <ratio>

LoopCompontent::LoopCompontent(std::vector<ViewLike>& vec)
    : m_vec(vec)
{ }

struct ViewHandler final
{
    ViewHandler(const auto) = delete;
    explicit ViewHandler(ncinput& ni)
        : m_ni(ni)
    { }

    bool operator ()(auto& v)
    {
        return v.handle_input(m_ni);
    }

private:
    ncinput& m_ni;
};


static std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> Start{};

static ncinput GetKeyPress()
{
    ncinput ni;

    util::Log(fg(fmt::color::yellow), "----------------------------------\n");

    notcurses_get_blocking(ncpp::NotCurses::get_instance().get_notcurses(), &ni);
    Start = std::chrono::high_resolution_clock::now();
    util::Log("Start: {}\n", Start);
    return ni;
}

void LoopCompontent::loop()
{
    Renderer::Render(); // Initial render

    static int count = 0;
    while (!Globals::stop_request)
    {
        auto input = GetKeyPress();
        for (auto& view : m_vec)
        {
            bool vis = std::visit(ViewHandler{input}, view);

            if (vis || Globals::stop_request)
                break;
        }

        Renderer::Render();
        util::Log(fg(fmt::color::green), "Frame: {} in {}\n", count++, std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - Start));
    }
}
