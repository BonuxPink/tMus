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

#include <cstdio>
#include <fmt/std.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <fmt/chrono.h>
#include <fmt/color.h>

#include <chrono>
#include <mutex>
#include <source_location>

namespace util
{
    namespace internal
    {
        inline constexpr bool OutputFile { true };
        inline std::ofstream logFileStream{};
        inline std::string message_buf{};
    }
    template <typename... Args>
    struct Log final
    {
    public:

        Log(fmt::text_style style, fmt::format_string<Args...>&& fmt, Args&&... args, std::source_location location = std::source_location::current()) noexcept
        {
            internal::message_buf.clear();
            const auto micros = std::chrono::time_point(std::chrono::high_resolution_clock::now());
            fmt::format_to(std::back_inserter(internal::message_buf), style, "[{}:{:03}][{:%H:%M:%S}] ", location.file_name(), location.line(), micros);
            fmt::format_to(std::back_inserter(internal::message_buf), std::forward<fmt::format_string<Args...>>(fmt), std::forward<Args>(args)...);

            if constexpr (internal::OutputFile)
            {
                internal::logFileStream << internal::message_buf;
                internal::logFileStream.flush();
            }
        }

        Log(fmt::format_string<Args...>&& fmt, Args&&... args, std::source_location location = std::source_location::current()) noexcept
        {
            Log(fmt::text_style(), std::forward<fmt::format_string<Args...>>(fmt), std::forward<Args>(args)..., std::forward<std::source_location>(location));
        }
    };

    // Deduction guides for Log constructors
    template<typename... Args>
    Log(fmt::text_style, fmt::format_string<Args...>, Args&&...) -> Log<Args...>;

    template<typename... Args>
    Log(fmt::format_string<Args...>, Args&&...) -> Log<Args...>;
}
