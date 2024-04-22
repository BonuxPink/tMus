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

#include <functional>
#include <memory>

struct FocusManager;
struct FocusData;

struct Focus
{
    using NotifyType = std::function<void()>;

    explicit Focus(NotifyType C = {}, FocusManager* = nullptr, bool focused = false);

    [[nodiscard]] bool hasFocus() const noexcept;

    void focus();
    void toggle() const noexcept;
    void setNotify(NotifyType f) noexcept;

    std::shared_ptr<FocusData> m_Data;
};

struct FocusData
{
    Focus::NotifyType notify{};
    FocusManager* m_ControlManager{};
    bool hasFocus{};
};

struct FocusManager
{
    FocusManager(std::shared_ptr<Focus> current, std::shared_ptr<Focus> last);

    void setFocus(std::shared_ptr<FocusData>);
    void toggle() noexcept;

    std::shared_ptr<FocusData> m_CurrentControl;
    std::shared_ptr<FocusData> m_LastControl;
};
