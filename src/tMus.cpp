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

#include "tMus.hpp"

#include "AudioLoop.hpp"
#include "Colors.hpp"
#include "Renderer.hpp"
#include "util.hpp"
#include "Factories.hpp"

#include <algorithm>
#include <ncpp/NotCurses.hh>
#include <fcntl.h>

static void SetupCallbacks(ListView& albumViewRef, ListView& songViewRef)
{
    albumViewRef.setSelectCallback([&songViewRef](const std::filesystem::path& path)
    {
        util::Log(color::moccasin, "album callback\n");
        std::vector<ListView::ItemType> songVec;
        for (const auto& file : std::filesystem::directory_iterator(path))
        {
            const auto ext = file.path().extension();
            if (ext == ".cue" || ext == ".jpg" || ext == ".png" || ext == ".m3u")
                continue;

            unsigned cols = (unsigned)ncstrwidth(file.path().filename().c_str(), nullptr, nullptr);
            songVec.push_back({ cols, file.path() });
        }

        // Sort the items alphabetically
        std::ranges::sort(songVec, [](const auto& a, const auto& b)
        {
            return a.second.string() < b.second.string();
        });

        songViewRef.setItems(std::move(songVec));

        return true;
    });

    songViewRef.setEnterCallback([&](const std::filesystem::path& path)
    {
        util::Log(color::moccasin, "song callback\n");
        auto starter = [audio_path = path](std::stop_token tkn)
        {
            try
            {
                AudioLoop loop{ audio_path };
                loop.consumer_loop(tkn);
            }
            catch (const std::runtime_error& e)
            {
                util::Log(color::red, "Runtime error: {}", e.what());
            }
            catch (const std::exception& e)
            {
                util::Log(color::red, "Exception: ", e.what());
            }
            catch (...)
            {
                util::Log(color::red, "Unhandled exception caught\n");
            }
        };

        if (playbackThread.joinable())
        {
            playbackThread.request_stop();
            playbackThread.join();
        }

        playbackThread = std::jthread{ starter };
        pthread_setname_np(playbackThread.native_handle(), "Consumer loop");

        return true;
    });
}

tMus::tMus(const std::shared_ptr<FocusManager>& manager)
{
    auto [albumPlane, songPlane, commandPlane] = MakePlanes();

    const auto albumView = std::make_shared<ListView>(std::move(albumPlane), manager->m_CurrentFocus);
    const auto songView  = std::make_shared<ListView>(std::move(songPlane), manager->m_LastFocus);

    auto cmdProcessor = MakeCommandProcessor(albumView, songView);

    cfg = std::make_shared<Config>( util::GetUserConfigDir() / "tMus.ini", cmdProcessor );

    const auto cmdView{ std::make_shared<CommandView>(std::move(commandPlane), cmdProcessor) };

    m_views[0] = cmdView;
    m_views[1] = albumView;
    m_views[2] = songView;

    auto& [_, albumViewRef, songViewRef] = m_views;

    manager->m_CurrentFocus->setNotify([&]()
    {
        util::Log("Current Focus Notify Set\n");
        std::get<std::shared_ptr<ListView>>(albumViewRef)->ColorSelected();
    });

    manager->m_LastFocus->setNotify([&]()
    {
        util::Log("Last Focus notify Set\n");
        std::get<std::shared_ptr<ListView>>(songViewRef)->ColorSelected();
    });

    SetupCallbacks(*std::get<std::shared_ptr<ListView>>(albumViewRef),
                   *std::get<std::shared_ptr<ListView>>(songViewRef));
}

void tMus::loop()
{
    using namespace std::chrono;

    Renderer::Render(); // Initial render

    int count = 0;

    auto* notcurses = ncpp::NotCurses::get_instance().get_notcurses();

    while (!Globals::stop_request)
    {
        ncinput ni;
        std::uint32_t input = notcurses_get_blocking(notcurses, &ni);

        // Get the time
        auto Start{ high_resolution_clock::now() };

        // Currently we don't want to handle 'Release' event.
        if (ni.evtype == ncpp::EvType::Release)
            continue;

        util::Log(color::yellow, "----------------------------------\n");

        if (const auto& cmdView = std::get<std::shared_ptr<CommandView>>(m_views[0]);
            input == ':' || cmdView->isFocused())
        {
            cmdView->handle_input(ni);
        }
        else
        {
            std::ignore = cfg->ProcessKeybinding(ni);
        }

        Renderer::Render();
        util::Log(color::green, "Frame: {} in {}\n", count++, duration_cast<microseconds>(high_resolution_clock::now() - Start));
    }
}

#if 0
    auto cb = +[]([[maybe_unused]] void* avcl, [[maybe_unused]] int level, const char* fmt, va_list args)
    {
        std::array<char, 1024> buf{0};
        vsnprintf(buf.data(), buf.size(), fmt, args);
        util::Log(color::lavender_blush, "{}", buf.data());

    };
#endif

void tMus::InitLog()
{
    if constexpr (util::internal::OutputFile)
    {
        util::internal::logFileStream = std::ofstream{ "/tmp/log.txt", std::ios::app };
    }

    util::internal::message_buf.reserve(4096);

    /*
     * Just be really quiet.
     */
    av_log_set_level(AV_LOG_QUIET);
#if 1
    auto cb = +[]([[maybe_unused]] void*,
                  [[maybe_unused]] int,
                  [[maybe_unused]] const char*,
                  [[maybe_unused]] va_list)
    { };
#endif
    av_log_set_callback(cb);
}

void tMus::Init()
{
    if (setlocale(LC_ALL, "") == nullptr)
    {
        throw std::runtime_error("Could not set locale to en_US.utf8\n");
    }

    const auto stdPlane = std::unique_ptr<ncpp::Plane>(ncpp::NotCurses::get_instance().get_stdplane());
    stdPlane->set_base("", 0, Colors::DefaultBackground);
}
