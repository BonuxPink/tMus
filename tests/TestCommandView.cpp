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

#include "ut.hpp"

#include <ncpp/NotCurses.hh>
#include "CommandView.hpp"

using namespace boost::ut;
using namespace std::chrono_literals;

int main()
{
    "CommandView"_test = []
    {
        notcurses_options opts{ .termtype = nullptr,
                                .loglevel = NCLOGLEVEL_FATAL,
                                .margin_t = 0, .margin_r = 0,
                                .margin_b = 0, .margin_l = 0,
                                .flags = NCOPTION_SUPPRESS_BANNERS,
        };

        ncpp::NotCurses nc{ opts };

        unsigned dimy = 0, dimx = 0;

        const auto stdPlane = std::make_unique<ncpp::Plane>(*nc.get_stdplane(dimy, dimx));
        expect (stdPlane.get() != nullptr);

        ncpp::Plane plane{ dimy, dimx, static_cast<int>(dimy), 0, nullptr, nullptr };

        auto com = std::make_shared<CommandProcessor>();

        CommandView view{ std::move(plane), com };

        view.draw();

        expect (view.get_notcurses() == stdPlane.get()->get_notcurses());
        expect (nc.render());
    };
}
