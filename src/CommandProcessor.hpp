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
    Command()                           = default;
    Command(const Command &)            = default;
    Command(Command &&)                 = delete;
    Command &operator=(const Command &) = default;
    Command &operator=(Command &&)      = delete;

    virtual ~Command()                  = default;

    virtual bool execute(std::string_view) = 0;
    [[nodiscard]] virtual bool complete(std::vector<std::uint32_t> &) const = 0;
};

struct SearchCommand : public Command
{
    explicit SearchCommand(std::shared_ptr<ListView>, std::shared_ptr<ListView>);
    bool execute(std::string_view) override;
    [[nodiscard]] bool complete(std::vector<std::uint32_t>&) const override;

private:
    std::shared_ptr<ListView> m_ListView;
    std::shared_ptr<ListView> m_SongView;
};

struct Add : public Command
{
    explicit Add(std::shared_ptr<ListView>);
    bool execute(std::string_view) override;
    [[nodiscard]] bool complete(std::vector<std::uint32_t>&) const override;

    std::shared_ptr<ListView> m_ListView;
private:
    enum class Type
    {
        SLASH,
        TILDA,
    };

    void CompletePath(Type, std::vector<std::uint32_t>&) const;
};

struct Play : public Command
{
    explicit Play(std::shared_ptr<ListView>, std::shared_ptr<ListView>);

    bool execute(std::string_view) override;
    [[nodiscard]] bool complete(std::vector<std::uint32_t>&) const override
    { return false; }

private:
    std::shared_ptr<ListView> m_ListView;
    std::shared_ptr<ListView> m_SongView;
};

struct Pause : public Command
{
    bool execute(std::string_view) override;
    [[nodiscard]] bool complete(std::vector<std::uint32_t>&) const override
    { return false; }
};

struct Volup : public Command
{
    bool execute(std::string_view) override;
    [[nodiscard]] bool complete(std::vector<std::uint32_t>&) const override
    { return false; }
};

struct Voldown : public Command
{
    bool execute(std::string_view) override;
    [[nodiscard]] bool complete(std::vector<std::uint32_t>&) const override
    { return false; }
};

struct BindCommand : public Command
{
    explicit BindCommand() = default;
    ~BindCommand() override = default;

    BindCommand(const BindCommand&) = delete;
    BindCommand(BindCommand&&) = delete;

    BindCommand& operator=(const BindCommand&) = delete;
    BindCommand& operator=(BindCommand&&) = delete;

    [[nodiscard]] bool execute(std::string_view) override;
    [[nodiscard]] bool complete(std::vector<std::uint32_t>&) const override;

    void BindKeys();
};

struct Quit : public Command
{
    [[nodiscard]] bool execute(std::string_view) override;
    [[nodiscard]] bool complete(std::vector<std::uint32_t>&) const noexcept override
    { return false; }
};

struct CycleFocus : public Command
{
    explicit CycleFocus(std::shared_ptr<ListView>, std::shared_ptr<ListView>);

    [[nodiscard]] bool execute(std::string_view) override;
    [[nodiscard]] bool complete(std::vector<std::uint32_t>&) const noexcept override
    { return false; }

private:
    std::shared_ptr<ListView> m_ListView;
    std::shared_ptr<ListView> m_SongView;
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
    explicit Clear(std::shared_ptr<ListView>, std::shared_ptr<ListView>);
    bool execute(std::string_view) override;

    [[nodiscard]] bool complete(std::vector<std::uint32_t>&) const noexcept override
    {
        return false;
    }

private:
    std::shared_ptr<ListView> m_ListView;
    std::shared_ptr<ListView> m_SongView;
};

struct Up : public Command
{
    explicit Up(std::shared_ptr<ListView>, std::shared_ptr<ListView>);

    bool execute(std::string_view) override;
    [[nodiscard]] bool complete(std::vector<std::uint32_t>&) const noexcept override
    { return false; }

private:
    std::shared_ptr<ListView> m_ListView;
    std::shared_ptr<ListView> m_SongView;
};

struct Down : public Command
{
    explicit Down(std::shared_ptr<ListView>, std::shared_ptr<ListView>);

    bool execute(std::string_view) override;
    [[nodiscard]] bool complete(std::vector<std::uint32_t>&) const noexcept override
    { return false; }

private:
    std::shared_ptr<ListView> m_ListView;
    std::shared_ptr<ListView> m_SongView;
};

struct Right : public Command
{
    explicit Right(std::shared_ptr<ListView>, std::shared_ptr<ListView>);
    bool execute(std::string_view) override;
    [[nodiscard]] bool complete(std::vector<std::uint32_t>&) const noexcept override
    { return false; }

private:
    std::shared_ptr<ListView> m_ListView;
    std::shared_ptr<ListView> m_SongView;
};

struct Left : public Command
{
    explicit Left(std::shared_ptr<ListView>, std::shared_ptr<ListView>);
    bool execute(std::string_view) override;
    [[nodiscard]] bool complete(std::vector<std::uint32_t>&) const noexcept override
    { return false; }

private:
    std::shared_ptr<ListView> m_ListView;
    std::shared_ptr<ListView> m_SongView;
};

struct CommandProcessor
{
public:
    void registerCommand(std::string_view, std::shared_ptr<Command>) noexcept;

    std::shared_ptr<Command> findCommand(std::string_view cmd) const;
    std::optional<std::string> processCommand(std::string_view CMD);
    std::optional<std::shared_ptr<Command>> getCommandByName(std::string_view);

    [[nodiscard]] std::string getCommandName(std::string_view) const noexcept;
    bool completeNext(std::vector<std::uint32_t>&) const noexcept;

private:
    std::unordered_map<std::string_view, std::shared_ptr<Command>> m_Commands;

    using StrPair = std::pair<std::string, std::string>;
    [[nodiscard]] StrPair processArguments(std::string_view) const noexcept;
    [[nodiscard]] std::string FindNeedle(std::vector<std::string_view>& Haystack, std::string_view needle) const noexcept;
};
