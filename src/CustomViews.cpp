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

#include "Factories.hpp"
#include "Renderer.hpp"
#include "globals.hpp"
#include "Colors.hpp"
#include "util.hpp"
#include "CustomViews.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <filesystem>
#include <ranges>
#include <stdexcept>
#include <string_view>

#include <notcurses/nckeys.h>
#include <notcurses/notcurses.h>

#include <ncpp/Utilities.hh>
#include <ncpp/Widget.hh>

ListView::ListView(ncpp::Plane& plane, std::shared_ptr<Control> focus)
    : ncpp::Widget(ncpp::Utilities::get_notcurses_cpp(plane))
    , m_ncp(plane)
    , m_Focus(std::move(focus))
    , m_itemcount(m_items.size())
{
    ncpp::Widget::ensure_valid_plane(plane);
    ncpp::Widget::take_plane_ownership(plane);
}

void ListView::draw()
{
    m_ncp.get_dim(m_dimy, m_dimx);
    static int draw = 0;
    util::Log(fg(fmt::color::teal), "Draw---------------------------{}\n", draw++);
    m_ncp.get_dim(m_dimy, m_dimx);

    m_ncp.cursor_move(1, 1);

    for (unsigned i = 1; i < m_dimy; ++i)
    {
        ncpp::Cell tmp{ std::uint32_t(0) };
        m_ncp.hline(tmp, m_dimx - 2);

        m_ncp.cursor_move(static_cast<int>(i), 1);
    }

    m_ncp.cursor_move(1, 1);

    unsigned printidx = m_startdisp;
    for (int yoff = 1; yoff < static_cast<int>(m_dimy); ++yoff)
    {
        if (!m_ncp.cursor_move(yoff, static_cast<int>(m_dimx) - 1))
        {
            util::Log(fg(fmt::color::yellow), "Failed to move cursor y:{}:{} x:{}:{}\n", yoff, m_dimy, m_dimx, m_dimx);
            return;
        }

        for (unsigned i = m_dimx + 1; i < m_dimx - 1; ++i)
        {
            ncpp::Cell trancs{};
            m_ncp.putc(trancs);
        }

        if (auto size = m_items.size(); size > printidx)
        {
            try
            {
                if (printidx == m_selected && m_Focus->hasFocus())
                {
                    util::Log("Coloring\n");
                    ColorSelected();
                }
                else
                {
                    m_ncp.set_channels(0);
                    auto item = m_items.at(printidx);
                    std::invoke( PrintLine{}, m_ncp, item, yoff );
                }
            }
            catch (std::out_of_range& e)
            {
                util::Log("Ex: {} idx: {} size: {} selected: {}\n", e.what(), printidx, size, m_selected);
                return;
            }
            catch (...)
            {
                util::Log("Uncaught exception\n");
                return;
            }

        }

        if (static_cast<unsigned int>(yoff) == m_dimy - 2)
        {
            break;
        }

        ++printidx;
    }
}

bool ListView::handle_input(const ncinput& input) noexcept
{
    if (input.id == 'q')
    {
        Globals::stop_request = true;
        return true;
    }

    if (m_itemcount <= 0)
        return false;

    if (input.id == NCKEY_UP)
    {
        SelectPrevitem();
        return true;
    }

    else if (input.id == NCKEY_DOWN)
    {
        SelectNextitem();
        return true;
    }

    else if (input.id == NCKEY_RIGHT)
    {
        Globals::event.SetEvent(Event::Action::SEEK_FORWARDS);
        return true;
    }

    else if (input.id == NCKEY_LEFT)
    {
        Globals::event.SetEvent(Event::Action::SEEK_BACKWARDS);
        return true;
    }

    else if (input.id == NCKEY_TAB)
    {
        if (!m_Focus->hasFocus())
        {
            m_Focus->toggle();
        }
        else
        {
            util::Log("Unfocusing song view\n");
            m_Focus->toggle();
        }
        return true;
    }

    else if (input.id == NCKEY_SPACE)
    {
        Globals::event.SetEvent(Event::Action::PAUSE);
        return true;
    }

    else if (input.utf8[0] == '+')
    {
        Globals::event.SetEvent(Event::Action::UP_VOLUME);
        return true;
    }

    else if (input.utf8[0] == '-')
    {
        Globals::event.SetEvent(Event::Action::DOWN_VOLUME);
        return true;
    }

    else if (input.id == NCKEY_ENTER)
    {
        return EnterCallback();
    }

    return false;
}

void ListView::SelectNextitem()
{
    if (m_itemcount == 0 || m_selected == m_itemcount - 1)
    {
        util::Log("Ret {} {} {}\n", m_itemcount, m_selected, m_itemcount - 1);
        return;
    }

    unsigned dimy, dimx;
    m_ncp.get_dim(dimy, dimx);

    const auto totalElements{ dimy - 2 };
    const auto thirdFromBottom{ m_startdisp + totalElements - 3 };
    const bool wouldOverflow{ m_startdisp + totalElements == m_itemcount };

    if (m_selected == thirdFromBottom && !wouldOverflow)
    {
        ++m_startdisp;
    }

    ++m_selected;

    SelectionCallback();
    draw();
}

void ListView::SelectPrevitem()
{
    if (m_itemcount == 0 || m_selected == 0)
    {
        util::Log("Ret {} {} {}\n", m_itemcount, m_selected, m_itemcount - 1);
        return;
    }

    unsigned dimy{}, dimx{};
    m_ncp.get_dim(dimy, dimx);

    if (m_startdisp > 0 && m_selected - m_startdisp < 3)
    {
        --m_startdisp;
    }

    --m_selected;

    SelectionCallback();
    draw();
}

void ListView::SelectionCallback()
{
    if (m_selectionCallback)
    {
        try
        {
            auto sec = m_items.at(m_selected).second;
            m_selectionCallback(sec);
        }
        catch (const std::bad_function_call& e)
        {
            util::Log("function: {}\n", e.what());
        }
        catch (const std::out_of_range& e)
        {
            util::Log("Out of range: {}\n", e.what());
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            util::Log("Filesystem: {}\n", e.what());
        }
        catch(...)
        {
            util::Log("... exception... What could it be?\n");
        }
    }
}

bool ListView::EnterCallback()
{
    if (m_enterCallback)
    {
        try
        {
            auto sec = m_items.at(m_selected).second;
            m_enterCallback(sec);
            return true;
        }
        catch (const std::runtime_error& e)
        {
            util::Log("runtime: {}\n", e.what());
        }
        catch (const std::bad_function_call& e)
        {
            util::Log("function: {}\n", e.what());
        }
        catch (const std::out_of_range& e)
        {
            util::Log("Out of range: {}\n", e.what());
        }
        catch(...)
        {
            util::Log("... exception... What could it be?\n");
        }
    }

    return false;
}

void ListView::Reset() noexcept
{
    m_itemcount = m_items.size();
    m_selected = 0;
    m_startdisp = 0;

    for (const auto& item : m_items)
    {
        if (item.first > m_longop)
            m_longop = item.first;
    }
}

void ListView::Clear() noexcept
{
    m_selected = 0;
    m_itemcount = 0;
    m_startdisp = 0;
    m_longop = 0;

    m_items.clear();
}

void ListView::PrintItems()
{
    for (auto& a : m_items)
    {
        util::Log("{}\n", a.second.string());
    }
}

void ListView::setItems(ItemContainer items) noexcept
{
    m_items = std::forward<ItemContainer>(items);
    m_itemcount = m_items.size();
    m_selected = 0;
    m_startdisp = 0;

    for (const auto& item : m_items)
    {
        if (item.first > m_longop)
            m_longop = item.first;
    }

    draw();
}

void ListView::ColorSelected() const noexcept
{
    if (m_Focus->hasFocus())
    {
        auto curCh = m_ncp.get_channels();

        ncchannels_set_bg_alpha(&curCh, NCALPHA_BLEND);
        m_ncp.set_channels(curCh);
    }
    else
    {
        m_ncp.set_channels(0);
    }

    auto item = m_items.at(m_selected);
    auto yoff = m_selected + 1 - m_startdisp;
    std::invoke( PrintLine{}, m_ncp, item, yoff );
}

void ListView::setSelectCallback(const selectFunc& f) noexcept
{
    m_selectionCallback = f;
}

void ListView::setEnterCallback(const enterFunc& f) noexcept
{
    m_enterCallback = f;
}

void ListView::selectLibrary(std::filesystem::path& album)
{
    int index = 0;
    for (const auto& dir : m_items)
    {
        if (dir.second.compare(album) == 0)
        {
            m_selected = index;
            m_startdisp = index - 5;
            SelectionCallback();
        }
        index++;
    }
}

void ListView::selectSong(std::filesystem::path& theSong)
{
    int index = 0;
    auto copyFile = theSong;

    for (const auto& song : std::filesystem::directory_iterator(theSong.remove_filename()))
    {
        if (song == copyFile)
        {
            m_selected = index;
            break;
        }
        index++;
    }
}

void PrintLine::operator()(const ncpp::Plane& ncp, const ListView::ItemType& item, int yoff) const noexcept
{
    auto dimX = ncp.get_dim_x();
    std::size_t sizeInBytes{ 0 };
    auto str = item.second.filename().string();
    const auto* cStr = str.c_str();

    if (ncplane_cursor_move_yx(ncp.to_ncplane(), yoff, 2))
    {
        util::Log(fg(fmt::color::red), "Meow\n");
        std::terminate();
    }

    while (*cStr)
    {
        int cols = ncplane_putegc_yx(ncp.to_ncplane(), -1, -1, cStr, &sizeInBytes);
        cStr += sizeInBytes;

        if (cols < 0)
            break;

        if (sizeInBytes == 0)
            break;

        unsigned X = ncp.cursor_x();
        if (X >= dimX - 3)
            break;
    }

    if (item.first > dimX - 4)
    {
        ncp.putc("…");
    }
}

bool customSelectCallback(ListView& albumView, std::filesystem::path&& path)
{
    util::Log("Select callback\n");
    std::vector<ListView::ItemType> songVec;
    for (const auto& file : std::filesystem::directory_iterator(path))
    {
        if (!std::filesystem::is_regular_file(file))
            continue;

        auto ext = file.path().extension();
        if (ext == ".cue" || ext == ".jpg" || ext == ".png")
            continue;

        unsigned cols = (unsigned)ncstrwidth(file.path().filename().c_str(), nullptr, nullptr);
        songVec.push_back({ cols, file.path() });
    }

    albumView.Reset();
    albumView.draw();


    return true;
}
