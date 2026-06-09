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

#include "ServerClient.hpp"

#include <format>
#include <sstream>
#include <stdexcept>

#include "third_party/httplib.h"
#include <json/json.h>

namespace
{
    // Split a base URL into the scheme://host:port prefix httplib::Client wants
    // and verify it carries a scheme. Returns the cleaned prefix.
    std::string clean_base(std::string url)
    {
        while (not url.empty() and url.back() == '/')
            url.pop_back();
        if (url.find("://") == std::string::npos)
            throw std::runtime_error(
                std::format("server URL must include a scheme (http://...): '{}'", url));
        return url;
    }

    Json::Value parse(const std::string& body)
    {
        Json::CharReaderBuilder builder;
        Json::Value root;
        std::string errs;
        std::istringstream ss { body };
        if (not Json::parseFromStream(builder, ss, &root, &errs))
            throw std::runtime_error(std::format("invalid JSON from server: {}", errs));
        return root;
    }

    // GET `path` from `base`, throwing a descriptive error on transport failure
    // or a non-2xx status. Returns the parsed JSON body.
    Json::Value get_json(const std::string& base, const std::string& path)
    {
        httplib::Client cli { base };
        cli.set_connection_timeout(5, 0);
        cli.set_read_timeout(10, 0);

        auto res = cli.Get(path);
        if (not res)
            throw std::runtime_error(
                std::format("cannot reach {}{} ({})", base, path,
                            httplib::to_string(res.error())));
        if (res->status / 100 != 2)
            throw std::runtime_error(
                std::format("{}{} returned HTTP {}", base, path, res->status));
        return parse(res->body);
    }

    std::optional<int> opt_int(const Json::Value& v, const char* key)
    {
        if (v.isMember(key) and v[key].isIntegral())
            return v[key].asInt();
        return std::nullopt;
    }
} // namespace

void ServerClient::connect(const std::string& baseUrl)
{
    std::string base = clean_base(baseUrl);

    // Probe the API so we fail fast on a bad address rather than at first browse.
    (void)get_json(base, "/library/albums?limit=1");

    m_base      = std::move(base);
    m_connected = true;
}

std::string ServerClient::streamUrl(const std::string& trackId) const
{
    return std::format("{}/stream/{}", m_base, trackId);
}

std::vector<RemoteAlbum> ServerClient::albums(int limit, int offset)
{
    auto root = get_json(m_base, std::format("/library/albums?limit={}&offset={}", limit, offset));

    std::vector<RemoteAlbum> out;
    for (const auto& a : root["items"])
    {
        RemoteAlbum album;
        album.id          = a["id"].asString();
        album.name        = a["name"].asString();
        album.artist      = a.get("artist", "").asString();
        album.year        = opt_int(a, "year");
        album.track_count = a.get("trackCount", 0).asInt();
        out.push_back(std::move(album));
    }
    return out;
}

std::vector<RemoteTrack> ServerClient::albumTracks(const std::string& albumId)
{
    auto root = get_json(m_base, std::format("/library/albums/{}", albumId));

    std::vector<RemoteTrack> out;
    for (const auto& t : root["tracks"])
    {
        RemoteTrack track;
        track.id         = t["id"].asString();
        track.title      = t.get("title", "").asString();
        track.artist     = t.get("artist", "").asString();
        track.album      = t.get("album", "").asString();
        track.track_no   = opt_int(t, "trackNo");
        track.duration_s = opt_int(t, "durationSec");
        track.stream_url = streamUrl(track.id);
        out.push_back(std::move(track));
    }
    return out;
}
