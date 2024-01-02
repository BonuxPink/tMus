#include "ut.hpp"

#include <ncpp/NotCurses.hh>
#include <notcurses/notcurses.h>
#include <utility>

#include <fmt/core.h>
#include <fmt/format.h>

#include "../src/ControlManager.hpp"

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
        auto X = std::make_shared<Control>(stub, nullptr, true); // Initially focused
        auto Y = std::make_shared<Control>(stub);

        auto CM = std::make_shared<ControlManager>(X, Y);

        expect ( X->hasFocus() );
        expect ( X->m_Data->hasFocus );

        const auto has_focus = CM->m_CurrentControl->hasFocus;
        const auto had_focus = CM->m_LastControl->hasFocus;

        CM->toggle();

        expect ( CM->m_CurrentControl->hasFocus == had_focus );
        expect ( CM->m_LastControl->hasFocus    == has_focus );

        expect ( not X->m_Data->hasFocus );
        expect ( Y->m_Data->hasFocus );

        CM->toggle();

        expect ( CM->m_CurrentControl->hasFocus != had_focus );
        expect ( CM->m_LastControl->hasFocus    != has_focus );

        expect ( X->m_Data->hasFocus );
        expect ( not Y->m_Data->hasFocus );
    };

    "Multifocus"_test = []
    {
        notcurses_options opts{ .termtype = nullptr,
                                .loglevel = NCLOGLEVEL_FATAL,
                                .margin_t = 0, .margin_r = 0,
                                .margin_b = 0, .margin_l = 0,
                                .flags = NCOPTION_SUPPRESS_BANNERS | NCOPTION_CLI_MODE,
        };

        ncpp::NotCurses nc{ opts };

        auto X = std::make_shared<Control>(stub, nullptr, true);
        auto Y = std::make_shared<Control>(stub, nullptr);

        auto CM = std::make_shared<ControlManager>(X, Y);

        auto Z = std::make_shared<Control>(stub, CM.get());

        expect ( X->m_Data->hasFocus );

        Z->focus();

        expect ( Z->m_Data->hasFocus );
        expect ( not X->m_Data->hasFocus );
    };

    "focus"_test = []
    {
        notcurses_options opts{ .termtype = nullptr,
                                .loglevel = NCLOGLEVEL_FATAL,
                                .margin_t = 0, .margin_r = 0,
                                .margin_b = 0, .margin_l = 0,
                                .flags = NCOPTION_SUPPRESS_BANNERS | NCOPTION_CLI_MODE,
        };

        ncpp::NotCurses nc{ opts };

        auto X = std::make_shared<Control>(func, nullptr);
        auto Y = std::make_shared<Control>(last, nullptr);

        auto CM = std::make_shared<ControlManager>(X, Y);

        expect ( glob == 0 );

        X->focus();

        expect ( glob == 1 );

        expect ( X->m_Data->hasFocus );
        expect ( Y->hasFocus() == false);

        Y->focus();

        expect ( glob == 0 );

        expect ( Y->m_Data->hasFocus );
        expect ( not X->m_Data->hasFocus );
    };

    "toggle"_test = []
    {
        auto Current = std::make_shared<Control>(func, nullptr);
        auto Last    = std::make_shared<Control>(last, nullptr);

        auto CM = std::make_shared<ControlManager>(Current, Last);

        expect (Current->hasFocus() == true);
        expect (Last->hasFocus()    == false);

        CM->toggle();

        expect (Current->m_Data->hasFocus == false);
        expect (Last->m_Data->hasFocus    == true);

        Current->toggle();

        expect ( Current->m_Data->hasFocus );
        expect ( not Last->m_Data->hasFocus );
    };
}
