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

#include "CommandView.hpp"
#include "Colors.hpp"
#include "globals.hpp"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <ranges>

#include <notcurses/nckeys.h>
#include <notcurses/notcurses.h>

#include <ncpp/Utilities.hh>

CommandView::CommandView(ncpp::Plane& plane)
    : ncpp::Widget(ncpp::Utilities::get_notcurses_cpp(plane))
    , m_ncp(plane)
    , m_textColor(Colors::TextColor)
{
    ncpp::Widget::ensure_valid_plane(plane);
    ncpp::Widget::take_plane_ownership(plane);
}

void CommandView::init(CommandProcessor* proc) noexcept
{
    m_commandBuffer.push_back(':');
    m_cmdProc = proc;
}

void CommandView::search(std::string str)
{
    clearDraw();

    util::Log("Search: {}\n", str);

    const int y = static_cast<int>(m_ncp.get_abs_y());
    const int x = static_cast<int>(m_commandBuffer.size());
    enableCursor(y, x);

    if (!m_ncp.cursor_move(0, 0))
    {
        util::Log(fg(fmt::color::red), "Cursor false\n");

    }

    m_ncp.set_channels((std::uint64_t)ncchannels_bchannel(m_textColor) << 32u | ncchannels_fchannel(m_textColor));

    for (const auto& e : m_commandBuffer)
    {
        m_ncp.putc(static_cast<char>(e));
    }
}

void CommandView::draw()
{
    clearDraw();

    const int y = static_cast<int>(m_ncp.get_abs_y());
    const int x = static_cast<int>(m_commandBuffer.size());
    enableCursor(y, x);

    if (!m_ncp.cursor_move(0, 0))
    {
        util::Log(fg(fmt::color::red), "Cursor false\n");
    }

    m_ncp.set_channels((std::uint64_t)ncchannels_bchannel(m_textColor) << 32u | ncchannels_fchannel(m_textColor));

    for (const auto& e : m_commandBuffer)
    {
        m_ncp.putc(static_cast<char>(e));
    }
}

bool CommandView::handle_input(const ncinput& ni) noexcept
{
    if (m_focus)
    {
        util::Log("Has focus\n");
        switch (ni.id)
        {
        case 'U':
            if (ncinput_ctrl_p(&ni))
            {
                m_commandBuffer.clear();
                m_search ? m_commandBuffer.push_back('/') : m_commandBuffer.push_back(':');
            }
            break;
        case NCKEY_TAB:
            if (ni.modifiers & NCKEY_MOD_SHIFT)
            {
                // TODO: make reverse tab
            }
            else
            {
                CompleteNext();
            }
            break;
        case NCKEY_SPACE:
            m_commandBuffer.push_back(' ');
            break;
        case NCKEY_BACKSPACE:
            if (m_commandBuffer.size() > 1)
            {
                m_commandBuffer.pop_back();
            }
            break;
        case NCKEY_ESC:
            clearCommand();
            Globals::lastCompletion.clear();
            return true;
        case NCKEY_ENTER:
            ProcessCommand(u32vecToString(m_commandBuffer));
            Globals::lastCompletion.clear();
            return true;
            break;
        default:
            m_commandBuffer.emplace_back(ni.id);
            break;
        }

        if (ni.id != NCKEY_TAB)
        {
            Globals::lastCompletion.clear();
        }

        draw();
        return true;
    }
    else if (ni.utf8[0] == ':')
    {
        util::Log(fg(fmt::color::teal), "pressed ':'\n");
        m_focus = true;
        draw();
        return true;
    }

    else if (ni.utf8[0] == '/') // '/' is a special command
    {
        util::Log(fg(fmt::color::yellow), "/ pressed\n");
        m_focus = true;
        m_search = true;
        m_commandBuffer[0] = '/';
        draw();
        return true;
    }

    return false;
}

void CommandView::CompleteNext() noexcept
{
    auto Complete = m_cmdProc->completeNext(m_commandBuffer);
    if (Complete)
    {

    }
}

void CommandView::clearCommand() noexcept
{
    m_focus = false;
    m_commandBuffer.clear();
    clearDraw();
    m_commandBuffer.push_back(':');
}

void CommandView::clearDraw() noexcept
{
    m_ncp.get_dim(m_dimy, m_dimx);

    ncpp::Cell transchar{};
    m_ncp.set_channels((std::uint64_t)ncchannels_bchannel(m_textColor) << 32u | ncchannels_fchannel(m_textColor));

    m_ncp.cursor_move(0, 0);
    m_ncp.hline(transchar, m_dimx - 1);

    disableCursor();
}

void CommandView::disableCursor()
{
    if (m_CursorEnabled)
    {
        notcurses_cursor_disable(m_ncp.get_notcurses());
        m_CursorEnabled = false;
    }
}

void CommandView::enableCursor(int y, int x)
{
    if (!m_CursorEnabled)
    {
        notcurses_cursor_enable(m_ncp.get_notcurses(), y, x);
        m_CursorEnabled = true;
    }
}

void CommandView::toggleCursor() { m_CursorEnabled ? disableCursor() : enableCursor(); }

void CommandView::ProcessCommand(std::string str)
{
    if (str.empty())
    {
        ReportError("Command empty");
        return;
    }

    if (m_search)
    {
        auto b = m_cmdProc->getCommandByName("/");
        if (b.has_value())
        {
            b.value()->execute(str);
        }
    }
    else
    {
        auto b = m_cmdProc->processCommand(str);
        if (b.has_value())
        {
            ReportError(b.value());
            return;
        }
    }

    clearCommand();
}

void CommandView::ReportError(std::string_view error) noexcept
{
    auto red = NCCHANNELS_INITIALIZER(255, 0, 0, 0, 0, 0);

    m_ncp.cursor_move(-1, 0);
    m_ncp.set_channels((std::uint64_t)ncchannels_bchannel(red) << 32u | ncchannels_fchannel(red));

    disableCursor();

    auto str = fmt::format("Error: {}", error);
    for (const auto c : str)
    {
        m_ncp.putc(c);
    }

    m_focus = false;
    m_commandBuffer.clear();
    m_commandBuffer.push_back(':');
}

std::string CommandView::u32vecToString(const std::vector<std::uint32_t>& vec) const noexcept
{
    std::string str;

    for (const auto& e : vec | std::views::drop(1))
    {
        str.push_back(static_cast<char>(e));
    }

    return str;
}
