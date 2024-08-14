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

#include "tMus.hpp"
#include "Colors.hpp"
#include "globals.hpp"

#include <ncpp/NotCurses.hh>

using namespace boost::ut;

int main()
{
    "Init"_test = []
    {
        notcurses_options opts{ .termtype = nullptr,
                                .loglevel = NCLOGLEVEL_WARNING,
                                .margin_t = 0, .margin_r = 0,
                                .margin_b = 0, .margin_l = 0,
                                .flags = NCOPTION_SUPPRESS_BANNERS,
        };

        ncpp::NotCurses nc{ opts };

        tMus::Init();

        const auto stdPlane = ncpp::NotCurses::get_instance().get_stdplane();

        ncpp::Cell c{};

        expect (stdPlane->is_valid());

        stdPlane->get_base(c);
        expect (c.get_channels() == Colors::DefaultBackground);
    };
}
