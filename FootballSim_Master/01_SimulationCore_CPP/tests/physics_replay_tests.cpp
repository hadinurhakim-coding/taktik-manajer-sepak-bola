#include "MatchEngine.h"
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

void check(bool result, const char* message) { if (!result) throw std::runtime_error(message); }
void physics() {
    using namespace fsim;
    BallState falling{{0, 1, 0}, {0, -5, 0}};
    const auto bounce = BallPhysics::step(falling, 0.2);
    check(bounce.bounced && falling.position.y >= BallPhysics::radius && falling.velocity.y > 0, "Ground bounce");
    BallState rolling{{0, BallPhysics::radius, 0}, {5, 0, 0}};
    for (int i = 0; i < 100; ++i) BallPhysics::step(rolling, 0.1);
    check(rolling.velocity.x == 0 && rolling.position.y == BallPhysics::radius, "Rolling ball settles");
    BallState right{{51, 0.5, 0}, {150, 0, 0}};
    const auto goal = BallPhysics::step(right, 0.1);
    check(goal.boundary == BallBoundary::positive_goal && std::abs(goal.crossing.x - 52.61) < 1e-9, "Swept full-ball positive goal");
    check(BallPhysics::step(right, 0.1).boundary == BallBoundary::none, "Goal must not repeat beyond line");
    BallState left{{-51, 0.5, 1}, {-100, 0, 0}};
    check(BallPhysics::step(left, 0.1).boundary == BallBoundary::negative_goal, "Negative goal");
    BallState high{{51, 3, 0}, {100, 0, 0}};
    check(BallPhysics::step(high, 0.1).boundary == BallBoundary::positive_endline, "Above crossbar is not goal");
    BallState wide{{51, 0.5, 3.6}, {100, 0, 0}};
    check(BallPhysics::step(wide, 0.1).boundary == BallBoundary::positive_endline, "Ball clipping post is not a goal");
    BallState partial{{52.49, 0.5, 0}, {2, 0, 0}};
    check(BallPhysics::step(partial, 0.01).boundary == BallBoundary::none, "Centre crossing alone is not a goal");
    BallState corner{{52, 0.5, 34}, {100, 0, 100}};
    check(BallPhysics::step(corner, 0.1).boundary == BallBoundary::touchline, "Earliest boundary wins");
    bool invalid = false;
    try { BallPhysics::step(rolling, std::numeric_limits<double>::quiet_NaN()); }
    catch (const std::invalid_argument&) { invalid = true; }
    check(invalid, "Reject invalid physics timestep");
}
void replay() {
    using namespace fsim;
    ReplayBuffer ring;
    MatchSnapshot s;
    for (std::uint64_t tick = 0; tick <= 1800; tick += 3) {
        s.clock.tick = tick; s.ball = {1.25, 0.5, -2}; ring.record(s, tick == 903 ? replay_restart : 0);
    }
    check(ring.size() == 301 && ring.at(0).tick == 900 && ring.at(300).tick == 1800, "Circular overwrite chronological 15 seconds");
    check(ring.at(1).flags == replay_restart, "Replay flags retained");
    const auto data = ring.export_fsim("{}");
    check(data.size() == 40 + 2 + 301 * 412, "Portable binary size");
    const auto u32 = [&](std::size_t offset) {
        return std::uint32_t(data[offset]) | (std::uint32_t(data[offset+1]) << 8) | (std::uint32_t(data[offset+2]) << 16) | (std::uint32_t(data[offset+3]) << 24);
    };
    check(data[0] == 'F' && data[3] == 'M' && u32(8) == 1 && u32(24) == 412, "Magic/version/stride");
    check(u32(42) == 900 && std::bit_cast<float>(u32(42 + 36)) == 1.25F, "Little-endian tick and float offsets");
    bool rejected = false;
    try { ring.record(s, 0); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "Duplicate sample rejected");
}
void ai() {
    using namespace fsim;
    MatchInput input;
    MatchSnapshot s;
    for (std::size_t i = 0; i < 22; ++i) {
        s.players[i].team_index = static_cast<std::uint32_t>(i / 11);
        s.players[i].lineup_index = static_cast<std::uint32_t>(i % 11);
        s.players[i].position = {i < 11 ? -40.0 : 40.0, 0, 25};
    }
    const PlayerAttributes strong{90, 80, 80, 95, 80, 95, 70, 80, 90, 70};
    s.players[10].position = {0, 0, 0}; s.ball = {0, 0.11, 0}; s.ball_owner = 10;
    auto decision = MatchAI::decide(input, s, strong, 10, {25, 0, 0}, 1);
    check(decision.action == AIAction::dribble, "Open field dribble");
    auto slow = strong; slow.speed = 10;
    check(decision.utility > MatchAI::decide(input, s, slow, 10, {25, 0, 0}, 1).utility, "Speed influences dribble utility");
    s.players[9].position = {12, 0, 2};
    input.teams[0].tactics.play_style = PlayStyle::possession;
    decision = MatchAI::decide(input, s, strong, 10, {25, 0, 0}, 1);
    check(decision.action == AIAction::short_pass && decision.pass_target == 9, "Possession utility chooses forward pass");
    input.teams[0].tactics.tempo = Tempo::fast;
    check(MatchAI::decide(input, s, strong, 10, {25, 0, 0}, 1).utility > decision.utility, "Tempo influences passing utility");
    input.teams[0].tactics.tempo = Tempo::normal;
    s.players[10].position = {46, 0, 0}; s.ball.x = 46;
    decision = MatchAI::decide(input, s, strong, 10, {25, 0, 0}, 1);
    check(decision.action == AIAction::shoot, "Finishing utility near goal");
    auto poor = strong; poor.finishing = 20;
    check(decision.utility > MatchAI::decide(input, s, poor, 10, {25, 0, 0}, 1).utility, "Finishing influences shooting utility");
    s.ball_owner = 21; s.players[21].position = {46.5, 0, 0};
    decision = MatchAI::decide(input, s, strong, 10, {25, 0, 0}, 1);
    check(decision.action == AIAction::tackle, "Nearby opponent carrier tackle");
    s.ball_owner = 9;
    auto narrow = MatchAI::decide(input, s, strong, 8, {20, 0, 12}, 1);
    input.teams[0].tactics.width = 100; input.teams[0].tactics.play_style = PlayStyle::wing_play;
    auto wide = MatchAI::decide(input, s, strong, 8, {20, 0, 12}, 1);
    check(wide.action == AIAction::off_ball && wide.target.z > narrow.target.z, "Wing play maintains greater width");
    input.teams[0].tactics.mentality = Mentality::defensive;
    auto defensive = MatchAI::decide(input, s, strong, 5, {-10, 0, 0}, 1);
    input.teams[0].tactics.mentality = Mentality::attacking;
    auto attacking = MatchAI::decide(input, s, strong, 5, {-10, 0, 0}, 1);
    check(attacking.target.x > defensive.target.x, "Mentality changes support line");
}
int main() {
    try { physics(); replay(); ai(); std::cout << "PASS: ball bounce/swept goals, circular FSIM, utility decisions\n"; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
