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

#include "Library.hpp"
#include "IniParser.hpp"
#include "util.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <set>
#include <stdexcept>
#include <system_error>

#include <sys/stat.h>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
}

// ------------------------------------------------------------------
// is_audio_file
// ------------------------------------------------------------------

bool is_audio_file(const std::filesystem::path& p) noexcept
{
    const auto ext { p.extension() };
    return ext == ".flac" || ext == ".mp3" || ext == ".ogg" || ext == ".opus" || ext == ".m4a"
        || ext == ".aac" || ext == ".wav" || ext == ".wv" || ext == ".ape";
}

// ------------------------------------------------------------------
// Internal parsing helpers
// ------------------------------------------------------------------

namespace
{
    std::string_view trim(std::string_view s)
    {
        auto start = s.find_first_not_of(" \t\r\n");
        if (start == std::string_view::npos)
            return {};

        auto end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    // Strip comment suffic (everything from ';' onward), then trim.
    std::string_view strip_comment(std::string_view s)
    {
        if (auto pos = s.find(';'); pos != std::string_view::npos)
            s = s.substr(0, pos);

        return trim(s);
    }

    std::optional<int> parse_int(std::string_view s)
    {
        // Handle "4/12" style track numbers - take only the part before '/'.
        auto slash = s.find('/');
        if (slash != std::string_view::npos)
            s = s.substr(0, slash);

        s = trim(s);
        if (s.empty())
            return std::nullopt;

        int v {};
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
        if (ec != std::errc {})
            return std::nullopt;

        return v;
    }

} // namespace

// ------------------------------------------------------------------
// Library::load
// ------------------------------------------------------------------

void Library::load(const std::filesystem::path& in_path)
{
    path = in_path;
    tracks.clear();
    index.clear();

    if (not std::filesystem::exists(path))
    {
        util::Log("Library: no existing library file at {}, starting fresh\n", path.string());
        return;
    }

    IniParser parser { path.string(), ParseOptions { .strip_comments = false } };
    for (const auto& section : parser.sections())
    {
        Track t;
        t.path = section.name;

        for (const auto& [key, val] : section.entries)
        {
            if (key == "title")
            {
                t.title = val.as_string();
            }

            else if (key == "artist")
            {
                t.artist = val.as_string();
            }

            else if (key == "album")
            {
                t.album = val.as_string();
            }

            else if (key == "format")
            {
                t.format = val.as_string();
            }

            else if (key == "track")
            {
                t.track_no = val.as<int>();
            }

            else if (key == "duration")
            {
                t.duration_s = val.as<int>();
            }

            else if (key == "year")
            {
                t.year = val.as<int>();
            }

            else if (key == "mtime")
            {
                t.mtime = val.as<int>();
            }

            else if (key == "size")
            {
                t.file_size = val.as<int>();
            }
        }

        index[t.path] = tracks.size();
        tracks.push_back(std::move(t));
    }

    util::Log(color::green, "Library: loaded {} tracks from {}\n", tracks.size(), path.string());
}

// ------------------------------------------------------------------
// Library::save
// ------------------------------------------------------------------

void Library::save() const
{
    if (path.empty())
        throw std::runtime_error("Library::save called before load");

    // Write to a temp file first, then atomically rename into place.
    auto tmp_path = path;
    tmp_path += ".tmp";

    {
        std::ofstream out { tmp_path };
        if (not out)
            throw std::runtime_error(
                std::format("Library::save: cannot open '{}' for writing", tmp_path.string()));

        for (const auto& t : tracks)
        {
            out << '[' << t.path << "]\n";
            if (not t.title.empty())
                out << "title=" << t.title << '\n';
            if (not t.artist.empty())
                out << "artist=" << t.artist << '\n';
            if (not t.album.empty())
                out << "album=" << t.album << '\n';
            if (not t.format.empty())
                out << "format=" << t.format << '\n';
            if (t.track_no)
                out << "track=" << *t.track_no << '\n';
            if (t.duration_s)
                out << "duration=" << *t.duration_s << '\n';
            if (t.year)
                out << "year=" << *t.year << '\n';
            if (t.mtime)
                out << "mtime=" << t.mtime << '\n';
            if (t.file_size)
                out << "size=" << t.file_size << '\n';
            out << '\n'; // blank line separator between records
        }
    }

    std::filesystem::rename(tmp_path, path);
}

// ------------------------------------------------------------------
// Library::save
// ------------------------------------------------------------------

std::optional<Track> Library::probe(const std::filesystem::path& p)
{
    AVFormatContext* ctx {};
    if (avformat_open_input(&ctx, p.c_str(), nullptr, nullptr) < 0)
        return std::nullopt;

    auto ctx_guard = std::unique_ptr<AVFormatContext, decltype([](AVFormatContext* c){ avformat_close_input(&c); })>{ ctx };

    if (avformat_find_stream_info(ctx, nullptr) < 0)
        return std::nullopt;

    // Requires at least one audio stream.
    bool has_audio {};
    for (unsigned i = 0; i < ctx->nb_streams; ++i)
    {
        if (ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            has_audio = true;
            break;
        }
    }

    if (not has_audio)
        return std::nullopt;

    auto get_tag = [&](const char* key) -> std::string
    {
        auto* e = av_dict_get(ctx->metadata, key, nullptr, AV_DICT_IGNORE_SUFFIX);
        return (e and e->value) ? std::string { e->value } : std::string {};
    };

    Track t;
    t.path   = p.string();
    t.title  = get_tag("title");
    t.artist = get_tag("artist");
    t.album  = get_tag("album");
    t.format = p.extension().string();
    if (not t.format.empty() and t.format[0] == '.')
        t.format.erase(0, 1);

    if (ctx->duration > 0)
        t.duration_s = static_cast<int>(ctx->duration / AV_TIME_BASE);

    // track number - may be "4/12"
    {
        auto raw = get_tag("track");
        if (not raw.empty())
            t.track_no = parse_int(raw);
    }

    // year - stored as "date" tag in most formats
    {
        auto raw = get_tag("date");
        if (not raw.empty())
        {
            // "date" may be "2000", "2000-01-01" or "2000-01" - take first 4 chars.
            if (raw.size() >= 4)
                t.year = parse_int(raw.substr(0, 4));
        }
    }

    // Filesystem metadata
    std::error_code ec;
    auto sz = std::filesystem::file_size(p, ec);
    if (not ec)
        t.file_size = static_cast<std::int64_t>(sz);

    // mtime via stat
    struct stat st {};
    if (::stat(p.c_str(), &st) == 0)
        t.mtime = static_cast<std::int64_t>(st.st_mtime);

    return t;
}

// ---------------------------------------------------------------------------
// Library::scan
// ---------------------------------------------------------------------------

ScanResult Library::scan(const std::filesystem::path& root)
{
    ScanResult result {};

    if (not std::filesystem::exists(root))
    {
        util::Log(color::indian_red, "Library::scan: path does not exists: {}\n", root.string());
        return result;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(
             root, std::filesystem::directory_options::skip_permission_denied))
    {
        if (not entry.is_regular_file())
            continue;

        const auto& p = entry.path();
        if (not is_audio_file(p))
            continue;

        // Fast path: if we already know this file and mtime hasn't changed, skip.
        if (auto it = index.find(p.string()); it != index.end())
        {
            const auto& existing = tracks[it->second];
            struct stat st {};
            if (::stat(p.c_str(), &st) == 0 and static_cast<std::int64_t>(st.st_mtime) == existing.mtime)
            {
                ++result.tracks_skipped;
                continue;
            }
        }

        auto t = probe(p);
        if (not t.has_value())
        {
            util::Log(color::green_yellow, "Library::scan: skipping unreachable: {}\n", p.string());
            ++result.errors;
            continue;
        }

        // Upsert into the in-memory store.
        if (auto it = index.find(t->path); it != index.end())
        {
            tracks[it->second] = *t;
        }
        else
        {
            index[t->path] = tracks.size();
            tracks.push_back(std::move(*t));
        }

        ++result.tracks_added;
    }

    util::Log(color::green,
              "Library::scan: {} added/updated, {} skipped, {} errors\n",
              result.tracks_added,
              result.tracks_skipped,
              result.errors);

    return result;
}

// ---------------------------------------------------------------------------
// Library::album_dirs
// ---------------------------------------------------------------------------

std::vector<std::filesystem::path> Library::album_dirs() const
{
    // Use a set for deduplication + automatic sort.
    std::set<std::filesystem::path> dirs;

    for (const auto& t : tracks)
    {
        std::filesystem::path p { t.path };
        if (p.has_parent_path())
            dirs.insert(p.parent_path());
    }

    return { dirs.begin(), dirs.end() };
}

// ---------------------------------------------------------------------------
// Library::tracks_in_dir
// ---------------------------------------------------------------------------

std::vector<Track> Library::tracks_in_dir(const std::filesystem::path& dir) const
{
    std::vector<Track> result;
    const auto dir_str = dir.string() + '/';

    for (const auto& t : tracks)
    {
        if (t.path.starts_with(dir_str))
            result.push_back(t);
    }

    std::ranges::sort(result,
                      [](const Track& a, const Track& b)
                      {
                          if (a.track_no and b.track_no and *a.track_no != *b.track_no)
                              return *a.track_no < *b.track_no;

                          return a.path < b.path;
                      });

    return result;
}
