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

#pragma once

#include "AudioLoop.hpp"
#include "CommandProcessor.hpp"
#include "CustomViews.hpp"

#include <SDL2/SDL_audio.h>

#include <memory>
#include <functional>

#include <notcurses/notcurses.h>
#include <ncpp/NotCurses.hh>

extern "C"
{
    #include <libavutil/channel_layout.h>
}

struct StdPlane
{
    static StdPlane& getInstance()
    {
        static StdPlane instance;
        return instance;
    }

    [[nodiscard]] auto getStdPlane() const { return stdPlane; }
    [[nodiscard]] auto& getNotCurses() { return nc; }

private:
    StdPlane()
        : nc{ opts }
        , stdPlane{ *nc.get_stdplane() }

    { }

    notcurses_options opts{ .termtype = nullptr,
                            .loglevel = NCLOGLEVEL_WARNING,
                            .margin_t = 0, .margin_r = 0,
                            .margin_b = 0, .margin_l = 0,
                            .flags = NCOPTION_SUPPRESS_BANNERS /* | NCOPTION_CLI_MODE */,
    };

    ncpp::NotCurses nc;
    ncpp::Plane stdPlane;
};

std::tuple<ncpp::Plane, ncpp::Plane, ncpp::Plane>
MakePlanes(ncpp::Plane& stdPlane);

ncpp::Plane MakeStatusPlane(ncpp::Plane stdPlane) noexcept;

CommandProcessor MakeCommandProcessor(ListView&, ListView&) noexcept;

using MakeViewFunc = std::function<ListView::ItemContainer(std::filesystem::path&&)>;
ListView MakeView(ncpp::Plane&, MakeViewFunc);

std::unique_ptr<SDL_AudioSpec> MakeWantedSpec(AudioLoop* state, AVChannelLayout* layout, int sampleRate);
