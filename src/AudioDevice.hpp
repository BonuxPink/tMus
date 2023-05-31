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

#include "AudioParams.hpp"

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
    AudioDevice(const auto) = delete;
    explicit AudioDevice(std::unique_ptr<SDL_AudioSpec> wanted, AVChannelLayout* layout);
    ~AudioDevice();

    [[nodiscard]] const SDL_AudioSpec& getSpec() const { return spec; }
    [[nodiscard]] const AudioParams& getParams() const { return params; }

private:
    void setupParams(AVChannelLayout* wantedLayout);

    SDL_AudioSpec spec{};
    SDL_AudioDeviceID audioDeviceID{};

    AudioParams params{};

    double audioDiffThreshold{};

    int audioHwBufSize{};
    int audioBufSize{};
    unsigned audioBufIndex{};
    int audioDuffAvgCount{};

};

inline std::jthread playbackThread{};
