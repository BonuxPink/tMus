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

#include "Config.hpp"
#include "util.hpp"

#include <string>

namespace fs = std::filesystem;

constinit std::string_view str{ "/tmp/" };

Config::Config(fs::path ConfigPath, std::shared_ptr<CommandProcessor> cmdproc)
    : m_cmdProc{ std::move(cmdproc) }
    , m_configFile{ ConfigPath }
{
    ParseConfig();
}

void Config::ParseConfig()
{
    std::stringstream ss;
    ss << m_configFile.rdbuf();

    IniParser parser(ss);

    for (const auto& [keybind, function] : parser["Keybindings"])
    {
        std::uint32_t key = 0;

        // If it's not a single letter
        if (keybind.size() > 1)
        {
            if (keybind == "up")
            {
                key = NCKEY_UP;
            }
            else if (keybind == "down")
            {
                key = NCKEY_DOWN;
            }
            else if (keybind == "tab")
            {
                key = NCKEY_TAB;
            }
            else if (keybind == "right")
            {
                key = NCKEY_RIGHT;
            }
            else if (keybind == "left")
            {
                key = NCKEY_LEFT;
            }
            else if (keybind == "space" || keybind == "SPC")
            {
                key = NCKEY_SPACE;
            }
            else if (keybind == "RET" || keybind == "RETURN" || keybind == "ENTER")
            {
                key = NCKEY_ENTER;
            }
        }
        else
        {
            key = static_cast<std::uint32_t>(static_cast<unsigned char>(keybind[0]));
        }

        if (key == 0)
        {
            throw std::runtime_error(std::format("Failed to parse '{}' from config", keybind));
        }

        auto commandName = function.as<std::string>();
        auto opt = m_cmdProc->findCommand(commandName);
        if (opt == nullptr)
        {
            throw std::runtime_error(std::format("Failed to find command '{}' in command list", commandName));
        }

        m_keybindingsSection.push_back({ key, std::move(opt) });
    }

    for (const auto& [key, value] : parser["Audio"])
    {
        m_audioSection[key] = value.as<int>();
    }
}

bool Config::ProcessKeybinding(ncinput ni)
{
    auto id = ni.id;
    util::Log("ID: {}, ch: {}\n", id, (char)id);
    for (const auto& elem : m_keybindingsSection)
    {
        if (elem.key == id)
        {
            elem.cmd->execute("");
            return true;
        }
    }

    return false;
}
