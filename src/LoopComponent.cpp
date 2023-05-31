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

    bool operator ()(ListView& v)
    {
        if (v.hasFocus())
        {
            return v.handle_input(m_ni);
        }

        return false;
    }

    bool operator ()([[maybe_unused]] StatusView& v)
    {
        // Status view does not hav handle_input
        v.draw();
        return false;
    }

    bool operator ()(CommandView& v)
    {
        return v.handle_input(m_ni);
    }

private:
    ncinput& m_ni;
};


static std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> Start{};

static ncinput GetKeyPress(notcurses* nc)
{
    ncinput ni;

    util::Log(fg(fmt::color::yellow), "----------------------------------\n");
    while (true)
    {
        notcurses_get_blocking(nc, &ni);
        Start = std::chrono::high_resolution_clock::now();
        if (ni.evtype == NCTYPE_RELEASE)
        {
            util::Log("continue\n");
            continue;
        }

        return ni;
    }
}

void LoopCompontent::loop(const ncpp::NotCurses& nc)
{
    nc.render();

    static int count = 0;
    while (!Globals::abort_request)
    {
        auto input = GetKeyPress(nc.get_notcurses());
        for (auto& e : m_vec)
        {
            bool vis = std::visit(ViewHandler{input}, e);

            if (vis || Globals::abort_request)
                break;
        }

        Renderer::Render();
        util::Log(fg(fmt::color::green), "Frame: {} in {}\n", count++, std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - Start));
    }
}
