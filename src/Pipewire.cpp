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

#include "Pipewire.hpp"
#include "util.hpp"

#include <cmath>
#include <utility>

#define PW_KEY_NODE_RATE "node.rate"

enum
{
    FMT_FLOAT,
    FMT_S8,
    FMT_U8,
    FMT_S16_LE,
    FMT_S16_BE,
    FMT_U16_LE,
    FMT_U16_BE,
    FMT_S24_LE,
    FMT_S24_BE,
    FMT_U24_LE,
    FMT_U24_BE, /* padded to 4 bytes */
    FMT_S32_LE,
    FMT_S32_BE,
    FMT_U32_LE,
    FMT_U32_BE,
    FMT_S24_3LE,
    FMT_S24_3BE,
    FMT_U24_3LE,
    FMT_U24_3BE
}; /* packed in 3 bytes */

// Convert ffmpegs format to pipewires format. If conversion is not possible default to FMT_S16_LE.
static int ConvertFFMPEGFormatToPipewire(enum AVSampleFormat format)
{
    switch (format)
    {
    case AV_SAMPLE_FMT_NONE:
        break;
    case AV_SAMPLE_FMT_U8:
        return FMT_U8;
    case AV_SAMPLE_FMT_S16:
        return FMT_S16_LE;

    // This should work in the future.
    // case AV_SAMPLE_FMT_S32:
    //     return FMT_S32_LE;
    case AV_SAMPLE_FMT_FLT:
        return FMT_FLOAT;

    case AV_SAMPLE_FMT_S32:  [[fallthrough]];
    case AV_SAMPLE_FMT_S16P: [[fallthrough]];
    case AV_SAMPLE_FMT_S32P: [[fallthrough]];
    case AV_SAMPLE_FMT_FLTP: [[fallthrough]];
    case AV_SAMPLE_FMT_DBLP: [[fallthrough]];
    case AV_SAMPLE_FMT_S64:  [[fallthrough]];
    case AV_SAMPLE_FMT_S64P: [[fallthrough]];
    case AV_SAMPLE_FMT_U8P:  [[fallthrough]];
    case AV_SAMPLE_FMT_DBL:  [[fallthrough]];
    case AV_SAMPLE_FMT_NB:   [[fallthrough]];
    default:
        return FMT_S16_LE;
    }

    throw std::runtime_error(std::format("Failed to find convert ffmpeg's format {}", static_cast<int>(format)));
}

Pipewire::Pipewire(std::shared_ptr<AudioSettings> audioSettings)
    : m_audioSettings{ std::move(audioSettings) }
{
    InitPipewire();

    if (not m_inited or not m_has_sinks)
    {
        throw std::runtime_error("Unable to initilaize loop\n");
    }

    constexpr auto FMT_SIZEOF = [](int f)
    {
        if (f >= FMT_S24_3LE)
            return 3;
        else if (f >= FMT_S24_LE)
            return 4;
        else if (f >= FMT_S16_LE)
            return 2;
        else if (f >= FMT_S8)
            return 1;
        return static_cast<int>(sizeof (float));
    };

    auto ConvertFmtToStr = [](AVSampleFormat fmt)
    {
        switch (fmt)
        {
        case AV_SAMPLE_FMT_U8:
            return "U8";
        case AV_SAMPLE_FMT_S16:
            return "S16";
        case AV_SAMPLE_FMT_S32:
            return "S32";
        case AV_SAMPLE_FMT_FLT:
            return "FLT";
        case AV_SAMPLE_FMT_DBL:
            return "DBL";

        case AV_SAMPLE_FMT_U8P:
            return "U8P";
        case AV_SAMPLE_FMT_S16P:
            return "S16P";
        case AV_SAMPLE_FMT_S32P:
            return "S32P";
        case AV_SAMPLE_FMT_FLTP:
            return "FLTP";
        case AV_SAMPLE_FMT_DBLP:
            return "DBLP";
        case AV_SAMPLE_FMT_S64:
            return "S64";
        case AV_SAMPLE_FMT_S64P:
            return "S64P";

        default:
            std::unreachable();
        };
    };

    util::Log(color::green, "Pipewire init [fmt][freq][nb_ch]: [{}][{}][{}]\n", ConvertFmtToStr(m_audioSettings->fmt), m_audioSettings->freq, m_audioSettings->ch_layout.nb_channels);

    m_stride      = FMT_SIZEOF(ConvertFFMPEGFormatToPipewire(m_audioSettings->fmt)) * m_audioSettings->ch_layout.nb_channels;
    m_frames      = std::clamp<int>(64, static_cast<int>(std::ceil(static_cast<float>(2048 * m_audioSettings->freq) / 48000.f)), 8192);
    m_buffer_size = m_frames * m_stride;
    m_buffer      = new unsigned char[m_buffer_size];

    stream_events.version       = PW_VERSION_STREAM_EVENTS;
    stream_events.state_changed = on_state_changed;
    stream_events.process       = on_process;
    stream_events.drained       = on_drained;
    pw_thread_loop_lock(m_loop);

    auto create_stream = [this]
    {
        pw_properties* props =
            pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                                PW_KEY_MEDIA_CATEGORY, "Playback",
                                PW_KEY_MEDIA_ROLE, "Music",
                                nullptr);

        pw_properties_setf(props, PW_KEY_NODE_RATE, "1/%u", m_audioSettings->freq);
        pw_properties_setf(props, PW_KEY_NODE_LATENCY, "%u/%u", m_frames, m_audioSettings->freq);

        return pw_stream_new(m_core, "Playback", props);
    };

    if (m_stream = create_stream(); not m_stream)
    {
        pw_thread_loop_unlock(m_loop); // ?
        throw std::runtime_error("Failed to create stream");
    }

    set_volume(0.3f);

    pw_stream_add_listener(m_stream, &m_stream_listener, &stream_events, this);

    auto to_spa_pipewire_format = [](int fmt)
    {
        switch (fmt)
        {
        case FMT_FLOAT:   return SPA_AUDIO_FORMAT_F32_LE;

        case FMT_S8:      return SPA_AUDIO_FORMAT_S8;
        case FMT_U8:      return SPA_AUDIO_FORMAT_U8;

        case FMT_S16_LE:  return SPA_AUDIO_FORMAT_S16_LE;
        case FMT_S16_BE:  return SPA_AUDIO_FORMAT_S16_BE;
        case FMT_U16_LE:  return SPA_AUDIO_FORMAT_U16_LE;
        case FMT_U16_BE:  return SPA_AUDIO_FORMAT_U16_BE;

        case FMT_S24_LE:  return SPA_AUDIO_FORMAT_S24_32_LE;
        case FMT_S24_BE:  return SPA_AUDIO_FORMAT_S24_32_BE;
        case FMT_U24_LE:  return SPA_AUDIO_FORMAT_U24_32_LE;
        case FMT_U24_BE:  return SPA_AUDIO_FORMAT_U24_32_BE;

        case FMT_S24_3LE: return SPA_AUDIO_FORMAT_S24_LE;
        case FMT_S24_3BE: return SPA_AUDIO_FORMAT_S24_BE;
        case FMT_U24_3LE: return SPA_AUDIO_FORMAT_U24_LE;
        case FMT_U24_3BE: return SPA_AUDIO_FORMAT_U24_BE;

        case FMT_S32_LE:  return SPA_AUDIO_FORMAT_S32_LE;
        case FMT_S32_BE:  return SPA_AUDIO_FORMAT_S32_BE;
        case FMT_U32_LE:  return SPA_AUDIO_FORMAT_U32_LE;
        case FMT_U32_BE:  return SPA_AUDIO_FORMAT_U32_BE;

        default:          return SPA_AUDIO_FORMAT_UNKNOWN;
        }
    };

    auto pw_format = to_spa_pipewire_format(ConvertFFMPEGFormatToPipewire(m_audioSettings->fmt));
    if (pw_format == SPA_AUDIO_FORMAT_UNKNOWN)
    {
        pw_thread_loop_unlock(m_loop);
        throw std::runtime_error("Unknown audio format");
    }

    if (not connect_stream(pw_format))
    {
        pw_thread_loop_unlock(m_loop);
        throw std::runtime_error("unable to connect stream");
    }

    pw_stream_set_active(m_stream, true);
    float volume{ 0.3f };
    pw_stream_set_control(m_stream, SPA_PROP_volume, 0, &volume, 1);
    pw_thread_loop_unlock(m_loop);
}

Pipewire::~Pipewire()
{
    if (m_stream)
    {
        pw_thread_loop_lock(m_loop);
        m_ignore_state_change = true;
        pw_stream_disconnect(m_stream);
        pw_stream_destroy(m_stream);
        m_ignore_state_change = false;
        pw_thread_loop_unlock(m_loop);
    }

    if (m_loop)
        pw_thread_loop_stop(m_loop);

    if (m_registry)
    {
        pw_proxy_destroy(std::bit_cast<pw_proxy*>(m_registry));
    }

    if (m_core)
    {
        pw_core_disconnect(m_core);
    }

    if (m_context)
    {
        pw_context_destroy(m_context);
    }

    if (m_loop)
    {
        pw_thread_loop_destroy(m_loop);
    }

    if (m_buffer)
    {
        delete[] m_buffer;
    }
}

void Pipewire::on_core_event (void* data, std::uint32_t id, int seq) noexcept
{
    auto* o = std::bit_cast<Pipewire*>(data);

    if (id != PW_ID_CORE or seq != o->m_core_init_seq)
        return;

    spa_hook_remove(&o->m_registry_listener);
    spa_hook_remove(&o->m_core_listener);

    o->m_inited = true;
    pw_thread_loop_signal(o->m_loop, false);
}

void Pipewire::on_registry_event_global(void* data, [[maybe_unused]] std::uint32_t id, [[maybe_unused]] std::uint32_t permissions,
                        const char* type, [[maybe_unused]] std::uint32_t version, const spa_dict* props)
{
    auto* o = std::bit_cast<Pipewire*>(data);

    if (std::string_view{ type } != PW_TYPE_INTERFACE_Node)
        return;

    auto* media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    if (not media_class)
        return;

    if (std::string_view{ media_class } != "Audio/Sink")
        return;

    o->m_has_sinks = true;
    o->m_core_init_seq = pw_core_sync(o->m_core, PW_ID_CORE, o->m_core_init_seq);
}

void Pipewire::on_state_changed(void* data, [[maybe_unused]] enum pw_stream_state old,
                      enum pw_stream_state state, [[maybe_unused]] const char* error)
{
    auto* o = std::bit_cast<Pipewire*>(data);

    if (o->m_ignore_state_change)
        return;

    switch (state)
    {
    case PW_STREAM_STATE_UNCONNECTED:
    case PW_STREAM_STATE_PAUSED:
    case PW_STREAM_STATE_STREAMING:
        pw_thread_loop_signal(o->m_loop, false);
    default:
        break;
    }

    util::Log("State changed from: {} to: {}\n", pw_stream_state_as_string(old), pw_stream_state_as_string(state));
}

void Pipewire::on_process(void* data)
{
    auto* o = std::bit_cast<Pipewire*>(data);

    if (not o->m_buffer_at)
    {
        pw_thread_loop_signal(o->m_loop, false);
        return;
    }

    pw_buffer* b{};
    if (b = pw_stream_dequeue_buffer(o->m_stream); not b)
    {
        util::Log("pipewire: out of buffers\n");
        return;
    }

    spa_buffer* buf = b->buffer;

    void* dst{};
    if (dst = buf->datas[0].data; not dst)
    {
        util::Log("pipewire: no data pointer\n");
        return;
    }

    auto size = std::min<std::uint32_t>(buf->datas[0].maxsize, o->m_buffer_at);
    memcpy(dst, o->m_buffer, size);
    o->m_buffer_at -= size;
    memmove(o->m_buffer, o->m_buffer + size, o->m_buffer_at);

    b->buffer->datas[0].chunk->offset = 0;
    b->buffer->datas[0].chunk->size   = o->m_buffer_size;
    b->buffer->datas[0].chunk->stride = o->m_stride;

    pw_stream_queue_buffer(o->m_stream, b);
    pw_thread_loop_signal(o->m_loop, false);
};

void Pipewire::on_drained(void* data)
{
    auto* o = std::bit_cast<Pipewire*>(data);
    pw_thread_loop_signal(o->m_loop, false);
    util::Log("Events drain\n");
};

void Pipewire::period_wait() noexcept
{
    if (m_buffer_at != m_buffer_size)
        return;

    pw_thread_loop_lock(m_loop);
    pw_thread_loop_timed_wait(m_loop, 1);
    pw_thread_loop_unlock(m_loop);
}

std::size_t Pipewire::write_audio(const void *data, std::size_t length) noexcept
{
    pw_thread_loop_lock(m_loop);

    auto size = std::min<std::size_t>(m_buffer_size - m_buffer_at, length);
    memcpy(m_buffer + m_buffer_at, data, size);
    m_buffer_at += static_cast<int>(size);

    pw_thread_loop_unlock(m_loop);
    return size;
}

void Pipewire::set_volume(float percent) noexcept
{
    if (!m_loop)
        return;

    std::array<float, 8> volume{};
    volume.fill(percent);

    pw_thread_loop_lock(m_loop);
    pw_stream_set_control(m_stream, SPA_PROP_channelVolumes, m_audioSettings->ch_layout.nb_channels, volume.data(), nullptr);
    pw_thread_loop_unlock(m_loop);
}

void Pipewire::InitPipewire()
{
    pw_init(nullptr, nullptr);

    if (m_loop = pw_thread_loop_new("tMus-pipewire-main-loop", nullptr); not m_loop)
    {
        throw std::runtime_error("Pipewire failed to create new loop\n");
    }

    if (m_context = pw_context_new(pw_thread_loop_get_loop(m_loop), nullptr, 0); not m_context)
    {
        throw std::runtime_error("pw_context_new failed\n");
    }

    if (m_core = pw_context_connect(m_context, nullptr, 0); not m_core)
    {
        throw std::runtime_error("Failed to connect context\n");
    }

    if (m_registry = pw_core_get_registry(m_core, PW_VERSION_REGISTRY, 0); not m_registry)
    {
        throw std::runtime_error("Failed to get core registry\n");
    }

    pw_core_add_listener(m_core, &m_core_listener, &core_events, this);
    pw_registry_add_listener(m_registry, &m_registry_listener, &registry_events, this);

    m_core_init_seq = pw_core_sync(m_core, PW_ID_CORE, m_core_init_seq);

    if (pw_thread_loop_start(m_loop) != 0)
    {
        throw std::runtime_error("Unable to start the loop\n");
    }

    pw_thread_loop_lock(m_loop);
    while (not m_inited)
    {
        if (pw_thread_loop_timed_wait(m_loop, 2) != 0)
            break;
    }

    pw_thread_loop_unlock(m_loop);
}

void Pipewire::set_channel_map(spa_audio_info_raw* info, int channels) noexcept
{
    switch (channels)
    {
        case 9:
            info->position[8] = SPA_AUDIO_CHANNEL_RC;
            [[fallthrough]];
        case 8:
            info->position[6] = SPA_AUDIO_CHANNEL_FLC;
            info->position[7] = SPA_AUDIO_CHANNEL_FRC;
            [[fallthrough]];
        case 6:
            info->position[4] = SPA_AUDIO_CHANNEL_RL;
            info->position[5] = SPA_AUDIO_CHANNEL_RR;
            [[fallthrough]];
        case 4:
            info->position[3] = SPA_AUDIO_CHANNEL_LFE;
            [[fallthrough]];
        case 3:
            info->position[2] = SPA_AUDIO_CHANNEL_FC;
            [[fallthrough]];
        case 2:
            info->position[0] = SPA_AUDIO_CHANNEL_FL;
            info->position[1] = SPA_AUDIO_CHANNEL_FR;
            break;
        case 1:
            info->position[0] = SPA_AUDIO_CHANNEL_MONO;
            break;
    }
}

bool Pipewire::connect_stream(enum spa_audio_format format) noexcept
{
    std::uint8_t buffer[1024];
    spa_pod_builder b = {
        .data = buffer,
        .size = sizeof buffer,
        ._padding = 0,
        .state = {.offset = 0, .flags = 0, .frame = nullptr},
        .callbacks = { nullptr, nullptr },
    };

    spa_audio_info_raw audio_info
    {
        .format   = format,
        .flags    = SPA_AUDIO_FLAG_NONE,
        .rate     = static_cast<std::uint32_t>(m_audioSettings->freq),
        .channels = static_cast<std::uint32_t>(m_audioSettings->ch_layout.nb_channels),
        .position = {},
    };

    set_channel_map(&audio_info, m_audioSettings->ch_layout.nb_channels);
    const spa_pod* params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info);

    auto stream_flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                                        PW_STREAM_FLAG_MAP_BUFFERS |
                                                        PW_STREAM_FLAG_RT_PROCESS);

    constexpr auto elems = []<typename T, int N>(const T(&)[N])
    {
        return N;
    };

    return pw_stream_connect(m_stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
                                stream_flags, params, elems(params)) == 0;
}
