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

#include "CommandProcessor.hpp"
#include "AudioDevice.hpp"
#include "globals.hpp"
#include "util.hpp"

#include <exception>
#include <filesystem>
#include <optional>
#include <pstl/glue_algorithm_defs.h>
#include <string>
#include <ranges>
#include <string_view>
#include <utility>

[[nodiscard]] static ListView::ItemContainer Albums(const std::filesystem::path& path)
{
    ListView::ItemContainer dirs;
    for (const auto& elem : std::filesystem::directory_iterator(path))
    {
        if (elem.is_directory())
        {
            int file_count = 0;
            for (const auto& file : std::filesystem::directory_iterator(elem))
            {
                if (file.is_regular_file())
                {
                    auto ext = file.path().extension();

                    if (ext != ".png" && ext != ".cue" && ext != ".jpg")
                    {
                        file_count++;
                    }
                }
            }

            if (file_count == 0)
                continue;

            unsigned cols = (unsigned)ncstrwidth(elem.path().filename().c_str(), nullptr, nullptr);

            ListView::ItemType item;
            item.first = cols;
            item.second = elem.path();

            dirs.push_back(std::move(item));
        }
    }

    return dirs;
}

SearchCommand::SearchCommand(ListView& listView, ListView& songView)
    : m_ListView(listView)
    , m_SongView(songView)
{ }

bool SearchCommand::execute(std::string_view str)
{
    std::filesystem::path found;
    for (const auto& el : m_ListView.getItems())
    {
        for (const auto& dir : std::filesystem::directory_iterator(el.second))
        {
            if (dir.path().filename().replace_extension().string().find(str) != std::string::npos)
            {
                found = dir.path();
                break;
            }
        }

        if (!found.empty())
            break;
    }

    util::Log(fg(fmt::color::yellow), "Found: {}\n", found.filename().string());

    auto withoutFilename = found.parent_path();
    m_ListView.selectLibrary(withoutFilename);
    m_SongView.selectSong(found);


    if (auto focus = m_SongView.getFocus(); !focus->hasFocus())
    {
        focus->focus();
    }

    m_ListView.draw();
    m_SongView.draw();

    return true;
}

bool SearchCommand::complete([[maybe_unused]] std::vector<std::uint32_t>& vec) const
{
    util::Log("Search command complete\n");
    return true;
}

AddCommand::AddCommand(ListView* listView)
    : m_ListView(listView)
{ }

bool AddCommand::execute(std::string_view str)
{
    util::Log(fg(fmt::color::coral), "AddCommand Received str: {}\n", str);

    std::string intermediate{ str };
    if (!str.empty() && str[0] == '~')
    {
        intermediate.replace(0, 1, std::getenv("HOME"));
    }

    if (!std::filesystem::exists(intermediate))
    {
        return false;
    }

    auto tmp = Albums(intermediate);
    m_ListView->setItems(std::move(tmp));
    m_ListView->SelectionCallback();

    return true;
}

bool AddCommand::complete(std::vector<std::uint32_t>& vec) const
{
    auto count = std::ranges::count(vec, ' ');
    if (count > 1)
        return false;

    auto ret = std::ranges::find(vec, ' ');
    if (ret == vec.end())
    {
        vec.push_back(' ');
        return false;
    }

    util::Log("{}\n", *ret++);
    switch (*ret++)
    {
    case '/':
        util::Log("slash\n");
        CompletePath(Type::SLASH, vec);
        break;
    case '~':
        util::Log("tilda\n");
        CompletePath(Type::TILDA, vec);
        break;
    default:
        util::Log("This should not be possible\n");
        return false;
    }

    return false;
}

static void CommonPart(std::vector<std::uint32_t>& vec, std::string& DesiredDir)
{
    util::Log("Last: {}\n", Globals::lastCompletion.str);

    if (Globals::lastCompletion.str.empty())
    {
        auto slash = DesiredDir.find_last_of('/');
        Globals::lastCompletion.str = DesiredDir.substr(slash + 1);
        if (slash == DesiredDir.length() -1)
        {
            DesiredDir[DesiredDir.length() - 1] = '\0';
            slash = DesiredDir.find_last_of('/');
        }

        DesiredDir.erase(slash + 1);
    }
    else 
    {
        auto slash = DesiredDir.find_last_of('/');
        util::Log("Slash: {}\n", slash);
        if (slash == DesiredDir.length() - 1)
        {
            DesiredDir[DesiredDir.length() - 1] = '\0';
            slash = DesiredDir.find_last_of('/', slash);
            DesiredDir.erase(slash + 1);
        }
        else
        {
            DesiredDir.erase(slash + 1);
        }
    }

    util::Log("Desired: {} {} {}\n", DesiredDir, Globals::lastCompletion.str, Globals::lastCompletion.str.empty());

    std::vector<std::filesystem::directory_iterator::value_type> dirs;
    for (const auto& dir : std::filesystem::directory_iterator(DesiredDir))
    {
        if (!std::filesystem::is_directory(dir))
            continue;

        if (Globals::lastCompletion.str.empty())
        {
            dirs.push_back(dir);
        }
        else if (Globals::lastCompletion.str[0] == dir.path().filename().string()[0])
        {
            dirs.push_back(dir);
        }
    }

    dirs.erase(std::remove_if(dirs.begin(), dirs.end(), [&](const auto& entry)
    {
        return entry.path().filename().string().substr(0, Globals::lastCompletion.str.size()) != Globals::lastCompletion.str;
    }), dirs.end());

    std::ranges::sort(dirs);

    util::Log("Completion candidates\n");
    for (const auto& e : dirs)
    {
        util::Log("Dir: {}\n", e.path().filename().string());
    }

    if (vec.back() == '/')
    {
        vec[vec.size() - 1] = '\0';
    }

    auto sl = std::ranges::find(std::ranges::reverse_view{vec}, '/');
    if (sl != vec.rend())
    {
        vec.erase(sl.base(), std::end(vec));
    }

    // Overflow
    util::Log("----------{} {}\n", Globals::lastCompletion.index, dirs.size());
    if (Globals::lastCompletion.index + 1 > dirs.size())
    {
        Globals::lastCompletion.index = 0;
    }

    for (const auto& e : dirs[Globals::lastCompletion.index++].path().filename().string())
    {
        vec.push_back(e);
    }

    vec.push_back('/');

    if (dirs.size() == 1)
    {
        Globals::lastCompletion.clear();
    }
}

void AddCommand::CompletePath(AddCommand::Type t, std::vector<std::uint32_t>& vec) const
{
    using enum AddCommand::Type;

    auto OneBeforeEnd = vec.end() - 1;
    if (*OneBeforeEnd == ' ')
        return;

    auto ret = std::ranges::find(vec, ' ');
    std::string DesiredDir{ ret + 1, vec.end() };

    std::string env = std::getenv("HOME");
    if (env.empty())
        return;

    if (t == SLASH)
    {
        if (DesiredDir[0] != '/')
        {
            util::Log("Slash exepcted as the firsth character\n");
            return;
        }

        CommonPart(vec, DesiredDir);

    }
    else if (t == TILDA)
    {
        if (DesiredDir[0] != '~')
        {
            util::Log("Tilda character expected as the first one\n");
            return;
        }

        else if (DesiredDir[0] == '~' && DesiredDir[1] != '/')
        {
            vec.push_back('/');
        }


        auto it = std::ranges::find(DesiredDir, '~');
        if (it != DesiredDir.end())
        {
            DesiredDir.erase(it);
            DesiredDir.insert(it, env.begin(), env.end());
        }
        CommonPart(vec, DesiredDir);
    }
    else
    {
        util::Log(fg(fmt::color::red), "Should not be possible\n");
    }
}

Clear::Clear(ListView* l, ListView* s)
    : m_ListView(l)
    , m_SongView(s)
{ }

bool Clear::execute([[maybe_unused]] std::string_view s)
{
    m_ListView->Clear();
    m_SongView->Clear();

    m_ListView->draw();
    m_SongView->draw();

    if (playbackThread.joinable())
    {
        playbackThread.request_stop();
        playbackThread.join();
    }

    return true;
}

void CommandProcessor::registerCommand(std::string_view cmd, std::shared_ptr<Command> obj) noexcept
{
    m_Commands[cmd.data()] = std::move(obj);
}

std::optional<std::string> CommandProcessor::processCommand(std::string_view CMD)
{
    auto [commandName, arguments] = processArguments(CMD);

    util::Log("Processing CMD: '{}' - '{}' and command: '{}'\n", CMD, commandName, arguments);

    auto it = m_Commands.find(commandName);
    if (it != m_Commands.end())
    {
        bool b = it->second->execute(arguments);
        if (b)
        {
            return {};
        }

        return fmt::format("Incorrect argument: {}", arguments);
    }

    return fmt::format("Could not find '{}' command", commandName);
}

std::optional<std::shared_ptr<Command>> CommandProcessor::getCommandByName(std::string_view str)
{
    auto it = m_Commands.find(str);
    if (it != m_Commands.end())
    {
        return it->second;
    }

    return {};
}

bool CommandProcessor::completeNext(std::vector<std::uint32_t>& vec) const noexcept
{
    if (vec.empty())
        return false;

    std::string CMD;

    for (const auto& e : vec | std::views::drop(1))
    {
        CMD.push_back(static_cast<char>(e));
    }

    auto [cmdName, arguments] = processArguments(CMD);

    util::Log("Num of spaces: {}\n", std::ranges::count_if(arguments, [](char ch){ return ch == ' '; }) + 1);

    auto it = m_Commands.find(cmdName);
    if (it != m_Commands.end())
    {
        try
        {
            return it->second->complete(vec);
        }
        catch (std::exception& e)
        {
            util::Log("Exception: {}\n", e.what());
            return false;
        }
    }
    else
    {
        std::vector<std::string_view> Haystack;

        for (const auto& e : m_Commands)
        {
            if (e.first[0] == cmdName[0])
            {
                Haystack.push_back(e.first);
            }
        }

        if (Haystack.size() == 0)
            return false;

        auto result = FindNeedle(Haystack, cmdName);

        vec.clear();
        vec.push_back(':');
        for (const auto& e : result)
        {
            vec.push_back(e);
        }

        vec.push_back(' ');

        util::Log("Needl: {}\n", cmdName);
        return true;
    }
}

std::string CommandProcessor::FindNeedle(std::vector<std::string_view>& Haystack, std::string_view needle) const noexcept
{
    if (Haystack.size() > 1)
    {
        for (const auto& s : Haystack)
        {
            for (std::size_t i = 1; i < s.size(); i++)
            {
                if (needle.size() <= i)
                    break;

                if (needle[i] != s[i])
                    Haystack.erase(std::ranges::find(Haystack, s));
            }
        }

        if (Haystack.size() > 1)
        {
            std::ranges::sort(Haystack);
        }
    }

    util::Log("Closest match: {}\n", Haystack[0]);
    return std::string(Haystack[0]);
}

std::string CommandProcessor::getCommandName(std::string_view CMD) const noexcept
{
    auto hasSpace = CMD.find_first_of(' ');
    if (hasSpace != std::string::npos)
    {
        return { CMD.begin(), hasSpace };
    }
    else
    {
        return { CMD.begin(), CMD.end() };
    }
}

CommandProcessor::StrPair CommandProcessor::processArguments(std::string_view CMD) const noexcept
{
    std::string cmdName = getCommandName(CMD);
    std::string arguments;
    std::ranges::move( CMD | std::views::drop_while([](char ch){ return ch != ' '; }) | std::views::drop(1), std::back_inserter(arguments));
    return { cmdName, arguments };
}
