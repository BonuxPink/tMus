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

#include "Colors.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <print>

#include <chrono>
#include <source_location>

namespace util
{
    namespace internal
    {
        inline constexpr bool OutputFile { true };
        inline std::ofstream logFileStream{};
        inline std::string message_buf{};
    }

    inline void Printer()
    {
        if constexpr (internal::OutputFile)
        {
            internal::logFileStream << internal::message_buf;
            internal::logFileStream.flush();
        }
    }

    template <typename... Args>
    struct Log final
    {
    public:

        Log(color c, std::format_string<Args...>&& fmt, Args&&... args, std::source_location location = std::source_location::current()) noexcept
        {
            internal::message_buf.clear();
            const auto micros = std::chrono::time_point(std::chrono::high_resolution_clock::now());
            const std::string_view floc{ location.file_name() };
            const std::string_view filename{ floc.data() + floc.find_last_of('/') + 1 };

            std::format_to(std::back_inserter(internal::message_buf), "[{}:{:03}][{:%H:%M:%S}] ", location.file_name(), location.line(), micros);
            std::format_to(std::back_inserter(internal::message_buf), std::forward<std::format_string<Args...>>(std::move(fmt)), std::forward<Args>(args)...);

            if (c != color::NOCOLOR)
            {
                struct rgb
                {
                    constexpr rgb(std::uint32_t hex)
                        : r{ static_cast<std::uint8_t>((hex >> 16) & 0xFF) }
                        , g{ static_cast<std::uint8_t>((hex >> 8) & 0xFF) }
                        , b{ static_cast<std::uint8_t>(hex & 0xFF) }
                    { }

                    std::uint8_t r;
                    std::uint8_t g;
                    std::uint8_t b;
                };

                struct ansi_color_escape
                {
                    constexpr ansi_color_escape(color col) noexcept
                    {
                        auto to_esc = [](std::uint8_t c, char* out, char delim) noexcept
                        {
                            out[0] = static_cast<char>('0' + c / 100);
                            out[1] = static_cast<char>('0' + c / 10 % 10);
                            out[2] = static_cast<char>('0' + c % 10);
                            out[3] = static_cast<char>(delim);
                        };

                        rgb color(static_cast<std::uint32_t>(col));
                        to_esc(color.r, buffer + 7, ';');
                        to_esc(color.g, buffer + 11, ';');
                        to_esc(color.b, buffer + 15, 'm');
                        buffer[19] = char('\0');
                    }

                    char buffer[7u + 3u * 8 + 1u]{ "\x1b[38;2;" };
                };

                const auto foreground = ansi_color_escape(c);
                internal::message_buf.insert(0, foreground.buffer);

                constexpr std::string_view reset_color{ "\x1b[0m" };
                internal::message_buf.append(reset_color.begin(), reset_color.end());
            }

            Printer();
        }

        Log(std::format_string<Args...>&& fmt, Args&&... args, std::source_location location = std::source_location::current()) noexcept
        {
            Log(color::NOCOLOR, std::forward<std::format_string<Args...>>(fmt), std::forward<Args>(args)..., std::forward<std::source_location>(location));
        }
    };

    // Deduction guides for Log constructors
    template<typename... Args>
    Log(color, std::format_string<Args...>, Args&&...) -> Log<Args...>;

    template<typename... Args>
    Log(std::format_string<Args...>, Args&&...) -> Log<Args...>;

    std::filesystem::path GetUserConfigDir();
}
