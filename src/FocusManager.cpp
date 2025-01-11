/*
 * Copyright (C) 2023-2025 Dāniels Ponamarjovs <bonux@duck.com>
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
    : m_notify(std::move(C)), m_FocusManager(CM), m_hasFocus(focused)
{ }

bool Focus::hasFocus() const noexcept { return m_hasFocus; }

void Focus::focus()
{
    m_FocusManager->setFocus(shared_from_this());
}

void Focus::toggle() noexcept
{
    // m_hasFocus = !m_hasFocus;
    m_FocusManager->toggle();
}

void Focus::setNotify(NotifyType f) noexcept
{
    m_notify = std::move(f);
}

void Focus::setFocusOff() noexcept
{
    m_hasFocus = false;
}

void FocusManager::setFocus(std::shared_ptr<Focus> ctrl)
{
    // m_CurrentFocus->hasFocus = false;
    // m_LastFocus->hasFocus = false;
    m_CurrentFocus->setFocusOff();
    m_LastFocus->setFocusOff();

    ctrl->m_notify();
    Renderer::Render();
    ctrl->m_hasFocus = true;

    m_LastFocus = m_CurrentFocus;

    m_CurrentFocus = ctrl;
}

void FocusManager::toggle() noexcept
{
    m_CurrentFocus->m_hasFocus = !m_CurrentFocus->m_hasFocus;
    m_LastFocus->m_hasFocus = !m_LastFocus->m_hasFocus;

    m_CurrentFocus->m_notify();
    m_LastFocus->m_notify();
}

FocusManager::FocusManager(std::shared_ptr<Focus> current, std::shared_ptr<Focus> last)
    : m_CurrentFocus(std::move(current))
    , m_LastFocus(std::move(last))
{
    m_CurrentFocus->m_FocusManager = this;
    m_LastFocus->m_FocusManager = this;
    m_CurrentFocus->m_hasFocus = true;
}
