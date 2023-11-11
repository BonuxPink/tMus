#include <catch2/catch_test_macros.hpp>
#include <fcntl.h>
#include <ncpp/NotCurses.hh>
#include <notcurses/notcurses.h>

#include "../src/util.hpp"

TEST_CASE ( "TestInput", "[test]" )
{
    notcurses_options opts{ .termtype = nullptr,
                            .loglevel = NCLOGLEVEL_WARNING,
                            .margin_t = 0, .margin_r = 0,
                            .margin_b = 0, .margin_l = 0,
                            .flags = NCOPTION_SUPPRESS_BANNERS | NCOPTION_CLI_MODE,
    };

    ncpp::NotCurses nc{ opts };

    util::Log<void>::setFd(open("/tmp/log.txt", O_WRONLY | O_APPEND | O_CREAT, 0644));

    while (true)
    {
        ncinput input;
        notcurses_get_nblock(nc, &input);
        util::Log("Input: {}\n", input.id);

        using namespace std::chrono_literals;
        std::this_thread::sleep_for(10ms);
    }


}
