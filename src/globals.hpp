/*
 * Copyright (C) 2023 Dāniels Ponamarjovs <bonux@duck.com>
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

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include "Controls.hpp"

extern "C"
{
    #include <libavutil/avutil.h>
}

namespace Globals
{
    inline std::int64_t start_time{ AV_NOPTS_VALUE };
    inline std::int64_t duration{ AV_NOPTS_VALUE };
    inline std::int64_t audio_callback_time{ 0 };
    inline std::atomic_bool abort_request{ false };
    inline Completion lastCompletion{};

    inline Event event;
}
