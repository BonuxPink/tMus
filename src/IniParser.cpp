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

#include "IniParser.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>
#include <format>

namespace rn = std::ranges;

static void ParseOutComment(std::string& str)
{
    if (str.contains(';'))
    {
        str.erase(str.find_first_of(';') - 1);
    }
}

static std::string TrimWhitespace(std::string str)
{
    const auto strBegin = str.find_first_not_of(' ');
    if (strBegin == std::string_view::npos)
    {
        return str;
    }

    const auto strEnd = str.find_last_not_of(' ');
    const auto diff = strEnd - strBegin + 1;

    return str.substr(strBegin, diff);
};

IniSection::IniSection(std::istream& f, std::string sName)
{
    ParseOutComment(sName);
    sName = TrimWhitespace(sName);

    if (not sName.ends_with(']'))
    {
        throw std::runtime_error("ERROR in parsing, section must end with ']'");
    }

    auto secEnd = sName.find(']') - 1; // The notorious -1
    auto secName = sName.substr(1, secEnd); // We know that we begin with '['
    section_name = secName;

    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty())
        {
            break;
        }

        if (not line.contains('='))
        {
            throw std::runtime_error(std::format("Line: {}, does not containt '='", line));
        }

        const auto eq = line.find('=');

        auto key = line.substr(0, eq);
        auto value = line.substr(eq + 1);

        key = TrimWhitespace(key);
        value = TrimWhitespace(value);

        auto ret = rn::find_if(value, [](auto ch)
        {
            if (ch == '.')
                return false;

            return not std::isdigit(ch);
        });

        if (values.contains(key))
        {
            throw std::runtime_error(std::format("Key: {}, has already been parsed", key));
        }

        if (value.contains('.')) // must be a float
        {
            values[key] = SmartKey{ std::stof(value) };
        }
        else if (ret == value.end()) // must be int
        {
            values[key] = SmartKey{ std::stoi(value) };
        }
        else // string
        {
            values[key] = SmartKey{ std::string(value) };
        }
    }
}

SmartKey& IniSection::operator[](std::string_view index)
{
    auto str = std::string{ index };
    return values[str];
}

const SmartKey& IniSection::operator[](std::string_view index) const
{
    auto end = rn::find_if(values.begin(), values.end(), [&](const auto& pair)
    {
        return pair.first == index;
    });

    if (end == values.end())
    {
        throw std::runtime_error(std::format("Failed to find key: {}", index));
    }

    return end->second;
}

IniParser::IniParser(std::istream& file)
{
    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        switch (auto ch = line.at(0); ch)
        {
        case ';': // ';' is a comment line, if we begin with it, just skip it
            continue;
        case '[':
            IniSection sec{ file, line };
            vec.push_back(std::move(sec));
            break;
        }
    }
}

IniSection& IniParser::operator[](std::string_view index)
{
    auto ret = rn::find_if(vec, [=](const auto& section_name)
    {
        return section_name == index;
    }, &IniSection::section_name);

    if (ret == vec.end())
    {
        throw std::runtime_error(std::format("The section: {} does not exist", index));
    }
    return *ret;
};

const IniSection& IniParser::operator[](std::string_view index) const
{
    auto ret = rn::find_if(vec, [=](const auto& section_name)
    {
        return section_name == index;
    }, &IniSection::section_name);

    if (ret == vec.end())
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
    for (const auto& elem : vec)
    {
        fs << '[' << elem.section_name << ']' << '\n';
        for (const auto& pair : elem.values)
        {
            fs << pair.first << " = ";
            print_variant(fs, pair.second);
            fs << '\n';
        }

        // Want to skip last iteration
        if (index < vec.size() - 1)
        {
            index++;
            fs << '\n';
        }
    }

    fs.flush();
}
