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

#pragma once

#include "FocusManager.hpp"

#include <memory>
#include <string>
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
    // A list row. `first`/`second` keep their historical meaning so existing
    // call sites are untouched:
    //   first   - display width in columns
    //   second  - the target: a local filesystem path, or (in client mode) the
    //             albumId / stream URL handed to the server / AudioLoop
    //   display - label shown to the user; if empty, second.filename() is used
    //             (so local browsing keeps showing file names for free).
    struct ItemType
    {
        unsigned first {};
        std::filesystem::path second {};
        std::string display {};

        // Member-wise lexicographic ordering (width, path, label). Provides the
        // full set of comparisons std::ranges::sort needs (totally_ordered),
        // matching the previous std::pair behaviour for the first two members.
        friend auto operator<=>(const ItemType&, const ItemType&) = default;
        friend bool operator==(const ItemType&, const ItemType&)  = default;
    };
    using ItemContainer = std::vector<ItemType>;

    using enterFunc = std::function<bool(const std::filesystem::path&)>;
    using selectFunc = std::function<bool(const std::filesystem::path&)>;

    ListView(ncpp::Plane&&, std::shared_ptr<Focus>);

    [[nodiscard]] bool hasFocus() const { return m_Focus->hasFocus(); }
    [[nodiscard]] Focus* getFocus() const { return m_Focus.get(); }

    void draw();

    void SelectionCallback();
    bool EnterCallback();
    void SelectNextItem();
    void SelectPrevItem();
    void Clear() noexcept;

    void setItems(ItemContainer) noexcept;
    void setSelectCallback(const selectFunc&);
    void setEnterCallback(const enterFunc& f);

    void ColorSelected() const;

    void selectLibrary(std::filesystem::path&);
    void selectSong(std::filesystem::path&);

    void toggleFocus() noexcept { m_Focus->toggle(); }
    [[nodiscard]] ItemContainer& getItems() noexcept { return m_items; }

    auto* getSelection() { return &m_selectionCallback; }
private:
    friend class PrintLine;

    void PrintItems();

    ncpp::Plane m_ncp;
    ItemContainer m_items{};

    std::shared_ptr<Focus> m_Focus{};

    std::size_t m_itemcount{};


    selectFunc m_selectionCallback;                                                          // Called everytime a sellection change has been made
    enterFunc m_enterCallback;                                                               // Called when enter is pressed

    unsigned m_selected{};                                                                     // index of selection
    unsigned m_longop{};                                                                       // columns occupied by longest option
    unsigned m_startdisp{};                                                                    // At which element to start the display
};

class PrintLine
{
public:
    void operator()(const ncpp::Plane&, const ListView::ItemType&, int) const noexcept;
};
