#include <catch2/catch_test_macros.hpp>
#include <utility>

#include <fmt/core.h>
#include <fmt/format.h>

#include "../src/ControlManager.hpp"

static int glob = 0;

auto stub = [](){};

void func()
{
    glob++;
}

void last()
{
    glob--;
}

TEST_CASE ( "manager", "[Controller]" )
{
    auto X = std::make_shared<Control>(stub, nullptr, true);
    auto Y = std::make_shared<Control>(stub, nullptr);

    auto CM = std::make_shared<ControlManager>(X, Y);

    REQUIRE ( X->m_Data->hasFocus );

    auto b1 = CM->m_CurrentControl->hasFocus;
    auto b2 = CM->m_LastControl->hasFocus;

    CM->toggle();

    REQUIRE ( CM->m_CurrentControl->hasFocus == !b1 );
    REQUIRE ( CM->m_LastControl->hasFocus == !b2 );

    REQUIRE_FALSE ( X->m_Data->hasFocus );
    REQUIRE ( Y->m_Data->hasFocus );
}

TEST_CASE ( "Multifocus", "[Controller]" )
{
    auto X = std::make_shared<Control>(stub, nullptr, true);
    auto Y = std::make_shared<Control>(stub, nullptr);

    auto CM = std::make_shared<ControlManager>(X, Y);

    auto Z = std::make_shared<Control>(stub, CM.get());

    REQUIRE ( X->m_Data->hasFocus );

    Z->focus();

    REQUIRE ( Z->m_Data->hasFocus );
    REQUIRE_FALSE ( X->m_Data->hasFocus );

    // REQUIRE ( Z.get() == CM->m_CurrentControl.get() );
}

TEST_CASE ( "focus", "[Controller]" )
{
    auto X = std::make_shared<Control>(func, nullptr);
    auto Y = std::make_shared<Control>(last, nullptr);

    auto CM = std::make_shared<ControlManager>(X, Y);

    X->focus();

    REQUIRE_FALSE( glob == 0 );
    REQUIRE ( glob == 1 );

    REQUIRE ( X->m_Data->hasFocus );
    REQUIRE_FALSE ( Y->hasFocus() );

    Y->focus();

    REQUIRE_FALSE( glob == 1 );
    REQUIRE ( glob == 0 );

    REQUIRE ( Y->m_Data->hasFocus );
    REQUIRE_FALSE ( X->m_Data->hasFocus );
}

TEST_CASE ( "toggle", "[Controller]" )
{
    auto Current = std::make_shared<Control>(func, nullptr);
    auto Last = std::make_shared<Control>(last, nullptr);

    auto CM = std::make_shared<ControlManager>(Current, Last);

    REQUIRE (Current->hasFocus() == true);
    REQUIRE (Last->hasFocus() == false);

    CM->toggle();

    fmt::print("Current: {} {}\n", Current->hasFocus(), Current->m_Data->hasFocus);
    REQUIRE (Current->m_Data->hasFocus == false);
    REQUIRE (Last->m_Data->hasFocus == true);

    Current->toggle();

    REQUIRE ( Current->m_Data->hasFocus );
    REQUIRE_FALSE( Last->m_Data->hasFocus );
}
