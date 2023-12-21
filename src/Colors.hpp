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

#include <cstdint>
#include <notcurses/notcurses.h>

namespace Colors
{
    inline constexpr auto DefaultBackground { NCCHANNELS_INITIALIZER(0, 0, 0, 204, 201, 195) };
    inline constexpr auto TextColor         { NCCHANNELS_INITIALIZER(0, 0, 0, 255, 255, 255) };
    inline constexpr auto StatusTextColor   { NCCHANNELS_INITIALIZER(255, 255, 255, 0, 0, 0)};
}
