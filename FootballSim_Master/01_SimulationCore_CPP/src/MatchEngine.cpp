#include "MatchEngine.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace fsim {
std::array<double, 10> PlayerAttributes::values() const noexcept {
    return {speed, acceleration, stamina, passing, ball_control, finishing, heading, tackling, positioning, goalkeeping};
}
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
bool bounded(double value, double low, double high) {
    return std::isfinite(value) && value >= low && value <= high;
}
void text_id(const std::string& value) {
    require(!value.empty() && value.size() <= 256 && value.find('\0') == std::string::npos, "ID/name must contain 1..256 UTF-8 bytes without NUL");
}
std::array<Vec2, 11> formation_positions(Formation formation) {
    const auto name = schema::formations.at(static_cast<std::size_t>(formation));
    std::vector<int> rows;
    for (const char c : name) if (c != '-') rows.push_back(c - '0');
    std::array<Vec2, 11> result{};
    result[0] = {-49.0, 0.0};
    std::size_t index = 1;
    for (std::size_t row = 0; row < rows.size(); ++row) {
        const int count = rows[row];
        const double x = -30.0 + static_cast<double>(row) * 55.0 / static_cast<double>(rows.size() - 1);
        for (int i = 0; i < count; ++i) {
            result[index++] = {x, (static_cast<double>(i) - (count - 1) / 2.0) * std::min(15.0, 48.0 / count)};
        }
    }
    return result;
}
} // namespace

MatchEngine::MatchEngine(MatchInput input) : input_(std::move(input)) {
    require(input_.teams[0].id != input_.teams[1].id, "Home and away must be different clubs");
    std::unordered_set<std::string> global_ids;
    for (std::size_t ti = 0; ti < 2; ++ti) {
        const auto& team = input_.teams[ti];
        text_id(team.id); text_id(team.league_id); text_id(team.name);
        require(bounded(team.rating, 0, 100) && bounded(team.fitness, 0, 100), "Team rating/fitness must be finite and in 0..100");
        require(team.players.size() >= 11 && team.players.size() <= 64, "Roster must have 11..64 players");
        const auto& tactics = team.tactics;
        require(static_cast<unsigned>(tactics.formation) < schema::formations.size(), "Unsupported formation");
        require(static_cast<unsigned>(tactics.play_style) < schema::play_styles.size(), "Unsupported play_style");
        require(static_cast<unsigned>(tactics.mentality) < schema::mentalities.size(), "Unsupported mentality");
        require(static_cast<unsigned>(tactics.pressing) < schema::pressing.size() && static_cast<unsigned>(tactics.tempo) < schema::tempos.size(), "Unsupported pressing/tempo");
        require(tactics.pressing_intensity <= 100 && tactics.tempo_intensity <= 100 && tactics.width <= 100, "Tactical numeric values must be in 0..100");
        const auto level = [](std::uint32_t value) { return value <= 33 ? 0U : value <= 66 ? 1U : 2U; };
        require(level(tactics.pressing_intensity) == static_cast<unsigned>(tactics.pressing) &&
                level(tactics.tempo_intensity) == static_cast<unsigned>(tactics.tempo), "Tactical enum disagrees with numeric intensity");
        std::unordered_set<std::string> roster;
        for (const auto& player : team.players) {
            text_id(player.id); text_id(player.name);
            require(roster.insert(player.id).second && global_ids.insert(player.id).second, "Duplicate player ID");
            require(static_cast<unsigned>(player.role) <= static_cast<unsigned>(Role::forward), "Invalid player role");
            for (const auto value : player.attributes.values()) require(bounded(value, 0, 100), "Player attributes must be finite and in 0..100");
            require(bounded(player.source_anchor.x, -52.5, 52.5) && bounded(player.source_anchor.z, -34, 34), "Anchor outside pitch");
        }
        std::unordered_set<std::string> selected;
        for (const auto& id : team.lineup) require(roster.contains(id) && selected.insert(id).second, "Lineup must contain 11 distinct roster IDs");
        for (std::size_t i = 0; i < tactics.specialists.size(); ++i) {
            require(selected.contains(tactics.specialists[i]), "Specialist must be in the starting lineup");
            const auto& backup = tactics.specialist_backups[i];
            require(backup.empty() || (roster.contains(backup) && backup != tactics.specialists[i]), "Invalid specialist backup");
        }
        const auto positions = formation_positions(tactics.formation);
        const double direction = ti == 0 ? 1.0 : -1.0;
        for (std::size_t i = 0; i < 11; ++i) {
            state_.players[ti * 11 + i] = {team.lineup[i], static_cast<std::uint32_t>(ti), static_cast<std::uint32_t>(i),
                                         {positions[i].x * direction, 0.0, positions[i].z * direction}};
            formation_[ti * 11 + i] = state_.players[ti * 11 + i].position;
            const auto found = std::find_if(team.players.begin(), team.players.end(), [&](const auto& p) { return p.id == team.lineup[i]; });
            attributes_[ti * 11 + i] = found->attributes;
        }
    }
    rng_ = input_.seed;
    kickoff(0);
    replay_.record(state_, replay_flags_);
    replay_flags_ = 0;
}
double MatchEngine::random_unit() noexcept {
    // SplitMix64: explicit integer algorithm, independent of STL distributions.
    auto z = (rng_ += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return static_cast<double>((z ^ (z >> 31)) >> 11) * 0x1.0p-53;
}
double MatchEngine::direction(std::uint32_t team) const noexcept {
    const double half = state_.clock.tick >= DualClock::total_ticks / 2 ? -1.0 : 1.0;
    return team == 0 ? half : -half;
}
void MatchEngine::take_possession(std::int32_t player) {
    state_.ball_owner = player; last_touch_ = player; possession_since_ = state_.clock.tick;
    shot_in_flight_ = false;
    cooldown_[static_cast<std::size_t>(player)] = 5;
    const auto& p = state_.players[static_cast<std::size_t>(player)];
    ball_ = {{p.position.x + direction(p.team_index) * 0.55, BallPhysics::radius, p.position.z}, {}};
}
void MatchEngine::kickoff(std::uint32_t team) {
    for (std::size_t i = 0; i < 22; ++i) {
        auto& p = state_.players[i];
        const double dir = direction(p.team_index);
        p.position = {-dir * std::abs(formation_[i].x), 0, formation_[i].z * (state_.clock.tick >= 27000 ? -1 : 1)};
        p.velocity = {}; p.action = AIAction::off_ball;
        decisions_[i] = {AIAction::off_ball, p.position, -1, 0}; cooldown_[i] = 0;
    }
    const auto taker = static_cast<std::int32_t>(team * 11 + 10);
    state_.players[static_cast<std::size_t>(taker)].position = {-direction(team) * 0.55, 0, 0};
    take_possession(taker);
    ball_.position = {0, BallPhysics::radius, 0};
    state_.ball = ball_.position; state_.ball_velocity = {};
    pending_kickoff_ = false; state_.restart_ticks_remaining = 0;
    ++state_.statistics.restarts; replay_flags_ |= replay_restart;
}
void MatchEngine::decide_and_act() {
    ++state_.statistics.ai_slices;
    for (std::size_t i = 0; i < 22; ++i) {
        auto anchor = formation_[i];
        if (state_.clock.tick >= 27000) { anchor.x = -anchor.x; anchor.z = -anchor.z; }
        decisions_[i] = MatchAI::decide(input_, state_, attributes_[i], static_cast<std::uint32_t>(i), anchor, direction(state_.players[i].team_index));
        state_.players[i].action = decisions_[i].action;
        if (decisions_[i].action == AIAction::dribble || decisions_[i].action == AIAction::off_ball)
            ++state_.statistics.actions[static_cast<std::size_t>(decisions_[i].action)];
    }
    // Resolve against the possession at the beginning of this decision slice.
    const auto owner = state_.ball_owner;
    if (owner < 0) return;
    const auto oi = static_cast<std::size_t>(owner);
    for (std::size_t i = 0; i < 22; ++i) {
        if (decisions_[i].action != AIAction::tackle || cooldown_[i] || state_.clock.tick - possession_since_ < 5) continue;
        const auto& p = state_.players[i]; const auto& carrier = state_.players[oi];
        if (p.team_index == carrier.team_index || std::hypot(p.position.x - carrier.position.x, p.position.z - carrier.position.z) > 1.6) continue;
        ++state_.statistics.actions[3]; cooldown_[i] = 12;
        const double chance = std::clamp(0.3 + (attributes_[i].tackling - attributes_[oi].ball_control) * 0.005, 0.08, 0.8);
        if (random_unit() < chance) { take_possession(static_cast<std::int32_t>(i)); return; }
    }
    const auto& action = decisions_[oi];
    if (cooldown_[oi] || state_.clock.tick - possession_since_ < 7 ||
        (action.action != AIAction::short_pass && action.action != AIAction::shoot)) return;
    const auto& attr = attributes_[oi];
    Vec3 target = action.target;
    double speed{};
    if (action.action == AIAction::shoot) {
        const double error = (100 - attr.finishing) * 0.075;
        target.z = (random_unit() * 2 - 1) * (2.0 + error);
        target.y = 0.5 + random_unit() * 1.2;
        speed = 20 + attr.finishing * 0.09;
        shot_in_flight_ = true;
    } else {
        if (action.pass_target < 0) return;
        const auto& receiver = state_.players[static_cast<std::size_t>(action.pass_target)];
        const double error = (100 - attr.passing) * 0.025;
        target.x += receiver.velocity.x * 0.2 + (random_unit() * 2 - 1) * error;
        target.z += receiver.velocity.z * 0.2 + (random_unit() * 2 - 1) * error;
        speed = 13 + attr.passing * 0.07;
        shot_in_flight_ = false;
    }
    const double dx = target.x - ball_.position.x, dz = target.z - ball_.position.z;
    const double length = std::max(0.1, std::hypot(dx, dz));
    ball_.velocity = {dx / length * speed, 0.5, dz / length * speed};
    if (shot_in_flight_) {
        const double flight = length / speed;
        ball_.velocity.y = (target.y - ball_.position.y + 0.5 * BallPhysics::gravity * flight * flight) / flight;
    }
    ++state_.statistics.actions[static_cast<std::size_t>(action.action)];
    state_.ball_owner = -1; last_touch_ = owner; kicked_at_ = state_.clock.tick;
    cooldown_[oi] = 6;
}
void MatchEngine::move_players() {
    std::array<Vec3, 22> previous;
    for (std::size_t i = 0; i < 22; ++i) previous[i] = state_.players[i].position;
    for (std::size_t i = 0; i < 22; ++i) {
        auto& p = state_.players[i];
        auto target = decisions_[i].target;
        for (std::size_t j = 0; j < 22; ++j) if (i != j) {
            const double dx = previous[i].x - previous[j].x, dz = previous[i].z - previous[j].z;
            const double d = std::hypot(dx, dz);
            if (d > 0.001 && d < 0.9) { target.x += dx / d * (0.9 - d); target.z += dz / d * (0.9 - d); }
        }
        const double dx = target.x - p.position.x, dz = target.z - p.position.z;
        const double d = std::hypot(dx, dz);
        double speed = 3.0 + attributes_[i].speed * 0.055;
        if (state_.ball_owner == static_cast<std::int32_t>(i)) speed *= 0.78;
        speed = std::min(speed, d / 0.1);
        Vec3 wanted = d > 1e-8 ? Vec3{dx / d * speed, 0, dz / d * speed} : Vec3{};
        const double dvx = wanted.x - p.velocity.x, dvz = wanted.z - p.velocity.z;
        const double change = std::hypot(dvx, dvz);
        const double factor = change > 0 ? std::min(1.0, (2 + attributes_[i].acceleration * 0.05) * 0.1 / change) : 0;
        p.velocity.x += dvx * factor; p.velocity.z += dvz * factor;
        p.position.x = std::clamp(p.position.x + p.velocity.x * 0.1, -51.5, 51.5);
        p.position.z = std::clamp(p.position.z + p.velocity.z * 0.1, -32.5, 32.5);
        p.velocity.x = (p.position.x - previous[i].x) / 0.1;
        p.velocity.z = (p.position.z - previous[i].z) / 0.1;
    }
}
void MatchEngine::restart_out(const BallStepResult& result) {
    const auto last_team = last_touch_ >= 0 ? state_.players[static_cast<std::size_t>(last_touch_)].team_index : 0U;
    std::uint32_t team = 1 - last_team;
    Vec3 spot{std::clamp(result.crossing.x, -50.5, 50.5), 0, std::clamp(result.crossing.z, -33.0, 33.0)};
    std::int32_t taker = -1;
    if (result.boundary != BallBoundary::touchline) {
        const bool positive = result.boundary == BallBoundary::positive_endline;
        const std::uint32_t attacking = (direction(0) > 0) == positive ? 0U : 1U;
        const std::uint32_t defending = 1 - attacking;
        if (last_team != defending) { team = defending; taker = static_cast<std::int32_t>(team * 11); spot = {-direction(team) * 48, 0, 0}; }
        else { team = attacking; spot = {positive ? 51.0 : -51.0, 0, result.crossing.z >= 0 ? 33.0 : -33.0}; }
    }
    if (taker < 0) {
        double best = 1e9;
        for (std::size_t i = team * 11 + 1; i < team * 11 + 11; ++i) {
            const auto& p = state_.players[i].position;
            const double d = std::hypot(p.x - spot.x, p.z - spot.z);
            if (d < best) { best = d; taker = static_cast<std::int32_t>(i); }
        }
    }
    auto& p = state_.players[static_cast<std::size_t>(taker)];
    p.position = spot; p.velocity = {};
    take_possession(taker);
    state_.restart_ticks_remaining = 10;
    replay_flags_ |= replay_restart; ++state_.statistics.restarts;
}
void MatchEngine::update_free_ball() {
    const auto before = ball_.position;
    const auto result = BallPhysics::step(ball_, 0.1);
    struct Candidate { double t{2}; std::size_t index{}; };
    std::array<Candidate, 22> candidates;
    const double dx = ball_.position.x - before.x, dz = ball_.position.z - before.z;
    const double length2 = dx * dx + dz * dz;
    for (std::size_t i = 0; i < 22; ++i) {
        if (cooldown_[i] || (last_touch_ == static_cast<std::int32_t>(i) && state_.clock.tick - kicked_at_ < 6)) continue;
        const auto& p = state_.players[i];
        const double t = length2 > 1e-9 ? std::clamp(((p.position.x - before.x) * dx + (p.position.z - before.z) * dz) / length2, 0.0, 1.0) : 0;
        const double y = before.y + (ball_.position.y - before.y) * t;
        const bool keeper = p.lineup_index == 0;
        if (y > (keeper ? 2.5 : 1.2)) continue;
        const double reach = keeper ? 1.6 : 1.0;
        if (std::hypot(p.position.x - before.x - dx * t, p.position.z - before.z - dz * t) <= reach) candidates[i] = {t, i};
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) { return a.t != b.t ? a.t < b.t : a.index < b.index; });
    for (const auto& c : candidates) if (c.t <= 1) {
        const bool keeper = state_.players[c.index].lineup_index == 0;
        const double chance = shot_in_flight_ ? (keeper ? 0.3 + attributes_[c.index].goalkeeping * 0.005 : 0.1 + attributes_[c.index].tackling * 0.002)
                                              : 0.4 + attributes_[c.index].ball_control * 0.005;
        cooldown_[c.index] = 10;
        if (random_unit() < chance) {
            if (keeper && shot_in_flight_) ++state_.statistics.saves;
            else ++state_.statistics.interceptions;
            take_possession(static_cast<std::int32_t>(c.index)); return;
        }
    }
    if (result.boundary == BallBoundary::positive_goal || result.boundary == BallBoundary::negative_goal) {
        const bool positive = result.boundary == BallBoundary::positive_goal;
        const std::uint32_t team = (direction(0) > 0) == positive ? 0U : 1U;
        ++state_.score[team];
        state_.last_goal = {state_.last_goal.sequence + 1, state_.clock.tick, team, last_touch_,
                           last_touch_ >= 0 && state_.players[static_cast<std::size_t>(last_touch_)].team_index != team, result.crossing};
        replay_flags_ |= replay_goal;
        kickoff_team_ = 1 - team; pending_kickoff_ = true; state_.restart_ticks_remaining = 20;
        ball_.velocity = {}; shot_in_flight_ = false;
    } else if (result.boundary != BallBoundary::none) restart_out(result);
}
void MatchEngine::step() {
    ++state_.clock.tick;
    for (auto& c : cooldown_) if (c) --c;
    if (state_.clock.tick == 27000) {
        kickoff(1); replay_flags_ |= replay_halftime;
    } else if (state_.restart_ticks_remaining) {
        for (auto& p : state_.players) p.velocity = {};
        --state_.restart_ticks_remaining;
        if (!state_.restart_ticks_remaining && pending_kickoff_) kickoff(kickoff_team_);
    } else {
        if ((state_.clock.tick - 1) % 3 == 0) decide_and_act();
        move_players();
        if (state_.ball_owner >= 0) {
            const auto& p = state_.players[static_cast<std::size_t>(state_.ball_owner)];
            ball_.position = {p.position.x + direction(p.team_index) * 0.55, BallPhysics::radius, p.position.z};
            ball_.velocity = p.velocity;
        } else update_free_ball();
    }
    state_.ball = ball_.position; state_.ball_velocity = ball_.velocity;
    state_.phase = state_.clock.tick == DualClock::total_ticks ? MatchPhase::completed :
                   state_.clock.tick >= 27000 ? MatchPhase::second_half : MatchPhase::first_half;
    if (state_.clock.tick % ReplayBuffer::sample_ticks == 0) { replay_.record(state_, replay_flags_); replay_flags_ = 0; }
}
void MatchEngine::tick_fixed(std::uint32_t tick_count) {
    const auto remaining = std::min<std::uint64_t>(tick_count, DualClock::total_ticks - state_.clock.tick);
    for (std::uint64_t i = 0; i < remaining; ++i) step();
}
} // namespace fsim
