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

#include "CustomViews.hpp"
#include "util.hpp"

#include <span>
#include <string_view>
#include <unordered_map>

struct Arguments
{
    Arguments(int nb)
        : nbArguments(nb)
    { }

    int nbArguments{};
};

struct Command
{
    virtual ~Command() = default;

    virtual bool execute(std::string_view) = 0;
    [[nodiscard]] virtual bool complete(std::vector<std::uint32_t>&) const = 0;
};

struct SearchCommand : public Command
{
    explicit SearchCommand(ListView&, ListView&);
    bool execute(std::string_view) override;
    [[nodiscard]] bool complete(std::vector<std::uint32_t>&) const override;

private:
    ListView& m_ListView;
    ListView& m_SongView;
};

struct AddCommand : public Command
{
    explicit AddCommand(ListView*);
    bool execute(std::string_view) override;
    [[nodiscard]] bool complete(std::vector<std::uint32_t>&) const override;

private:
    enum class Type
    {
        SLASH,
        TILDA,
    };

    void CompletePath(Type, std::vector<std::uint32_t>&) const;
    ListView* m_ListView;
};

struct HelloWorld : public Command
{
    bool execute(std::string_view s) override
    {
        util::Log("Hello World! {}\n", s);
        return true;
    }

    [[nodiscard]] bool complete(std::vector<std::uint32_t>&) const noexcept override
    {
        return false;
    }
};

struct Clear : public Command
{
    explicit Clear(ListView*, ListView*);
    bool execute(std::string_view) override;

    [[nodiscard]] bool complete(std::vector<std::uint32_t>&) const noexcept override
    {
        return false;
    }

private:
    ListView* m_ListView{};
    ListView* m_SongView{};
};

struct CommandProcessor
{
public:
    void registerCommand(std::string_view, std::shared_ptr<Command>) noexcept;
    std::optional<std::string> processCommand(std::string_view CMD);
    std::optional<std::shared_ptr<Command>> getCommandByName(std::string_view);

    std::string getCommandName(std::string_view) const noexcept;
    bool completeNext(std::vector<std::uint32_t>&) const noexcept;

private:
    using StrPair = std::pair<std::string, std::string>;
    [[nodiscard]] StrPair processArguments(std::string_view) const noexcept;
    [[nodiscard]] std::string FindNeedle(std::vector<std::string_view>& Haystack, std::string_view needle) const noexcept;

    std::unordered_map<std::string_view, std::shared_ptr<Command>> m_Commands;
};
