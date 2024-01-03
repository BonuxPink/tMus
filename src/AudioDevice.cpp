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

#include "AudioDevice.hpp"
#include "util.hpp"

#include <SDL2/SDL_audio.h>
#include <array>
#include <libavformat/avformat.h>
#include <vector>
#include <condition_variable>
#include <exception>
#include <stdexcept>

extern "C"
{
    #include <libavutil/samplefmt.h>
    #include <libavutil/channel_layout.h>
}

[[gnu::nonnull(3)]]
AudioDevice::AudioDevice(std::unique_ptr<SDL_AudioSpec> wanted, AVChannelLayout* wantedLayout)
{
    constexpr std::array nextSampleRate{ 48'000, 44'100 };
    int counter = -1;
    int it = 0;

    do
    {
        wanted->freq = nextSampleRate[counter++];
        audioDeviceID = SDL_OpenAudioDevice(nullptr, 0, wanted.get(), &actual, 0);
        util::Log("Iteration: {}\n", it++);
    } while (audioDeviceID == 0);

    if (actual.format != AUDIO_S16SYS)
    {
        throw std::runtime_error(fmt::format("SDL advised audio format {} is not supported", actual.format));
    }

    if (actual.channels != wanted->channels)
    {
        av_channel_layout_uninit(wantedLayout);
        av_channel_layout_default(wantedLayout, actual.channels);
        if (wantedLayout->order != AV_CHANNEL_ORDER_NATIVE)
        {
            throw std::runtime_error(fmt::format("SDL advised channel count {} is not supported", actual.channels));
        }
    }

    util::Log(fg(fmt::color::teal),
              "SDL spec we got Ch: {}, Freq: {}, Format: {}, padding: {}, silence: {}, size: {}\n",
              actual.channels, actual.freq, actual.format, actual.padding, actual.silence, actual.size);

    util::Log(fg(fmt::color::yellow), "devID: {}\n", audioDeviceID);
    SDL_PauseAudioDevice(audioDeviceID, 0);
}

AudioDevice::~AudioDevice()
{
    SDL_CloseAudioDevice(audioDeviceID);
}
