#pragma once
#include "BallPhysics.h"
#include <array>
#include <cstdint>
#include <string_view>

namespace fsim {
struct MatchInput;
struct MatchSnapshot;
struct PlayerAttributes;
enum class AIAction : std::uint8_t { short_pass, dribble, shoot, tackle, off_ball };
inline constexpr std::array<std::string_view, 5> ai_action_names = {"short_pass", "dribble", "shoot", "tackle", "off_ball_movement"};
struct AIDecision {
    AIAction action{AIAction::off_ball};
    Vec3 target{};
    std::int32_t pass_target{-1};
    double utility{};
};
class MatchAI {
public:
    // Pure utility evaluation: all players see the same pre-movement state.
    static AIDecision decide(const MatchInput&, const MatchSnapshot&,
                             const PlayerAttributes&, std::uint32_t player,
                             Vec3 formation_anchor, double attack_direction);
};
}
