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

#include "Constants.hpp"
#include <bits/chrono.h>
#include <cmath>
#include <atomic>
#include <memory>
#include <chrono>

extern "C"
{
    #include <libavutil/time.h>
}

struct Clock
{
    double pts{};
    double pts_drift{};
    double last_updated{};
    double speed{ 1.0 };
    std::shared_ptr<int> queue_serial{};
    std::atomic<int> serial{};
    std::atomic<bool> paused{ false };

    mutable std::mutex clock_mutex; // Mutex must be available. Even in const methods

    Clock() = delete;
    Clock(const auto&) = delete;
    Clock(Clock&&) noexcept = delete;

    Clock& operator=(const Clock&) = delete;
    Clock& operator=(Clock&&) noexcept = delete;

    explicit Clock(std::nullptr_t)
        : queue_serial(std::make_shared<int>(0))
    {
        set_clock(NAN, -1);
    }

    explicit Clock(std::shared_ptr<int> q_serial)
        : queue_serial(std::move(q_serial))
    {
        set_clock(NAN, -1);
    }

    void set_clock_at(double in_pts, int in_serial, double in_time)
    {
        const std::lock_guard lk{ clock_mutex };

        pts = in_pts;
        last_updated = in_time;
        pts_drift = pts - in_time;
        serial = in_serial;
    }

    void set_clock(double in_pts, int in_serial)
    {
        auto now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        auto time = static_cast<double>(now) / 1000000.0;

        set_clock_at(in_pts, in_serial, time);
    }

    double get_clock() const
    {
        const std::lock_guard lk{ clock_mutex };
        if (*queue_serial != serial)
            return NAN;

        if (paused)
        {
            return pts;
        }
        else
        {
            auto now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            auto time = static_cast<double>(now ) / 1000000.0;

            return  pts_drift + time - (time - last_updated) * (1.0 - speed);
        }

    }

    void set_clock_speed(double in_speed)
    {
        set_clock(get_clock(), serial);
        speed = in_speed;
    }

    void sync_clock_to_slave(const Clock& slave)
    {
        double clock = get_clock();
        double slave_clock = slave.get_clock();

        if (!std::isnan(slave_clock) && (std::isnan(clock) ||
            std::fabs(clock - slave_clock) > Constants::AV_NOSYNC_THRESHOLD))
        {
            set_clock(slave_clock, slave.serial);
        }
    }
};
