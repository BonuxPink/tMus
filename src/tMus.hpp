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

#include <array>
#include <memory>

#include "CommandView.hpp"
#include "Config.hpp"

class tMus
{
public:
    tMus(const std::shared_ptr<FocusManager>&);
    void loop();

    static void Init();
    static void InitLog();

    using ViewLike = std::variant<std::shared_ptr<CommandView>, std::shared_ptr<ListView>>;

#ifdef DEBUG
    [[nodiscard]] auto& GetViewRef() const noexcept(true)
    { return m_views[0]; }
#endif

private:
    std::array<ViewLike, 3> m_views;
    std::shared_ptr<Config> cfg;
};
