#include "BallPhysics.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fsim {
BallStepResult BallPhysics::step(BallState& b, double dt) {
    const auto finite = [](Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); };
    if (!std::isfinite(dt) || dt < 0 || dt > 0.25 || !finite(b.position) || !finite(b.velocity))
        throw std::invalid_argument("Ball step requires finite state and dt in 0..0.25 seconds");
    BallStepResult result;
    if (dt == 0) return result;
    const int steps = static_cast<int>(std::ceil(dt / 0.01));
    const double h = dt / steps;
    for (int i = 0; i < steps; ++i) {
        const Vec3 before = b.position;
        if (b.position.y <= radius + 1e-8 && b.velocity.y <= 0) {
            b.position.y = radius; b.velocity.y = 0;
            const double speed = std::hypot(b.velocity.x, b.velocity.z);
            const double factor = speed > 0 ? std::max(0.0, speed - 2.8 * h) / speed : 0;
            b.velocity.x *= factor; b.velocity.z *= factor;
        } else {
            b.velocity.y -= gravity * h;
            b.velocity.x *= std::exp(-0.08 * h); b.velocity.z *= std::exp(-0.08 * h);
        }
        b.position.x += b.velocity.x * h;
        b.position.y += b.velocity.y * h;
        b.position.z += b.velocity.z * h;
        if (b.position.y < radius) {
            b.position.y = radius;
            b.velocity.y = -b.velocity.y * 0.48;
            if (b.velocity.y < 0.6) b.velocity.y = 0;
            result.bounced = true;
        }
        // Select the earliest segment crossing (important near field corners).
        double first = 2;
        BallBoundary boundary = BallBoundary::none;
        Vec3 crossing{};
        const auto check = [&](double start, double end, double limit, BallBoundary out, bool goal_line) {
            if (!(start <= limit && end > limit)) return;
            const double t = (limit - start) / (end - start);
            if (t >= first) return;
            Vec3 p{before.x + (b.position.x - before.x) * t,
                   before.y + (b.position.y - before.y) * t,
                   before.z + (b.position.z - before.z) * t};
            if (goal_line && std::abs(p.z) + radius <= goal_half_width && p.y + radius <= goal_height && p.y >= radius - 1e-8)
                out = p.x < 0 ? BallBoundary::negative_goal : BallBoundary::positive_goal;
            first = t; boundary = out; crossing = p;
        };
        check(before.x, b.position.x, half_length + radius, BallBoundary::positive_endline, true);
        check(-before.x, -b.position.x, half_length + radius, BallBoundary::negative_endline, true);
        check(before.z, b.position.z, half_width + radius, BallBoundary::touchline, false);
        check(-before.z, -b.position.z, half_width + radius, BallBoundary::touchline, false);
        if (boundary != BallBoundary::none) {
            result.boundary = boundary; result.crossing = crossing;
            // Retain a point just outside the plane, so an accidental further
            // step cannot emit the same crossing again. Host handles restart.
            return result;
        }
    }
    return result;
}
}
