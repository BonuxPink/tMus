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

#include <functional>
#include <memory>
#include <utility>

struct ControlManager;
struct ControlData;

struct Control
{
    using NotifyType = std::function<void()>;
    Control(auto) = delete;

    explicit Control(NotifyType C = {}, ControlManager* = nullptr, bool focused = false);

    [[nodiscard]] bool hasFocus() const noexcept;

    void focus();
    void toggle() const noexcept;
    void setNotify(NotifyType f) noexcept;

    std::shared_ptr<ControlData> m_Data;
};

struct ControlData
{
    Control::NotifyType notify{};
    ControlManager* m_ControlManager{};
    bool hasFocus{};
};

struct ControlManager
{
    void setFocus(std::shared_ptr<ControlData>);
    void toggle() noexcept;

    std::shared_ptr<ControlData> m_CurrentControl;
    std::shared_ptr<ControlData> m_LastControl;

    ControlManager(std::shared_ptr<Control> current, std::shared_ptr<Control> last);
};
