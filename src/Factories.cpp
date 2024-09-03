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

#include <memory>
#include <ncpp/Root.hh>

extern "C"
{
    #include <libavutil/common.h>
}

std::tuple<ncpp::Plane, ncpp::Plane, ncpp::Plane> MakePlanes()
{
    auto stdPlanePtr = std::unique_ptr<ncpp::Plane>(ncpp::NotCurses::get_instance().get_stdplane());
    auto& stdPlane = *stdPlanePtr.get();

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

std::shared_ptr<CommandProcessor> MakeCommandProcessor(const std::shared_ptr<ListView>& albumViewPtr, const std::shared_ptr<ListView>& songViewPtr) noexcept
{
    auto com = std::make_shared<CommandProcessor>();
    com->registerCommand("/",          std::make_shared<SearchCommand>(albumViewPtr, songViewPtr)); // Special command
    com->registerCommand("add",        std::make_shared<Add>(albumViewPtr));
    com->registerCommand("hello",      std::make_shared<HelloWorld>());
    com->registerCommand("clear",      std::make_shared<Clear>(albumViewPtr, songViewPtr));
    com->registerCommand("bind",       std::make_shared<BindCommand>());
    com->registerCommand("up",         std::make_shared<Up>(albumViewPtr, songViewPtr));
    com->registerCommand("down",       std::make_shared<Down>(albumViewPtr, songViewPtr));
    com->registerCommand("right",      std::make_shared<Right>(albumViewPtr, songViewPtr));
    com->registerCommand("left",       std::make_shared<Left>(albumViewPtr, songViewPtr));
    com->registerCommand("quit",       std::make_shared<Quit>());
    com->registerCommand("cyclefocus", std::make_shared<CycleFocus>(albumViewPtr, songViewPtr));
    com->registerCommand("play",       std::make_shared<Play>(albumViewPtr, songViewPtr));
    com->registerCommand("pause",      std::make_shared<Pause>());
    com->registerCommand("volup",      std::make_shared<Volup>());
    com->registerCommand("voldown",    std::make_shared<Voldown>());

    return com;
}

std::unique_ptr<ncpp::Plane> MakeStatusPlane() noexcept
{
    const auto stdPlanePtr = std::unique_ptr<ncpp::Plane>(ncpp::NotCurses::get_instance().get_stdplane());
    const auto stdPlane = stdPlanePtr.get();

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

    auto statusPlane = std::make_unique<ncpp::Plane>(*stdPlane, statusOpts, stdPlane->get_notcurses_cpp());
    statusPlane->set_base("", 0, Colors::DefaultBackground);

    return statusPlane;
}
