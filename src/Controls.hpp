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

#include <atomic>
#include <bitset>
#include <string>

struct Event
{
    enum class Action
    {
        UP_VOLUME = 0,
        DOWN_VOLUME,
        SEEK_FORWARDS,
        SEEK_BACKWARDS,
        PAUSE,
    };

    void SetEvent(Action in) noexcept
    {
        act = in;
        m_EventHappened = true;
    }

    std::atomic<bool> m_EventHappened;
    std::atomic<Action> act;
};

struct Completion
{
    std::string str;
    unsigned int index{};

    void clear()
    {
        str.clear();
        index = 0;
    }
};
