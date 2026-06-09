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

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct SmartKey
{
    using VariantType = std::variant<int, std::string, float>;

    explicit SmartKey() = default;

    explicit SmartKey(auto&& v)
        : value { std::forward<decltype(v)>(v) }
    { }

    template <typename T>
    [[nodiscard]] T as() const
    {
        return std::get<T>(value);
    }

    [[nodiscard]] std::string as_string() const
    {
        if (std::holds_alternative<std::string>(value))
            return std::get<std::string>(value);

        if (std::holds_alternative<int>(value))
            return std::to_string(std::get<int>(value));

        if (std::holds_alternative<float>(value))
            return std::to_string(std::get<float>(value));

        return {};
    }

    SmartKey& operator=(auto newvalue)
    {
        value = std::forward<decltype(newvalue)>(newvalue);
        return *this;
    }

    [[nodiscard]] inline const VariantType& GetValue() const noexcept
    {
        return value;
    }

private:
    VariantType value;
};

struct IniSection
{
    explicit(true) IniSection(std::string sName);

    [[nodiscard]] SmartKey& operator[](std::string_view index);
    [[nodiscard]] const SmartKey& operator[](std::string_view index) const;

    auto begin() const noexcept
    {
        return entries.begin();
    }
    auto end() const noexcept
    {
        return entries.end();
    }

    std::string name;
    std::unordered_map<std::string, SmartKey> entries;
};

struct ParseOptions
{
    bool strip_comments = true;
};

class IniParser
{
public:
    explicit IniParser(std::string_view filename, ParseOptions opts = ParseOptions {});

    [[nodiscard]] auto getSectionCount() const noexcept
    {
        return m_sections.size();
    }

    [[nodiscard]] IniSection& operator[](std::string_view index);
    [[nodiscard]] const std::vector<IniSection>& sections() const noexcept
    {
        return m_sections;
    }
    [[nodiscard]] const IniSection& operator[](std::string_view index) const;

    void SaveToFile(std::string_view filename) const;

private:
    void parse_stream(std::istream& stream);

    ParseOptions opts;
    std::vector<IniSection> m_sections;
};
