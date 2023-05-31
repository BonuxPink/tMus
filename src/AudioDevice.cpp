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
    static constexpr const std::array nextSampleRate{ 0, 44'100, 48'000, 96'000, 192'000 };
    int nextSampleRateIdx = nextSampleRate.size() - 1;

    while (nextSampleRateIdx && nextSampleRate[nextSampleRateIdx] >= wanted->freq)
        nextSampleRateIdx--;

    auto wantedNbChannels = wanted->channels;
    do
    {
        audioDeviceID = SDL_OpenAudioDevice(nullptr, 0, wanted.get(), &spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
        if (audioDeviceID == 0)
        {
            util::Log("SDL_OpenAudio ({} channels, {} Hz): {}\n", wanted->channels, wanted->freq, SDL_GetError());
            static const std::array<unsigned char, 8> nextNbChannels{ 0, 0, 1, 6, 2, 6, 4, 6 };
            wanted->channels = nextNbChannels[std::min(std::uint8_t(7), wanted->channels)];
            if (!wanted->channels)
            {
                wanted->freq = nextSampleRate[nextSampleRateIdx--];
                wanted->channels = wantedNbChannels;
                if (!wanted->freq)
                {
                    throw std::runtime_error("No more combinations to try, audio open failed");
                }
            }
            av_channel_layout_default(wantedLayout, wanted->channels);
        }

    } while( audioDeviceID == 0 );

    if (spec.format != AUDIO_S16SYS)
    {
        throw std::runtime_error(fmt::format("SDL advised audio format {} is not supported", spec.format));
    }

    if (spec.channels != wanted->channels)
    {
        av_channel_layout_uninit(wantedLayout);
        av_channel_layout_default(wantedLayout, spec.channels);
        if (wantedLayout->order != AV_CHANNEL_ORDER_NATIVE)
        {
            throw std::runtime_error(fmt::format("SDL advised channel count {} is not supported", spec.channels));
        }
    }

    util::Log(fg(fmt::color::teal),
              "SDL spec we got Ch: {}, Freq: {}, Format: {}, padding: {}, silence: {}, size: {}\n",
              spec.channels, spec.freq, spec.format, spec.padding, spec.silence, spec.size);

    setupParams(wantedLayout);

    audioHwBufSize = static_cast<int>(spec.size);
    audioDiffThreshold = static_cast<double>(audioHwBufSize) / params.bytes_per_sec;
    util::Log(fg(fmt::color::yellow), "devID: {}\n", audioDeviceID);
    SDL_PauseAudioDevice(audioDeviceID, 0);
}

AudioDevice::~AudioDevice()
{
    SDL_CloseAudioDevice(audioDeviceID);
}

void AudioDevice::setupParams(AVChannelLayout* wantedLayout)
{
    util::Log("Setting up params\n");
    params.fmt = AV_SAMPLE_FMT_S16;
    params.freq = spec.freq;

    if (av_channel_layout_copy(&params.ch_layout, wantedLayout))
    {
        throw std::runtime_error("Failed to copy layout");
    }

    params.frame_size = av_samples_get_buffer_size(nullptr, params.ch_layout.nb_channels, 1, params.fmt, 1);
    params.bytes_per_sec = av_samples_get_buffer_size(nullptr, params.ch_layout.nb_channels, params.freq, params.fmt, 1);

    if (params.bytes_per_sec <= 0 || params.frame_size <= 0)
    {
        throw std::runtime_error(fmt::format("bytes_per_sec: {} or frame_size: {} are dubious",
                                             params.bytes_per_sec, params.frame_size));
    }
}
