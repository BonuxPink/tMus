/*
 * Copyright (C) 2023 Dāniels Ponamarjovs <bonux@duck.com>
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

#include "AudioDevice.hpp"
#include "AudioLoop.hpp"
#include "Colors.hpp"
#include "CustomViews.hpp"
#include "Factories.hpp"
#include "LoopComponent.hpp"
#include "StatusView.hpp"
#include "Wrapper.hpp"
#include "globals.hpp"
#include "util.hpp"

#include <SDL2/SDL_audio.h>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <functional>

#include <SDL2/SDL.h>

#include <pthread.h>
#include <stdexcept>
#include <thread>

extern "C"
{
    #include <libavutil/log.h>
    #include <libavutil/channel_layout.h>
}

static void init_logger()
{
    int fd = open("/tmp/log.txt", O_CREAT | O_APPEND | O_WRONLY, 0644);
    util::Log<void>::setFd(fd);

#if 0
    auto cb = +[]([[maybe_unused]] void* avcl, [[maybe_unused]] int level, const char* fmt, va_list args)
    {
        std::array<char, 1024> buf{0};
        vsnprintf(buf.data(), buf.size(), fmt, args);

        auto size = strnlen(buf.data(), buf.size());

        auto err = write(util::internal::fd, buf.data(), size);
        if (err < 0)
        {
            fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "Failed to write to file: {}\n", strerror(errno));
        }

    };

    av_log_set_level(AV_LOG_ERROR);
    av_log_set_callback(cb);
#else
    /*
     * Just be really quiet.
     */
    av_log_set_level(AV_LOG_QUIET);
    auto cb = +[]([[maybe_unused]] void*,
                  [[maybe_unused]] int,
                  [[maybe_unused]] const char*,
                  [[maybe_unused]] va_list)
    { };
    av_log_set_callback(cb);
#endif
}

static void SetupCallbacks(ListView& albumViewRef, ListView& songViewRef)
{
    albumViewRef.setSelectCallback([&songViewRef](const std::filesystem::path& path)
    {
        std::vector<ListView::ItemType> songVec;
        for (const auto& file : std::filesystem::directory_iterator(path))
        {
            const auto ext = file.path().extension();
            if (ext == ".cue" || ext == ".jpg" || ext == ".png" || ext == ".m3u")
                continue;

            unsigned cols = (unsigned)ncstrwidth(file.path().filename().c_str(), nullptr, nullptr);
            songVec.push_back({ cols, file.path() });
        }

        songViewRef.setItems(std::move(songVec));

        return true;
    });

    songViewRef.setEnterCallback([&](const std::filesystem::path& path)
    {
        auto starter = [audio_path = path](std::stop_token tkn)
        {
            try
            {
                AudioLoop state{ audio_path };
                StatusView::Create(state.getFormatCtx(), state.getCodecContext());
                state.consumer_loop(tkn);
            }
            catch (const std::runtime_error& e)
            {
                util::Log(fg(fmt::color::red), "Runtime error: {}", e.what());
            }
            catch (const std::exception& e)
            {
                util::Log(fg(fmt::color::red), "Exception: ", e.what());
            }
            catch (...)
            {
                util::Log(fg(fmt::color::red), "Unhandled exception caught\n");
            }
        };

        if (playbackThread.joinable())
        {
            playbackThread.request_stop();
            playbackThread.join();
        }

        playbackThread = std::jthread{ starter };
        pthread_setname_np(playbackThread.native_handle(), "Main loop");

        return true;
    });
}

int main()
{
    init_logger();

    if (setlocale(LC_ALL, "en_US.utf8") == nullptr)
    {
        util::Log("Could not set locale to en_US.utf8\n");
        return EXIT_FAILURE;
    }

    if (!SDL_getenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE"))
        SDL_setenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE", "1", 1);

    if (SDL_Init(SDL_INIT_AUDIO))
    {
        util::Log("{}\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    notcurses_options opts{ .termtype = nullptr,
                            .loglevel = NCLOGLEVEL_WARNING,
                            .margin_t = 0, .margin_r = 0,
                            .margin_b = 0, .margin_l = 0,
                            .flags = NCOPTION_SUPPRESS_BANNERS | NCOPTION_CLI_MODE,
    };

    ncpp::NotCurses nc{ opts };

    auto statusPlane = MakeStatusPlane();
    Globals::statusPlane = &statusPlane;

    const auto stdPlane = Wrap::getStdPlane();
    stdPlane->set_base("", 0, Colors::DefaultBackground);

    auto [albumPlane, songPlane, commandPlane] = MakePlanes(**stdPlane);

    const auto albumViewFocus = std::make_shared<Control>();
    const auto songViewFocus  = std::make_shared<Control>();
    const auto manager        = std::make_shared<ControlManager>(albumViewFocus, songViewFocus);

    std::vector<LoopComponent::ViewLike> views;
    views.push_back( CommandView { commandPlane });
    views.push_back( ListView    { albumPlane, albumViewFocus });
    views.push_back( ListView    { songPlane, songViewFocus });

    auto& cmdViewRef   = std::get<CommandView>(views[0]);
    auto& albumViewRef = std::get<ListView>(views[1]);
    auto& songViewRef  = std::get<ListView>(views[2]);

    albumViewFocus->setNotify([&](){ albumViewRef.ColorSelected(); });
    songViewFocus->setNotify([&]() { songViewRef.ColorSelected(); });

    SetupCallbacks(albumViewRef, songViewRef);

    auto cmdProcessor = MakeCommandProcessor(albumViewRef, songViewRef);
    cmdViewRef.init(&cmdProcessor);

#ifdef DEBUG
    cmdProcessor.processCommand("add ~/Music");
#endif

    LoopComponent loop{ views };
    loop.loop();

    if (playbackThread.joinable())
    {
        playbackThread.join();
    }

    util::Log(fg(fmt::color::green), "Program exiting\n");
    SDL_Quit();
    SDL_TLSCleanup();
    return EXIT_SUCCESS;
}
