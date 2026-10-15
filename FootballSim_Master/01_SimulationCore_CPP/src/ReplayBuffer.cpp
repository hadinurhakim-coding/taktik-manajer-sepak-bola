#include "ReplayBuffer.h"
#include "MatchEngine.h"
#include <bit>
#include <limits>
#include <stdexcept>

namespace fsim {
ReplayBuffer::ReplayBuffer() : frames_(capacity) {}
void ReplayBuffer::record(const MatchSnapshot& s, std::uint32_t flags) {
    if (s.clock.tick % sample_ticks != 0) throw std::invalid_argument("Replay sample must be on the three-tick grid");
    if (count_ && s.clock.tick != at(count_ - 1).tick + sample_ticks) throw std::invalid_argument("Replay samples must be consecutive");
    auto& f = frames_[next_];
    f.tick = s.clock.tick; f.goal_sequence = s.last_goal.sequence; f.score = s.score;
    f.phase = static_cast<std::uint32_t>(s.phase); f.flags = flags; f.ball_owner = s.ball_owner;
    f.ball = s.ball; f.ball_velocity = s.ball_velocity;
    for (std::size_t i = 0; i < 22; ++i) { f.players[i] = s.players[i].position; f.actions[i] = static_cast<std::uint32_t>(s.players[i].action); }
    next_ = (next_ + 1) % capacity;
    if (count_ < capacity) ++count_;
}
const ReplayFrame& ReplayBuffer::at(std::size_t index) const {
    if (index >= count_) throw std::out_of_range("Replay frame index");
    return frames_[(next_ + capacity - count_ + index) % capacity];
}
std::vector<std::uint8_t> ReplayBuffer::export_fsim(std::string_view metadata) const {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
    if (metadata.size() > 1024 * 1024) throw std::invalid_argument("Replay metadata exceeds 1 MiB");
    std::vector<std::uint8_t> bytes;
    bytes.reserve(header_bytes + metadata.size() + count_ * frame_bytes);
    const auto u32 = [&](std::uint32_t n) { for (int shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<std::uint8_t>(n >> shift)); };
    const auto u64 = [&](std::uint64_t n) { for (int shift = 0; shift < 64; shift += 8) bytes.push_back(static_cast<std::uint8_t>(n >> shift)); };
    const auto vec = [&](Vec3 v) { for (double n : {v.x, v.y, v.z}) u32(std::bit_cast<std::uint32_t>(static_cast<float>(n))); };
    for (auto b : std::array<std::uint8_t, 8>{'F','S','I','M',13,10,26,10}) bytes.push_back(b);
    u32(1); u32(header_bytes); u32(static_cast<std::uint32_t>(metadata.size()));
    u32(static_cast<std::uint32_t>(count_)); u32(frame_bytes); u32(sample_ticks); u32(60); u32(10);
    bytes.insert(bytes.end(), metadata.begin(), metadata.end());
    for (std::size_t i = 0; i < count_; ++i) {
        const auto& f = at(i);
        u64(f.tick); u32(f.score[0]); u32(f.score[1]); u32(f.phase); u32(f.flags);
        u64(f.goal_sequence); u32(static_cast<std::uint32_t>(f.ball_owner));
        vec(f.ball); vec(f.ball_velocity);
        for (std::size_t p = 0; p < 22; ++p) { vec(f.players[p]); u32(f.actions[p]); }
    }
    return bytes;
}
}
