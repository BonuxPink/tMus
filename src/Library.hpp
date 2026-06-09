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

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// ------------------------------------------------------------------
// Track one record in the library
// ------------------------------------------------------------------

struct Track
{
    std::string path;

    std::string title;
    std::string artist;
    std::string album;

    std::optional<int> track_no;
    std::optional<int> duration_s;
    std::optional<int> year;
    std::string format;

    std::int64_t mtime {};
    std::int64_t file_size {};
};

// ------------------------------------------------------------------
// ScanResult - summary returned by the Library::scan()
// ------------------------------------------------------------------

struct ScanResult
{
    int tracks_added {};
    int tracks_skipped {};
    int errors {};
};

// ------------------------------------------------------------------
// Library
// ------------------------------------------------------------------
//
// On-disk format (~/.local/share/tMus/library):
//
//   [/absolute/path/to/track.flac]
//   title=Some Title
//   artist=Some Artist
//   album=Some Album
//   track=1
//   duration=224
//   year=2000
//   format=flac
//   mtime=1718000000
//   size=23431168
//
//   [/absolute/path/to/next.flac]
//   ...
//
// Section name   = absolute path (primary key).
// Blank lines    = ignored (used as visual separators in the file).
// Lines with ';' = comments (stripped by the parser).
// Unknown keys   = silently ignored (forward compatibility).
//
// The file is rewritten atomically on every save() call: written to a
// temporary file first, then rename()'d into place.

class Library
{
public:
    // Constructor without loading - call load() explicitl.
    Library() = default;

    // Loads library from `path`. Creates an empty library if the file does
    // not exist yet. Throws std::runtime_error on parse errors.
    void load(const std::filesystem::path& path);

    // Atomically rewrites the library file at the path given to load().
    // Throws on I/O failure.
    void save() const;

    // ------------------------------------------------------------------
    // Scanning
    // ------------------------------------------------------------------

    // Recursively walks `root`, propes every audio file with FFmpeg, and
    // adds or updates entries in the in-memory store.
    //
    // Skips files whose path is already known AND whose mtime has not changed
    // (fast path - no avformat_open_input call needed).
    //
    // Does NOT call save() - the caller decides when to persist.
    ScanResult scan(const std::filesystem::path& path);

    // ------------------------------------------------------------------
    // Queries
    // ------------------------------------------------------------------

    // Returns one entry per distinct parent directory that contains at least
    // one known track, sorted alphabetically.
    [[nodiscard]] std::vector<std::filesystem::path> album_dirs() const;

    // Returns all tracks whose path starts with `dir/`, ordered by
    // track_no then path.
    [[nodiscard]] std::vector<Track> tracks_in_dir(const std::filesystem::path& dir) const;

    [[nodiscard]] std::size_t size() const noexcept
    {
        return tracks.size();
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return tracks.empty();
    }

private:
    // Probes a single file with avformat and returns a populated Track.
    // Returns std::nullopt if the file is not a readable audio file.
    static std::optional<Track> probe(const std::filesystem::path& p);

    // Path -> Track, insertion-ordered via a parallel vector so save() is stable.
    std::unordered_map<std::string, std::size_t> index;
    std::vector<Track> tracks;
    std::filesystem::path path;
};

// ------------------------------------------------------------------
// Free helpers
// ------------------------------------------------------------------

// Returns true for file extensions we consider audio.
[[nodiscard]] bool is_audio_file(const std::filesystem::path& p) noexcept;
