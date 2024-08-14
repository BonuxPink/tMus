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

#include "util.hpp"
#include <expected>

namespace fs = std::filesystem;

static std::expected<std::string_view, bool> GetEnv(std::string_view ENV)
{
    auto ret = getenv(ENV.data());
    if (ret != nullptr)
        return { ret };

    return std::unexpected(false);
}

fs::path util::GetUserConfigDir()
{
    if (auto XDG_CONFIG_DIR = GetEnv("XDG_CONFIG_HOME"); not XDG_CONFIG_DIR.has_value())
    {
        if (auto HOME = GetEnv("HOME"); not HOME.has_value())
        {
            throw std::runtime_error("Failed to get user's $HOME variable");
        }
        else
        {
            auto ConfigPath = fs::path(HOME.value()) / ".config" / "tMus";
            if (not fs::exists(ConfigPath))
            {
                if (not fs::create_directory(ConfigPath))
                {
                    throw std::runtime_error(std::format("Failed to create user directory: {}",
                                                            ConfigPath.string()));
                }
            }

            return ConfigPath;
        }
    }
    else
    {
        auto ConfigPath = fs::path(XDG_CONFIG_DIR.value()) / "tMus";
        if (not fs::exists(ConfigPath))
        {
            if (not fs::create_directory(ConfigPath))
            {
                throw std::runtime_error(std::format("Failed to create user directory: {}",
                                                        ConfigPath.string()));
            }
        }

        return ConfigPath;
    }
}
