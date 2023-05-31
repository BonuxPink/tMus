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

#include <algorithm>
#include <cmath>

namespace Constants
{
    inline constexpr int FRAME_QUEUE_SIZE { 16 };
    inline constexpr int MAX_QUEUE_SIZE { 15 * 1024 * 1024 };
    inline constexpr int MIN_FRAMES{ 25 };

    inline constexpr int AUDIO_DIFF_AVG_NB { 20 };
    inline constexpr int SAMPLE_CORRECTION_PERCENT_MAX { 10 };

    inline constexpr int SDL_AUDIO_MIN_BUFFER_SIZE { 512 };
    inline constexpr int SDL_AUDIO_MAX_CALLBACKS_PER_SEC { 30 };

    inline constexpr double AV_NOSYNC_THRESHOLD { 10.0 };
}
