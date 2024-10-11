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

#include <cstdint>
#include <memory>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>

#pragma GCC diagnostic pop
extern "C"
{
    #include <libavutil/samplefmt.h>
}

#include "AudioSettings.hpp"

class Pipewire
{
public:

    Pipewire(const Pipewire &) = delete;
    Pipewire(Pipewire &&) = delete;
    Pipewire &operator=(const Pipewire &) = delete;
    Pipewire &operator=(Pipewire &&) = delete;

    explicit Pipewire(std::shared_ptr<AudioSettings> audioSettings);
    ~Pipewire();

    void period_wait() noexcept;
    std::size_t write_audio(const void *data, std::size_t length) noexcept;
    void set_volume(float percent) noexcept;

private:

    void InitPipewire();
    void set_channel_map(spa_audio_info_raw* info, int channels) noexcept;
    bool connect_stream(enum spa_audio_format format) noexcept;

    void open_audio(enum AVSampleFormat format, int rate, int channels);

    std::shared_ptr<AudioSettings> m_audioSettings;

    pw_core_events core_events
    {
        .version     = PW_VERSION_CORE_EVENTS,
        .info        = nullptr,
        .done        = on_core_event,
        .ping        = nullptr,
        .error       = nullptr,
        .remove_id   = nullptr,
        .bound_id    = nullptr,
        .add_mem     = nullptr,
        .remove_mem  = nullptr,
        .bound_props = nullptr
    };

    pw_registry_events registry_events
    {
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global = on_registry_event_global,
        .global_remove = nullptr
    };

    pw_stream_events stream_events {};


    pw_thread_loop*  m_loop{};
    pw_stream*       m_stream{};
    pw_context*      m_context{};
    pw_core*         m_core{};
    pw_registry*     m_registry{};

    bool m_inited{};
    bool m_has_sinks{};
    bool m_ignore_state_change{};

    int m_core_init_seq{};

    unsigned char* m_buffer{};
    unsigned m_buffer_at{};
    unsigned m_buffer_size{};
    unsigned m_frames{};
    unsigned m_stride{};

    spa_hook m_core_listener{};
    spa_hook m_stream_listener{};
    spa_hook m_registry_listener{};

    static void on_core_event (void* data, std::uint32_t id, int seq) noexcept;
    static void on_registry_event_global(void* data, [[maybe_unused]] std::uint32_t id, [[maybe_unused]] std::uint32_t permissions,
                                         const char* type, [[maybe_unused]] std::uint32_t version, const spa_dict* props);
    static void on_state_changed(void* data, [[maybe_unused]] enum pw_stream_state old,
                                 enum pw_stream_state state, [[maybe_unused]] const char* error);
    static void on_process(void* data);
    static void on_drained(void* data);
};
