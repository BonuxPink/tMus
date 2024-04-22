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

#pragma once

#include "FocusManager.hpp"

#include <memory>
#include <vector>
#include <filesystem>
#include <functional>

#include <ncpp/NotCurses.hh>
#include <ncpp/Plane.hh>
#include <ncpp/Widget.hh>

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

class ListView;

class ListView : public ncpp::Widget
{
public:
    // Real length and path
    using ItemType = std::pair<unsigned, std::filesystem::path>;
    using ItemContainer = std::vector<ItemType>;

    using enterFunc = std::function<bool(const std::filesystem::path&)>;
    using selectFunc = std::function<bool(const std::filesystem::path&)>;

    ListView(ncpp::Plane&, std::shared_ptr<Focus>);

    [[nodiscard]] bool hasFocus() const { return m_Focus->hasFocus(); }
    [[nodiscard]] Focus* getFocus() const { return m_Focus.get(); }

    void draw();
    bool handle_input(const ncinput&) noexcept;

    void SelectionCallback();
    bool EnterCallback();
    void SelectNextItem();
    void SelectPrevItem();
    void Reset() noexcept;
    void Clear() noexcept;

    void setItems(ItemContainer) noexcept;
    void setSelectCallback(const selectFunc&) noexcept;
    void setEnterCallback(const enterFunc& f) noexcept;

    void ColorSelected() const noexcept;

    void selectLibrary(std::filesystem::path&);
    void selectSong(std::filesystem::path&);

    [[nodiscard]] ItemContainer& getItems() noexcept { return m_items; }
private:
    friend class PrintLine;

    void PrintItems();

    ncpp::Plane m_ncp;
    ItemContainer m_items{};

    std::shared_ptr<Focus> m_Focus{};

    std::size_t m_itemcount{};


    selectFunc m_selectionCallback{};                                                          // Called everytime a sellection change has been made
    enterFunc m_enterCallback{};                                                               // Called when enter is pressed

    unsigned m_selected{};                                                                     // index of selection
    unsigned m_longop{};                                                                       // columns occupied by longest option
    unsigned m_startdisp{};                                                                    // At which element to start the display
};

class PrintLine
{
public:
    void operator()(const ncpp::Plane&, const ListView::ItemType&, int) const noexcept;
};

bool customSelectCallback(ListView&, ListView::ItemContainer, std::filesystem::path&&);
bool customEnterCallback(std::filesystem::path&&);
