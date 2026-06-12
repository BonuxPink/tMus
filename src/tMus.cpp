/*
 * Copyright (C) 2023-2025 Dāniels Ponamarjovs <bonux@duck.com>
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
#include "ServerClient.hpp"

#include <algorithm>
#include <ncpp/NotCurses.hh>
#include <fcntl.h>

static void SetupCallbacks(ListView& albumViewRef, ListView& songViewRef, CommandView& cmdView,
                           std::shared_ptr<ServerClient> client)
{
    albumViewRef.setSelectCallback([&songViewRef, client](const std::filesystem::path& path)
    {
        util::Log(color::moccasin, "album callback\n");
        std::vector<ListView::ItemType> songVec;

        if (client->connected())
        {
            // Client mode: `path` carries the albumId. Fetch its tracks from the
            // server and store each track's stream URL as the play target.
            const auto remoteTracks = client->albumTracks(path.string());

            // When every track of the album names the same artist, repeating it
            // on each row is noise - the album view already identifies it.
            const bool uniformArtist = not remoteTracks.empty()
                and not remoteTracks.front().artist.empty()
                and std::ranges::all_of(remoteTracks, [&](const RemoteTrack& t)
                                        { return t.artist == remoteTracks.front().artist; });

            for (const auto& t : remoteTracks)
            {
                // Untagged files come back with empty title/artist; fall back to
                // the file name so the row never collapses to a bare " - ".
                std::string name;
                if (not t.title.empty())
                    name = (t.artist.empty() or uniformArtist)
                         ? t.title
                         : std::format("{} - {}", t.artist, t.title);
                else if (not t.file_name.empty())
                    name = t.file_name;
                else
                    name = std::format("Track {}", t.id);

                std::string label = t.track_no
                    ? std::format("{:02} {}", *t.track_no, name)
                    : std::move(name);

                unsigned cols = (unsigned)ncstrwidth(label.c_str(), nullptr, nullptr);

                ListView::ItemType item;
                item.first    = cols;
                item.second   = t.stream_url; // avformat opens http:// directly
                item.display  = std::move(label);
                item.startSec = t.start_s.value_or(0);
                songVec.push_back(std::move(item));
            }
            // Keep server order (track order); no local sort.
        }
        else
        {
            for (const auto& file : std::filesystem::directory_iterator(path))
            {
                const auto ext = file.path().extension();
                if (ext == ".cue" || ext == ".jpg" || ext == ".png" || ext == ".m3u")
                    continue;

                unsigned cols = (unsigned)ncstrwidth(file.path().filename().c_str(), nullptr, nullptr);
                ListView::ItemType item;
                item.first  = cols;
                item.second = file.path();
                songVec.push_back(std::move(item));
            }

            // Sort the items alphabetically
            std::ranges::sort(songVec, [](const auto& a, const auto& b)
            {
                return a.second.string() < b.second.string();
            });
        }

        songViewRef.setItems(std::move(songVec));

        return true;
    });

    songViewRef.setEnterCallback([&](const ListView::ItemType& item)
    {
        util::Log(color::moccasin, "song callback\n");
        auto starter = [&cmdView, audio_path = item.second, title = item.display,
                        startSec = item.startSec](std::stop_token tkn)
        {
            try
            {
                AudioLoop loop{ audio_path, title, startSec };
                loop.consumer_loop(tkn);
            }
            catch (const std::runtime_error& e)
            {
                cmdView.ReportError(e.what());
                util::Log(color::red, "Runtime error: {}", e.what());
            }
            catch (const std::exception& e)
            {
                cmdView.ReportError(e.what());
                util::Log(color::red, "Exception: ", e.what());
            }
            catch (...)
            {
                cmdView.ReportError("Unhandled exception");
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

    m_client = std::make_shared<ServerClient>();

    auto cmdProcessor = MakeCommandProcessor(albumView, songView, m_client);

    cfg = std::make_shared<Config>( util::GetUserConfigDir() / "tMus.ini", cmdProcessor );

    const auto cmdView{ std::make_shared<CommandView>(std::move(commandPlane), cmdProcessor) };

    m_views[0] = cmdView;
    m_views[1] = albumView;
    m_views[2] = songView;

    auto& [cmdViewRef, albumViewRef, songViewRef] = m_views;

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
                   *std::get<std::shared_ptr<ListView>>(songViewRef),
                   *std::get<std::shared_ptr<CommandView>>(cmdViewRef),
                   m_client);
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

        // notcurses' enhanced (kitty) keyboard protocol reports the bare keycap
        // in id/utf8 (';' for Shift+';'), with Shift as a separate modifier bit.
        // eff_text is the codepoint *after* modifiers are applied (':'), which is
        // what we actually want for command detection and typing. Fall back to id.
        const std::uint32_t eff = ni.eff_text[0] ? ni.eff_text[0] : ni.id;

        util::Log(color::yellow, "--- id:{} utf8:'{}' eff:{} mods:{:#x} ---\n",
                  input, ni.utf8, eff, ni.modifiers);

        if (const auto& cmdView = std::get<std::shared_ptr<CommandView>>(m_views[0]);
            eff == ':' || eff == '/' || cmdView->isFocused())
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
