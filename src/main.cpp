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
#include "CustomViews.hpp"
#include "Decoder.hpp"
#include "Factories.hpp"
#include "Frame.hpp"
#include "globals.hpp"
#include "LoopComponent.hpp"
#include "util.hpp"
#include "Colors.hpp"

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

int main()
{
    int fd = open("/tmp/log.txt", O_CREAT | O_APPEND | O_WRONLY, 0644);
    util::Log<void>::setFd(fd);

    if (setlocale(LC_ALL, "en_US.utf8") == nullptr)
    {
        return EXIT_FAILURE;
    }

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

    av_log_set_level(AV_LOG_WARNING);
    av_log_set_callback(cb);

    {
        if (!SDL_getenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE"))
            SDL_setenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE", "1", 1);

        int flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_AUDIO;
        if (SDL_Init(flags))
        {
            util::Log("{}\n", SDL_GetError());
            return EXIT_FAILURE;
        }
    }

    auto stdPlane = StdPlane::getInstance().getStdPlane();
    stdPlane.set_base("", 0, Colors::DefaultBackground);
    auto [albumPlane, songPlane, commandPlane] = MakePlanes(stdPlane);

    auto albumViewFocus = std::make_shared<Control>();
    auto songViewFocus = std::make_shared<Control>();
    auto manager = std::make_shared<ControlManager>(albumViewFocus, songViewFocus);

    std::vector<LoopCompontent::ViewLike> views;
    views.push_back( CommandView { commandPlane });
    views.push_back( ListView    { albumPlane, albumViewFocus });
    views.push_back( ListView    { songPlane, songViewFocus });

    auto& cmdViewRef = std::get<CommandView>(views[0]);
    auto& albumViewRef = std::get<ListView>(views[1]);
    auto& songViewRef = std::get<ListView>(views[2]);

    albumViewFocus->setNotify([&](){ util::Log("+++\n"); albumViewRef.ColorSelected(); StdPlane::getInstance().getNotCurses().render(); });
    songViewFocus->setNotify([&](){ util::Log("----\n"); songViewRef.ColorSelected(); StdPlane::getInstance().getNotCurses().render(); });

    albumViewRef.setSelectCallback([&songViewRef](const std::filesystem::path& path)
    {
        std::vector<ListView::ItemType> songVec;
        for (const auto& file : std::filesystem::directory_iterator(path))
        {
            auto ext = file.path().extension();
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
        auto starter = [&, p = path](std::stop_token tkn)
        {
            auto packetQueue = std::make_shared<PacketQueue>();
            auto frameQueue = std::make_shared<FrameQueue>(&packetQueue->abort_request);

            AudioLoop state{ p, packetQueue, frameQueue };

            std::shared_ptr<AVCodecContext> avctx;
            try
            {
                avctx = state.streamOpen();
            }
            catch (const std::runtime_error& e)
            {
                util::Log("Runtime: {}\n", e.what());
                throw;
            }

            int sampleRate = avctx->sample_rate;
            AVChannelLayout layout{};
            int ret = av_channel_layout_copy(&layout, &avctx->ch_layout);
            if (ret < 0)
            {
                throw std::runtime_error("Failed to copy layout with ret");
            }

            util::Log(fg(fmt::color::teal), "sample rate: {}, ret {}\n", sampleRate, ret);
            AudioDevice device{ MakeWantedSpec(&state, &layout, sampleRate), &layout };
            state.audioSrc = device.getParams();
            state.audioTgt = device.getParams();

            Decoder dec{ avctx, packetQueue, state.getCv() };

            auto StatusThread = [&](std::stop_token statusTok)
            {
                StatusView statusView{ MakeStatusPlane(stdPlane), state.getFormatCtx(), frameQueue, avctx };
                using namespace std::chrono_literals;

                while (statusTok.stop_requested() != true && Globals::abort_request != true)
                {
                    statusView.draw();
                    std::this_thread::sleep_for(1s);
                }
            };

            std::jthread statusTh = std::jthread{ StatusThread };
            pthread_setname_np(statusTh.native_handle(), "statusThread");

            try
            {
                dec.start(frameQueue.get());
                state.loop(tkn);
                dec.abort(*frameQueue.get());
            }
            catch (const std::invalid_argument& e)
            {
                util::Log(fg(fmt::color::red), "Invalid: {}\n", e.what());
            }
            catch (...)
            {
                util::Log(fg(fmt::color::red), "Catch All\n");
            }
        };

        if (playbackThread.joinable())
        {
            SDL_PauseAudioDevice(2, 1);
            playbackThread.request_stop();
            util::Log(fg(fmt::color::yellow), "Requested stop, now waiting\n");
            playbackThread.join();
        }

        playbackThread = std::jthread{ starter };

        return true;
    });

    auto cmdProcessor = MakeCommandProcessor(albumViewRef, songViewRef);
    cmdViewRef.init(&cmdProcessor);

    LoopCompontent loop{ views };
    loop.loop(StdPlane::getInstance().getNotCurses());

    if (playbackThread.joinable())
    {
        playbackThread.join();
    }

    util::Log(fg(fmt::color::green), "Program exiting\n");
    return EXIT_SUCCESS;
}
