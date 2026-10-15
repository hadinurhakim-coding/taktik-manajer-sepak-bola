#include "MatchAI.h"
#include "MatchEngine.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace fsim {
namespace {
double distance(Vec3 a, Vec3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
Vec3 field(Vec3 p) { return {std::clamp(p.x, -51.5, 51.5), 0, std::clamp(p.z, -32.5, 32.5)}; }
}
AIDecision MatchAI::decide(const MatchInput& input, const MatchSnapshot& s,
                          const PlayerAttributes& a, std::uint32_t index, Vec3 anchor, double dir) {
    const auto& p = s.players.at(index);
    const auto& t = input.teams[p.team_index].tactics;
    const auto team = p.team_index;
    const auto owner = s.ball_owner;
    const bool keeper = p.lineup_index == 0;
    const bool has_ball = owner == static_cast<std::int32_t>(index);
    const bool team_ball = owner >= 0 && s.players[static_cast<std::size_t>(owner)].team_index == team;
    const double mentality = t.mentality == Mentality::attacking ? 1 : t.mentality == Mentality::defensive ? -1 : 0;
    const double tempo = t.tempo == Tempo::fast ? 1 : t.tempo == Tempo::slow ? -1 : 0;
    double pressure = 50;
    Vec3 nearest{};
    for (const auto& other : s.players) if (other.team_index != team) {
        const double d = distance(p.position, other.position);
        if (d < pressure) { pressure = d; nearest = other.position; }
    }
    if (has_ball) {
        AIDecision best{AIAction::dribble, field({p.position.x + dir * 12, 0, p.position.z}), -1,
                        keeper ? -1.0 : 0.42 + a.speed * 0.003 + std::min(pressure, 8.0) * 0.025 + mentality * 0.06};
        if (t.play_style == PlayStyle::counter_attack) best.utility += 0.14;
        if (pressure < 5) best.target.z = std::clamp(p.position.z + (p.position.z >= nearest.z ? 4.0 : -4.0), -30.0, 30.0);
        for (std::size_t j = 0; j < s.players.size(); ++j) {
            const auto& other = s.players[j];
            if (j == index || other.team_index != team || other.lineup_index == 0) continue;
            const double d = distance(p.position, other.position);
            if (d < 4 || d > 32) continue;
            double clearance = 20;
            for (const auto& defender : s.players) if (defender.team_index != team)
                clearance = std::min(clearance, distance(other.position, defender.position));
            const double forward = dir * (other.position.x - p.position.x);
            const double utility = 0.23 + a.passing * 0.004 + forward * 0.016 + std::min(clearance, 10.0) * 0.025 - d * 0.009
                + (pressure < 3 ? 0.25 : 0) + (t.play_style == PlayStyle::possession ? 0.24 : 0)
                + (t.play_style == PlayStyle::direct_pass ? std::max(0.0, forward) * 0.009 : 0) + tempo * 0.04;
            if (utility > best.utility) best = {AIAction::short_pass, other.position, static_cast<std::int32_t>(j), utility};
        }
        const double goal_distance = std::hypot(52.5 - dir * p.position.x, p.position.z);
        if (!keeper && goal_distance < 32) {
            const double shoot = 0.08 + a.finishing * 0.005 + (1 - goal_distance / 32) * 1.15
                - std::abs(p.position.z) * 0.007 + mentality * 0.12 + tempo * 0.04;
            if (shoot > best.utility) best = {AIAction::shoot, {dir * 54, 1.2, 0}, -1, shoot};
        }
        // A keeper always releases the ball, even if all teammates are distant.
        if (keeper && best.action == AIAction::dribble) {
            const auto teammate = static_cast<std::int32_t>(team * 11 + 5);
            best = {AIAction::short_pass, s.players[static_cast<std::size_t>(teammate)].position, teammate, 1};
        }
        return best;
    }
    if (keeper) {
        return {AIAction::off_ball, {-dir * 49.0, 0, std::clamp(s.ball.z * 0.35, -3.2, 3.2)}, -1, 1};
    }
    const Vec3 carrier = owner >= 0 ? s.players[static_cast<std::size_t>(owner)].position : s.ball;
    const double ball_distance = distance(p.position, carrier);
    int nearer = 0;
    for (std::size_t j = team * 11 + 1; j < team * 11 + 11; ++j) {
        const double d = distance(s.players[j].position, carrier);
        if (d < ball_distance || (d == ball_distance && j < index)) ++nearer;
    }
    if (!team_ball) {
        const int pressers = t.pressing == Pressing::high || t.play_style == PlayStyle::high_press ? 3 : 2;
        if (nearer < (owner < 0 ? 2 : pressers)) {
            Vec3 target = carrier;
            if (owner < 0) { target.x += s.ball_velocity.x * 0.15; target.z += s.ball_velocity.z * 0.15; }
            return {owner >= 0 && ball_distance < 1.8 ? AIAction::tackle : AIAction::off_ball, field(target), -1,
                    0.5 + a.positioning * 0.005};
        }
    }
    // Shape and support: positioning improves response to the ball; width and
    // wing play spread support players rather than sending everyone to it.
    const double reaction = 0.25 + a.positioning * 0.003;
    Vec3 target = anchor;
    target.x += s.ball.x * reaction + dir * (mentality * 4 + (team_ball ? 6 : -4));
    target.z *= 0.65 + t.width * 0.007 + (t.play_style == PlayStyle::wing_play ? 0.2 : 0);
    if (team_ball && p.lineup_index >= 7) target.x = s.ball.x + dir * (7 + (p.lineup_index % 3) * 4);
    target.z += s.ball.z * 0.12;
    return {AIAction::off_ball, field(target), -1, 0.4 + a.positioning * 0.004};
}
}
