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

#include "FocusManager.hpp"
#include "Renderer.hpp"

#include <utility>

Focus::Focus(NotifyType C, FocusManager *CM, bool focused)
    : m_Data(std::make_shared<FocusData>())
{
    m_Data->m_ControlManager = CM;
    m_Data->hasFocus = focused;
    m_Data->notify = std::move(C);
}

bool Focus::hasFocus() const noexcept { return m_Data->hasFocus; }

void Focus::focus()
{
    m_Data->m_ControlManager->setFocus(m_Data);
}

void Focus::toggle() const noexcept
{
    m_Data->m_ControlManager->toggle();
}

void Focus::setNotify(NotifyType f) noexcept
{
    m_Data->notify = std::move(f);
}

void FocusManager::setFocus(std::shared_ptr<FocusData> ctrl)
{
    m_CurrentControl->hasFocus = false;
    m_LastControl->hasFocus = false;

    ctrl->notify();
    Renderer::Render();
    ctrl->hasFocus = true;

    m_LastControl = m_CurrentControl;

    m_CurrentControl = ctrl;
}

void FocusManager::toggle() noexcept
{
    m_CurrentControl->hasFocus = !m_CurrentControl->hasFocus;
    m_LastControl->hasFocus = !m_LastControl->hasFocus;

    m_CurrentControl->notify();
    m_LastControl->notify();
}

FocusManager::FocusManager(std::shared_ptr<Focus> current, std::shared_ptr<Focus> last)
    : m_CurrentControl(current->m_Data)
    , m_LastControl(last->m_Data)
{
    m_CurrentControl->hasFocus = true;

    m_CurrentControl->m_ControlManager = this;
    m_LastControl->m_ControlManager = this;
}
