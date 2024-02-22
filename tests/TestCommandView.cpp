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
        CommandView view{ plane };

        view.draw();

        expect (view.get_notcurses() == stdPlane.get()->get_notcurses());
        expect (nc.render());
    };
}
