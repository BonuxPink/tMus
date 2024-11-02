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

#pragma once

#include <vector>
#include <unordered_map>
#include <variant>
#include <string>

struct SmartKey
{
    using VariantType = std::variant<int, std::string, float>;

    explicit SmartKey() = default;
    explicit SmartKey(auto value)
        : m_value{ value }
    { }

    template <typename T>
    [[nodiscard]] T as() const
    {
        return std::get<T>(m_value);
    }

    SmartKey& operator=(auto newvalue)
    {
        m_value = newvalue;
        return *this;
    }

    [[nodiscard]] inline constexpr auto GetValue() const noexcept
    { return m_value; }

private:
    VariantType m_value;
};

struct IniSection
{
    explicit(true) IniSection(std::istream& f, std::string sName);

    [[nodiscard]] SmartKey& operator[](std::string_view index);
    [[nodiscard]] const SmartKey& operator[](std::string_view index) const;

    auto begin() const noexcept
    {
        return values.begin();
    }

    auto end() const noexcept
    {
        return values.end();
    }

    std::string section_name;
    std::unordered_map<std::string, SmartKey> values;
};

class IniParser
{
public:
    explicit(true) IniParser(std::istream& file);

    [[nodiscard]] auto getSectionCount() const noexcept
    {
        return m_sections.size();
    }

    [[nodiscard]] IniSection& operator[](std::string_view index);
    [[nodiscard]] const IniSection& operator[](std::string_view index) const;

    void SaveToFile(std::string_view filename) const;

private:
    std::vector<IniSection> m_sections;
};
