#include "ut.hpp"

#include "Factories.hpp"
#include "Config.hpp"
#include "tMus.hpp"

#include <fcntl.h>
#include <filesystem>

using namespace boost::ut;
namespace fs = std::filesystem;

int main()
{
    detail::cfg::abort_early = true;

    notcurses_options opts{ .termtype = nullptr,
                            .loglevel = NCLOGLEVEL_WARNING,
                            .margin_t = 0, .margin_r = 0,
                            .margin_b = 0, .margin_l = 0,
                            .flags = NCOPTION_SUPPRESS_BANNERS };

    ncpp::NotCurses nc{ opts };
    tMus::Init();

    "Config"_test = []
    {
        std::string_view config_text = { R"([Keybindings]
t     = up
h     = down
down  = down
up    = up
tab   = cyclefocus
right = seekforward
left  = seekback
n     = seekforward
d     = seekback
SPC   = togglepause
RET   = play
q     = quit
+     = volup
-     = voldown

[Audio]
volume = 30
)"};

        if (not fs::exists("/tmp/tmus-test/"))
        {
            expect (fs::create_directory("/tmp/tmus-test")) << "Failed to create directory /tmp/tmus-test";
        }

        auto fd = open("/tmp/tmus-test/test", O_TRUNC | O_CREAT | O_RDWR, 0600);
        expect (fd >= 0) << "Failed to open a file with: " << strerror(errno);

        auto written = write (fd, config_text.data(), config_text.size());
        expect (written > 0) << "Failed to write with: " << strerror(errno);

        auto [albumPlane, songPlane, _] = MakePlanes();
        auto a = std::make_shared<ListView>(std::move(albumPlane), std::make_shared<Focus>());
        auto b = std::make_shared<ListView>(std::move(songPlane), std::make_shared<Focus>());

        auto cmd_proc = MakeCommandProcessor(a, b);

        fs::path config_path{ "/tmp/tmus-test/test" };
        Config cfg{ config_path, cmd_proc };
    };
}
