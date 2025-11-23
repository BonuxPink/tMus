#pragma once

#include "Frontend.hpp"
#include "tMus.hpp"

#include <ncpp/NotCurses.hh>
#include <notcurses/notcurses.h>

namespace Frontends
{
    class NotCursesFrontend
    {
    public:

        explicit NotCursesFrontend();
        ~NotCursesFrontend() = default;

        NotCursesFrontend(const NotCursesFrontend&) = delete;
        NotCursesFrontend& operator=(const NotCursesFrontend&) = delete;

        NotCursesFrontend(NotCursesFrontend&&) = delete;
        NotCursesFrontend& operator=(NotCursesFrontend&&) = delete;

        void render(const Player::PlayerState& state)
        {

        }

        void poll_action()
        {

        }

    private:

        ncpp::NotCurses nc;

        auto to_action(const ncinput& input)
        {

        }
    };
}

static_assert(Frontend<Frontends::NotCursesFrontend>);

struct App
{
    App(Frontends::NotCursesFrontend& f)
        : frontend{ f }
    { }

    void run()
    {
        // while (frontend.poll_action())
        //     frontend.render(this);
    }

    Frontends::NotCursesFrontend& frontend;
};

// int main()
// {
//     auto frontend = std::make_unique<Frontends::NotCursesFrontend>();

// }
