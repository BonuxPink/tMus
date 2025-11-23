#pragma once

#include <concepts>
#include <functional>
#include <optional>
#include <chrono>
#include <variant>

enum class PlaybackState : std::uint8_t { Playing, Paused, Stopped };
enum class RepeatMode : std::uint8_t { None, Track, Playlist };

using namespace std::chrono_literals;

namespace audio
{
    struct Track
    {
        std::string title;
        std::string artist;
        std::string album;
        std::chrono::seconds duration;
        std::string file_path;
        std::optional<std::string> artwork_path;

        auto operator<=>(const Track&) const = default;
    };

    using Position = std::chrono::milliseconds;

    struct Volume
    {
        float volume;

        explicit Volume(float v = 0.3f)
            : volume{ std::clamp(v, 0.0f, 1.0f) }
        { }

        auto operator<=>(const Volume&) const = default;
    };
}

namespace commands
{
    struct PlayPause {};
    struct Stop {};
    struct NextTrack {};
    struct PreviousTrack {};
    struct SeekTo { audio::Position position; };
    struct SetVolume { audio::Volume volume; };
    struct LoadPlaylist { std::string path; };
    struct AddToPlaylist { std::string file_path; };
    struct RemoveFromPlaylist { size_t index; };
    struct SetRepeatMode { enum { None, One, All } mode; };
    struct SetShuffleMode { bool enabled; };

    using Command = std::variant<
        PlayPause, Stop, NextTrack, PreviousTrack,
        SeekTo, SetVolume, LoadPlaylist, AddToPlaylist,
        RemoveFromPlaylist, SetRepeatMode, SetShuffleMode
    >;
}

namespace ui
{
    struct ViewState
    {

    };
}

namespace Player
{
    struct PlayerState
    {
        audio::Track current_track;

        audio::Position current_position{ 0 };

        audio::Volume volume_percent;
        bool is_playing{};
    };
}

template<typename T>
concept Frontend = requires(T frontend, const Player::PlayerState& state)

{
    { frontend.render(state) };
    { frontend.poll_action() };
};
