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
#include <notcurses/notcurses.h>
#include <utility>

#include <fmt/core.h>
#include <fmt/format.h>

#include "FocusManager.hpp"

using namespace boost::ut;

auto stub = [](){};

static int glob = 0;

void func()
{
    glob++;
}

void last()
{
    glob--;
}

int main()
{
    "ControlManager"_test = []
    {
        auto X = std::make_shared<Focus>(stub, nullptr, true); // Initially focused
        auto Y = std::make_shared<Focus>(stub);

        auto CM = std::make_shared<FocusManager>(X, Y);

        expect ( X->hasFocus() );
        expect ( X->m_hasFocus );

        const auto has_focus = CM->m_CurrentFocus->m_hasFocus;
        const auto had_focus = CM->m_LastFocus->m_hasFocus;

        CM->toggle();

        expect ( CM->m_CurrentFocus->m_hasFocus == had_focus );
        expect ( CM->m_LastFocus->m_hasFocus    == has_focus );

        expect ( not X->m_hasFocus );
        expect ( Y->m_hasFocus );

        CM->toggle();

        expect ( CM->m_CurrentFocus->m_hasFocus != had_focus );
        expect ( CM->m_LastFocus->m_hasFocus    != has_focus );

        expect ( X->m_hasFocus );
        expect ( not Y->m_hasFocus );
    };

    "Multifocus"_test = []
    {
        notcurses_options opts{ .termtype = nullptr,
                                .loglevel = NCLOGLEVEL_FATAL,
                                .margin_t = 0, .margin_r = 0,
                                .margin_b = 0, .margin_l = 0,
                                .flags = NCOPTION_SUPPRESS_BANNERS,
        };

        ncpp::NotCurses nc{ opts };

        auto X = std::make_shared<Focus>(stub, nullptr, true);
        auto Y = std::make_shared<Focus>(stub, nullptr);

        auto CM = std::make_shared<FocusManager>(X, Y);

        auto Z = std::make_shared<Focus>(stub, CM.get());

        expect ( X->m_hasFocus );

        Z->focus();

        expect ( Z->m_hasFocus );
        expect ( not X->m_hasFocus );
    };

    "focus"_test = []
    {
        notcurses_options opts{ .termtype = nullptr,
                                .loglevel = NCLOGLEVEL_FATAL,
                                .margin_t = 0, .margin_r = 0,
                                .margin_b = 0, .margin_l = 0,
                                .flags = NCOPTION_SUPPRESS_BANNERS,
        };

        ncpp::NotCurses nc{ opts };

        auto X = std::make_shared<Focus>(func, nullptr);
        auto Y = std::make_shared<Focus>(last, nullptr);

        auto CM = std::make_shared<FocusManager>(X, Y);

        expect ( glob == 0 );

        X->focus();

        expect ( glob == 1 );

        expect ( X->m_hasFocus );
        expect ( Y->hasFocus() == false);

        Y->focus();

        expect ( glob == 0 );

        expect ( Y->m_hasFocus );
        expect ( not X->m_hasFocus );
    };

    "toggle"_test = []
    {
        auto Current = std::make_shared<Focus>(func, nullptr);
        auto Last    = std::make_shared<Focus>(last, nullptr);

        auto CM = std::make_shared<FocusManager>(Current, Last);

        expect (Current->hasFocus() == true);
        expect (Last->hasFocus()    == false);

        CM->toggle();

        expect (Current->m_hasFocus == false);
        expect (Last->m_hasFocus    == true);

        Current->toggle();

        expect ( Current->m_hasFocus );
        expect ( not Last->m_hasFocus );
    };
}
