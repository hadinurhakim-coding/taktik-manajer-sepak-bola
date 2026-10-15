#pragma once
#include "BallPhysics.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace fsim {
struct MatchSnapshot;
enum ReplayFlags : std::uint32_t { replay_goal = 1, replay_restart = 2, replay_halftime = 4, replay_shot = 8, replay_save = 16, replay_big_save = 32 };
struct ReplayFrame {
    std::uint64_t tick{}, goal_sequence{};
    std::array<std::uint32_t, 2> score{};
    std::uint32_t phase{}, flags{};
    std::int32_t ball_owner{-1};
    Vec3 ball{}, ball_velocity{};
    std::array<Vec3, 22> players{};
    std::array<std::uint32_t, 22> actions{};
};
class ReplayBuffer {
public:
    static constexpr std::size_t capacity = 301; // inclusive endpoints: 300 x 0.05 s
    static constexpr std::uint32_t sample_ticks = 3, frame_bytes = 412, header_bytes = 40;
    explicit ReplayBuffer(std::size_t max_frames = capacity);
    void record(const MatchSnapshot&, std::uint32_t flags);
    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] const ReplayFrame& at(std::size_t chronological_index) const;
    // Explicit little-endian IEEE754 binary, never dump native C++ structs.
    [[nodiscard]] std::vector<std::uint8_t> export_fsim(std::string_view metadata_json, std::uint32_t version = 1) const;
private:
    std::vector<ReplayFrame> frames_;
    std::size_t next_{}, count_{}, frame_capacity_{};
};
}
