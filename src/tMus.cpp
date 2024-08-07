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

#include "tMus.hpp"

#include "util.hpp"
#include "Factories.hpp"
#include "Colors.hpp"

#include <ncpp/NotCurses.hh>
#include <fcntl.h>

#if 0
    auto cb = +[]([[maybe_unused]] void* avcl, [[maybe_unused]] int level, const char* fmt, va_list args)
    {
        std::array<char, 1024> buf{0};
        vsnprintf(buf.data(), buf.size(), fmt, args);
        util::Log(fg(fmt::color::lavender_blush), "{}", buf.data());

    };
#endif

void tMus::InitLog()
{
    if constexpr (util::internal::OutputFile)
    {
        util::internal::logFileStream = std::ofstream{ "/tmp/log.txt", std::ios::app };
    }

    util::internal::message_buf.reserve(4096);

    /*
     * Just be really quiet.
     */
    av_log_set_level(AV_LOG_QUIET);
#if 1
    auto cb = +[]([[maybe_unused]] void*,
                  [[maybe_unused]] int,
                  [[maybe_unused]] const char*,
                  [[maybe_unused]] va_list)
    { };
#endif
    av_log_set_callback(cb);
}

void tMus::Init()
{
    if (setlocale(LC_ALL, "") == nullptr)
    {
        throw std::runtime_error("Could not set locale to en_US.utf8\n");
    }

    const auto stdPlane = std::unique_ptr<ncpp::Plane>(ncpp::NotCurses::get_instance().get_stdplane());
    stdPlane->set_base("", 0, Colors::DefaultBackground);
}
