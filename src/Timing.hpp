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

#include "Clock.hpp"
#include "Constants.hpp"
#include <memory>

struct Timing
{
    explicit Timing(int serial)
        : audioclk(std::make_shared<int>(serial))
        , extclk(nullptr)
        , m_audioDiffAvgCoef(std::exp(std::log(0.01) / Constants::AUDIO_DIFF_AVG_NB))
    { }

    enum class AVSync
    {
        AV_SYNC_AUDIO_MASTER,   /* default choice */
        AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
    };

    Timing::AVSync get_master_sync_type();

    AVSync avSyncType{};
    Clock audioclk;
    Clock extclk;

    double m_audioDiffAvgCoef{};
    double m_audioDiffThreshold{};
    double m_audioDiffCum{};

    int m_audioDiffAvgCount{};
};
