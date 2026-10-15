#pragma once
#include <cstdint>

namespace fsim {
struct Vec2 { double x{}, z{}; };
struct Vec3 { double x{}, y{}, z{}; };
struct BallState { Vec3 position{0, 0.11, 0}, velocity{}; };
enum class BallBoundary : std::uint8_t { none, negative_goal, positive_goal, negative_endline, positive_endline, touchline };
struct BallStepResult { BallBoundary boundary{BallBoundary::none}; Vec3 crossing{}; bool bounced{}; };
class BallPhysics {
public:
    static constexpr double radius = 0.11, half_length = 52.5, half_width = 34.0;
    static constexpr double goal_half_width = 3.66, goal_height = 2.44, gravity = 9.81;
    // Seconds and metres in GAME time; swept crossing uses the entire ball.
    // Boundary events are emitted once per outward crossing, never for a ball
    // already beyond the boundary. Ground contact is substepped at <= 0.01 s.
    static BallStepResult step(BallState& ball, double seconds);
};
}
