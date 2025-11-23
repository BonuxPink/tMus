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

#include "IniParser.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>
#include <format>
#include <utility>

namespace rn = std::ranges;

namespace
{

    std::string_view TrimWhitespace(std::string_view str)
    {
        auto start = str.find_first_not_of(" \t\r\n");
        if (start == std::string_view::npos)
            return "";

        auto end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    SmartKey DetermineType(std::string_view value)
    {
        if (value.find('.') != std::string_view::npos)
        {
            float result{};
            auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
            if (ec == std::errc{})
            {
                return SmartKey{ result };
            }
        }
        else
        {
            int result{};
            auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
            if (ec == std::errc{} and ptr == value.data() + value.size())
                return SmartKey{ result };
        }

        return SmartKey{ std::string(value) };
    }
}

IniSection::IniSection(std::string name)
    : name{ std::move(name) }
{ }

SmartKey& IniSection::operator[](std::string_view key)
{
    return const_cast<SmartKey&>(std::as_const(*this)[key]);
}

const SmartKey& IniSection::operator[](std::string_view key) const
{
    auto it = entries.find(std::string{ key });
    if (it == entries.end())
    {
        throw std::runtime_error(std::format("Key '{}' not found in section '{}'", key, name));
    }

    return it->second;
}

IniParser::IniParser(std::string_view filename)
{
    std::ifstream file{ filename.data() };
    if (not file)
        throw std::runtime_error(std::format("Cannot open file: {}", filename));

    parse_stream(file);
}

void IniParser::parse_stream(std::istream& stream)
{
    std::string line_raw;

    while (std::getline(stream, line_raw))
    {
        // Trim
        std::string_view line = line_raw;
        if (auto pos = line.find(';'); pos != std::string_view::npos)
            line = line.substr(0, pos);

        line = TrimWhitespace(line);
        if (line.empty()) // Skip blank lines
            continue;

        // New Section
        if (line.starts_with('['))
        {
            if (not line.ends_with(']'))
                throw std::runtime_error(std::format("Malformed section line: {}", line));

            auto name = line.substr(1, line.size() - 2);

            m_sections.emplace_back(std::string{ name });
            continue;
        }

        auto eq_pos = line.find('=');
        if (eq_pos != std::string_view::npos)
        {
            if (m_sections.empty())
            {
                throw std::runtime_error(std::format("Key found before any section: {}", line));
            }

            IniSection& current_section = m_sections.back();

            auto key = TrimWhitespace(line.substr(0, eq_pos));
            auto value = TrimWhitespace(line.substr(eq_pos + 1));

            current_section.entries.insert_or_assign(std::string{ key }, DetermineType(value));
        }
        else
        {
            throw std::runtime_error(std::format("Invalid line format: {}", line));
        }
    }
}

IniSection& IniParser::operator[](std::string_view index)
{
    auto ret = rn::find_if(m_sections, [=](const auto& section_name)
    {
        return section_name == index;
    }, &IniSection::name);

    if (ret == m_sections.end())
    {
        throw std::runtime_error(std::format("The section: {} does not exist", index));
    }
    return *ret;
};

const IniSection& IniParser::operator[](std::string_view index) const
{
    auto ret = rn::find_if(m_sections, [=](const auto& section_name)
    {
        return section_name == index;
    }, &IniSection::name);

    if (ret == m_sections.end())
    {
        throw std::runtime_error(std::format("The section: {} does not exist", index));
    }
    return *ret;
}

void IniParser::SaveToFile(std::string_view filename) const
{
    std::fstream fs{ filename.data(), std::fstream::out };

    if (not fs.is_open())
    {
        throw std::runtime_error(std::format("Failed to open file: {}", filename));
    }

    auto print_variant = [](std::iostream& ios, const SmartKey& type)
    {
        if (std::holds_alternative<int>(type.GetValue()))
            ios << type.as<int>();
        else if (std::holds_alternative<float>(type.GetValue()))
            ios << type.as<float>();
        else if (std::holds_alternative<std::string>(type.GetValue()))
            ios << type.as<std::string>();
        else
            throw std::runtime_error("Something is wrong here");
    };


    std::size_t index{};
    for (const auto& elem : m_sections)
    {
        fs << '[' << elem.name << ']' << '\n';
        for (const auto& pair : elem.entries)
        {
            fs << pair.first << " = ";
            print_variant(fs, pair.second);
            fs << '\n';
        }

        // Want to skip last iteration
        if (index < m_sections.size() - 1)
        {
            index++;
            fs << '\n';
        }
    }

    fs.flush();
}
