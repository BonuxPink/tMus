
#include "../src/AudioLoop.hpp"
#include "../src/StatusView.hpp"
#include "../src/Factories.hpp"
#include "../src/globals.hpp"

#include "ut.hpp"

using namespace boost::ut;

int main()
{
    auto p = std::filesystem::path("/home/meow/Music/ACDC/01 - Hells Bells.mp3");

    "AudioLoop sound"_test = [&]
    {
        auto th = [&](std::stop_token tkn)
        {
            SDL_Init(SDL_INIT_AUDIO);

            AudioLoop loop { p };

            notcurses_options opts{ .termtype = nullptr,
                                    .loglevel = NCLOGLEVEL_SILENT,
                                    .margin_t = 0, .margin_r = 0,
                                    .margin_b = 0, .margin_l = 0,
                                    .flags = NCOPTION_SUPPRESS_BANNERS | NCOPTION_CLI_MODE,
            };

            ncpp::NotCurses nc{ opts };

            auto statusPlane = MakeStatusPlane();
            Globals::statusPlane = &statusPlane;

            StatusView::Create(loop.getFormatCtx(), loop.getCodecContext());

            loop.consumer_loop(tkn);
        };

        auto thread = std::jthread { th };
        // using namespace std::chrono_literals;
        // std::this_thread::sleep_for(5s);
        // REQUIRE (true) ;
    };
}
