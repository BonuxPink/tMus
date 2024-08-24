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

#include "AudioLoop.hpp"
#include "tMus.hpp"
#include "util.hpp"

#include <cstdlib>
#include <exception>

#include <stdexcept>

int main() try
{
    notcurses_options opts{ .termtype = nullptr,
                            .loglevel = NCLOGLEVEL_WARNING,
                            .margin_t = 0, .margin_r = 0,
                            .margin_b = 0, .margin_l = 0,
                            .flags = NCOPTION_SUPPRESS_BANNERS | NCOPTION_CLI_MODE };

    ncpp::NotCurses nc{ opts };

    tMus::Init();
    tMus::InitLog();

    const auto albumViewFocus = std::make_shared<Focus>();
    const auto songViewFocus  = std::make_shared<Focus>();
    const auto manager = std::make_shared<FocusManager>(albumViewFocus, songViewFocus);

    auto tmus = std::make_unique<tMus>(manager);

#ifdef DEBUG
    if (std::filesystem::exists("/home/meow/Music"))
    {
        auto proc = std::get<std::shared_ptr<CommandView>>(tmus->GetViewRef())->getCmdProc();
        proc->processCommand("add ~/Music");
    }
#endif

    tmus->loop();

    if (playbackThread.joinable())
    {
        playbackThread.join();
    }

    util::Log(color::green, "Program exiting\n");
    return EXIT_SUCCESS;
}
catch (const std::runtime_error& e)
{
    util::Log(color::red, "Exception caught with: {}\n", e.what());
}
catch (const std::exception& e)
{
    util::Log(color::red, "Exception caught with: {}\n", e.what());
}
catch (...)
{
    util::Log(color::red, "Unknown exception caught\n");
}
