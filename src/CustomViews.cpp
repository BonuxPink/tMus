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

#include "globals.hpp"
#include "util.hpp"
#include "CustomViews.hpp"

#include <filesystem>
#include <stdexcept>

#include <notcurses/nckeys.h>
#include <notcurses/notcurses.h>

#include <ncpp/Utilities.hh>
#include <ncpp/Widget.hh>
#include <utility>

ListView::ListView(ncpp::Plane&& plane, std::shared_ptr<Focus> focus)
    : ncpp::Widget(ncpp::NotCurses::get_instance().get_notcurses_cpp())
    , m_ncp(std::move(plane))
    , m_Focus(std::move(focus))
{
    util::Log("Has focus: {}\n", m_Focus->m_hasFocus);
    ncpp::Widget::ensure_valid_plane(m_ncp);

    util::Log(fg(fmt::color::green), "CALLBACKS: {} {}\n", (bool)m_enterCallback, (bool)m_selectionCallback);
}

void ListView::draw()
{
    unsigned dimy = 0, dimx = 0;
    m_ncp.get_dim(dimy, dimx);
    static int draw = 0;
    util::Log(fg(fmt::color::teal), "Draw---------------------------{}\n", draw++);
    m_ncp.get_dim(dimy, dimx);

    m_ncp.cursor_move(1, 1);

    for (unsigned i = 1; i < dimy; ++i)
    {
        ncpp::Cell tmp{ std::uint32_t(0) };
        m_ncp.hline(tmp, dimx - 2);

        m_ncp.cursor_move(static_cast<int>(i), 1);
    }

    m_ncp.cursor_move(1, 1);

    unsigned printidx = m_startdisp;
    for (int yoff = 1; yoff < static_cast<int>(dimy); ++yoff)
    {
        if (!m_ncp.cursor_move(yoff, static_cast<int>(dimx) - 1))
        {
            util::Log(fg(fmt::color::yellow), "Failed to move cursor y:{}:{} x:{}:{}\n", yoff, dimy, dimx, dimx);
            return;
        }

        for (unsigned i = dimx + 1; i < dimx - 1; ++i)
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

        if (static_cast<unsigned int>(yoff) == dimy - 2)
        {
            break;
        }

        ++printidx;
    }
}

bool ListView::handle_input(const ncinput& input) noexcept
{
    if (!hasFocus())
    {
        util::Log("Not has focus\n");
        return false;
    }

    util::Log("Has focus\n");

    if (input.id == 'q')
    {
        Globals::stop_request = true;
        return true;
    }

    // if (input.id == NCKEY_UP)
    // {
    //     SelectPrevItem();
    //     return true;
    // }

    else if (input.id == NCKEY_DOWN)
    {
        SelectNextItem();
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

void ListView::SelectNextItem()
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

void ListView::SelectPrevItem()
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
    m_items = std::move(items);
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

void ListView::ColorSelected() const
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

void ListView::setSelectCallback(const selectFunc& f)
{
    m_selectionCallback = f;
}

void ListView::setEnterCallback(const enterFunc& f)
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

    ncplane_cursor_move_yx(ncp.to_ncplane(), yoff, 2);

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
