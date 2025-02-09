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

#include <fstream>
#include <filesystem>

#include <notcurses/notcurses.h>

#include "CommandProcessor.hpp"
#include "IniParser.hpp"

struct Config
{
    Config() = delete;
    ~Config() = default;

    Config(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(const Config&) = delete;
    Config& operator=(Config&&) = delete;

    explicit Config(std::filesystem::path ConfigPath, std::shared_ptr<CommandProcessor>);

    [[nodiscard]] bool ProcessKeybinding(ncinput ni);

    [[nodiscard]] int GetVolume() noexcept
    { return m_audioSection["volume"]; }

private:
    void ParseConfig();

    struct Keybind
    {
        std::uint32_t key;
        std::shared_ptr<Command> cmd;
    };

    std::shared_ptr<CommandProcessor> m_cmdProc;

    std::vector<Keybind> m_keybindingsSection;
    std::unordered_map<std::string, int> m_audioSection;
    std::ifstream m_configFile;
};
