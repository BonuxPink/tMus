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
