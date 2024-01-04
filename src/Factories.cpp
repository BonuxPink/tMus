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

#include "Factories.hpp"
#include "Colors.hpp"
#include "CommandProcessor.hpp"
#include "globals.hpp"

#include <memory>
#include <ncpp/Root.hh>

extern "C"
{
    #include <libavutil/common.h>
}

std::tuple<ncpp::Plane, ncpp::Plane, ncpp::Plane> MakePlanes()
{
    const auto stdPlane{ *ncpp::NotCurses::get_instance().get_stdplane() };

    ncplane_options albOpts
    {
        .y = 0, .x = 0,
        .rows = stdPlane.get_dim_y() - 2,
        .cols = stdPlane.get_dim_x() * 40 / 100,
        .userptr = nullptr, .name = nullptr,
        .resizecb = nullptr,
        .flags = 0, .margin_b = 0, .margin_r = 0,
    };

    ncplane_options songOpts
    {
        .y = 0, .x = static_cast<int>(albOpts.cols),
        .rows = albOpts.rows,
        .cols = stdPlane.get_dim_x() - albOpts.cols,
        .userptr = nullptr, .name = nullptr, .resizecb = nullptr,
        .flags = 0, .margin_b = 0, .margin_r = 0,
    };

    ncplane_options commandOpts
    {
        .y = static_cast<int>(ncplane_dim_y(stdPlane) - 1),
        .x = 0,
        .rows = 1,
        .cols = stdPlane.get_dim_x(),
        .userptr = nullptr, .name = nullptr,
        .resizecb = nullptr,
        .flags = 0, .margin_b = 0, .margin_r = 0,
    };

    ncpp::Plane albumPlane{ stdPlane, albOpts, stdPlane.get_notcurses_cpp() };
    albumPlane.set_base("", 0, Colors::DefaultBackground);

    ncpp::Plane songPlane{ stdPlane, songOpts, stdPlane.get_notcurses_cpp() };
    songPlane.set_base("", 0, Colors::DefaultBackground);

    ncpp::Plane commandPlane{ stdPlane, commandOpts, stdPlane.get_notcurses_cpp() };
    commandPlane.set_base("", 0, Colors::DefaultBackground);

    return std::make_tuple(albumPlane, songPlane, commandPlane);
}

CommandProcessor MakeCommandProcessor(ListView& albumView, ListView& songView) noexcept
{
    CommandProcessor com;
    com.registerCommand("/", std::make_shared<SearchCommand>(albumView, songView)); // Special command
    com.registerCommand("add", std::make_shared<AddCommand>(&albumView));
    com.registerCommand("hello", std::make_shared<HelloWorld>());
    com.registerCommand("clear", std::make_shared<Clear>(&albumView, &songView));

    return com;
}

std::unique_ptr<SDL_AudioSpec> MakeWantedSpec(int nb_channels)
{
    auto wantedSpec = std::make_unique<SDL_AudioSpec>();

    wantedSpec->channels = static_cast<unsigned char>(nb_channels);
    wantedSpec->freq = 48'000;

    if (wantedSpec->channels <= 0)
    {
        throw std::runtime_error(fmt::format("Invalid channel layout {}", wantedSpec->channels));
    }

    wantedSpec->format = AUDIO_S16SYS;
    wantedSpec->silence = 0;
    wantedSpec->samples = 2048;
    wantedSpec->callback = nullptr;
    wantedSpec->userdata = nullptr;

    util::Log("wanted samples: {}, wanted freq: {}\n", wantedSpec->samples, wantedSpec->freq);

    return wantedSpec;
}

void MakeStatusPlane() noexcept
{
    const auto stdPlane = ncpp::NotCurses::get_instance().get_stdplane();

    ncplane_options statusOpts
    {
        .y = static_cast<int>(stdPlane->get_dim_y() - 2),
        .x = 0,
        .rows = 1,
        .cols = stdPlane->get_dim_x(),
        .userptr = nullptr, .name = nullptr,
        .resizecb = nullptr,
        .flags = 0, .margin_b = 0, .margin_r = 0,
    };

    Globals::statusPlane = std::make_unique<ncpp::Plane>(*stdPlane, statusOpts, stdPlane->get_notcurses_cpp());
    Globals::statusPlane->set_base("", 0, Colors::DefaultBackground);
}
