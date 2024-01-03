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

#include <SDL2/SDL_audio.h>

#include <condition_variable>
#include <memory>
#include <thread>

extern "C"
{
    #include <libavutil/channel_layout.h>
}

struct AudioDevice
{
    AudioDevice() = default;
    explicit AudioDevice(std::unique_ptr<SDL_AudioSpec> wanted, AVChannelLayout* layout);
    ~AudioDevice();

    AudioDevice& operator=(const AudioDevice&) = default;

    SDL_AudioSpec actual{};
    SDL_AudioDeviceID audioDeviceID{};
};

inline std::jthread playbackThread{};
