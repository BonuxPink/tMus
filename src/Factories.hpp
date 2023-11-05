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

std::tuple<ncpp::Plane, ncpp::Plane, ncpp::Plane>
MakePlanes(ncpp::Plane& stdPlane);

ncpp::Plane MakeStatusPlane() noexcept;

CommandProcessor MakeCommandProcessor(ListView&, ListView&) noexcept;

using MakeViewFunc = std::function<ListView::ItemContainer(std::filesystem::path&&)>;
ListView MakeView(ncpp::Plane&, MakeViewFunc);

std::unique_ptr<SDL_AudioSpec> MakeWantedSpec(int nb_channels);
