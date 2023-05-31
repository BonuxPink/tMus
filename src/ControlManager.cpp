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

#include "ControlManager.hpp"

#include <utility>

Control::Control(NotifyType C, ControlManager *CM, bool focused)
    : m_Data(std::make_shared<ControlData>())
{
    m_Data->m_ControlManager = CM;
    m_Data->hasFocus = focused;
    m_Data->notify = std::move(C);
}

bool Control::hasFocus() const noexcept { return m_Data->hasFocus; }

void Control::focus()
{
    m_Data->m_ControlManager->setFocus(m_Data);
}

void Control::toggle() const noexcept
{
    m_Data->m_ControlManager->toggle();
}

void Control::setNotify(NotifyType f) noexcept
{
    m_Data->notify = std::move(f);
}

void ControlManager::setFocus(std::shared_ptr<ControlData> ctrl)
{
    m_CurrentControl->hasFocus = false;
    m_LastControl->hasFocus = false;

    ctrl->notify();
    ctrl->hasFocus = true;

    m_LastControl = m_CurrentControl;

    m_CurrentControl = ctrl;
}

void ControlManager::toggle() noexcept
{
    m_CurrentControl->hasFocus = !m_CurrentControl->hasFocus;
    m_LastControl->hasFocus = !m_LastControl->hasFocus;

    m_CurrentControl->notify();
    m_LastControl->notify();
}

ControlManager::ControlManager(std::shared_ptr<Control> current, std::shared_ptr<Control> last)
    : m_CurrentControl(current->m_Data)
    , m_LastControl(last->m_Data)
{
    m_CurrentControl->hasFocus = true;

    m_CurrentControl->m_ControlManager = this;
    m_LastControl->m_ControlManager = this;
}
