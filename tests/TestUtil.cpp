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

#include "ut.hpp"
#include "util.hpp"
#include <filesystem>
#include <print>

using namespace boost::ut;

namespace fs = std::filesystem;

int main()
{
    "GetUserConfigDir"_test = []
    {
        expect (getenv("HOME") != nullptr or getenv("XDG_CONFIG_HOME") != nullptr);
        auto dir = util::GetUserConfigDir();

        expect (fs::exists(dir));
    };
}
