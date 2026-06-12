/*
 * Copyright (C) 2025 Dāniels Ponamarjovs <bonux@duck.com>
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

#include <optional>
#include <string>
#include <vector>

// ------------------------------------------------------------------
// ServerClient - talks to a tMus-server over its custom REST API.
//
// Turns the terminal player into a client: instead of walking the local
// filesystem, it fetches albums/tracks from the server and hands AudioLoop a
// stream URL (avformat opens http:// natively, with Range-based seeking).
//
// Only std types are exposed here; httplib.h and jsoncpp are confined to the
// .cpp so the heavy single-header HTTP lib does not bleed into every TU.
// ------------------------------------------------------------------

struct RemoteAlbum
{
    std::string id; // opaque albumId from the server
    std::string name;
    std::string artist;
    std::optional<int> year;
    int track_count {};
};

struct RemoteTrack
{
    std::string id; // opaque trackId
    std::string title;
    std::string artist;
    std::string album;
    std::optional<int> track_no;
    std::optional<int> duration_s;
    std::string file_name; // base name of the source file; display fallback for untagged tracks

    // Cue-sheet virtual track: stream_url serves the whole rip file and this
    // track is the [start_s, end_s) window of it.
    std::optional<int> start_s;
    std::optional<int> end_s;

    std::string stream_url; // ready-to-open URL, e.g. http://host:8088/stream/8
};

class ServerClient
{
public:
    // Stores the base URL (e.g. "http://homeserver:8088"), trimming a trailing
    // slash, and verifies the server answers by requesting /library/albums.
    // Throws std::runtime_error if unreachable or the response is not 2xx.
    void connect(const std::string& baseUrl);

    [[nodiscard]] bool connected() const noexcept { return m_connected; }
    [[nodiscard]] const std::string& baseUrl() const noexcept { return m_base; }

    // GET /library/albums -> parsed albums (one request, server-side paged).
    [[nodiscard]] std::vector<RemoteAlbum> albums(int limit = 500, int offset = 0);

    // GET /library/albums/{albumId} -> the album's tracks, each with a
    // fully-formed stream_url.
    [[nodiscard]] std::vector<RemoteTrack> albumTracks(const std::string& albumId);

    // Build the stream URL for a track id without a request.
    [[nodiscard]] std::string streamUrl(const std::string& trackId) const;

private:
    std::string m_base;
    bool m_connected {};
};
