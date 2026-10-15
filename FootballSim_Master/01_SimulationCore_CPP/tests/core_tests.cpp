#include "MatchEngine.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

void check(bool result, const char* message) { if (!result) throw std::runtime_error(message); }
fsim::MatchInput fixture() {
    fsim::MatchInput input;
    input.seed = 18446744073709551615ULL;
    for (std::size_t t = 0; t < 2; ++t) {
        auto& team = input.teams[t];
        team.id = "club_" + std::to_string(t); team.league_id = "league"; team.name = team.id;
        team.rating = 75; team.fitness = 100;
        for (std::size_t p = 0; p < 11; ++p) {
            fsim::Player player;
            player.id = team.id + ":" + std::to_string(p); player.name = "Player";
            player.number = static_cast<std::uint32_t>(p + 1);
            player.role = p == 0 ? fsim::Role::goalkeeper : fsim::Role::midfielder;
            player.attributes = {75, 75, 75, 75, 75, 75, 75, 75, 75, 75};
            team.lineup[p] = player.id;
            team.players.push_back(player);
        }
        team.tactics.specialists.fill(team.lineup[0]);
    }
    return input;
}
template<class F> void invalid(F change) {
    auto input = fixture(); change(input);
    bool rejected = false;
    try { fsim::MatchEngine engine(input); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "Invalid input was accepted");
}
int main() {
    try {
        fsim::MatchEngine engine(fixture());
        check(engine.snapshot().clock.tick == 0, "Initial tick");
        check(engine.snapshot().players[0].position.x == -49.0 && engine.snapshot().players[11].position.x == 49.0, "Home/away formation orientation");
        engine.tick_fixed(1);
        check(std::abs(engine.snapshot().clock.game_seconds() - 0.1) < 1e-12, "One tick game clock");
        check(std::abs(engine.snapshot().clock.real_seconds() * 6.0 - 0.1) < 1e-12, "Dual clock ratio");
        engine.tick_fixed(26998);
        check(engine.snapshot().phase == fsim::MatchPhase::first_half, "Premature halftime");
        engine.tick_fixed(1);
        check(engine.snapshot().phase == fsim::MatchPhase::second_half && engine.snapshot().clock.game_seconds() == 2700.0, "Halftime boundary");
        engine.tick_fixed(std::numeric_limits<std::uint32_t>::max());
        check(engine.snapshot().clock.real_seconds() == 900.0 && engine.snapshot().clock.game_seconds() == 5400.0, "Full-time clamp");
        check(engine.snapshot().phase == fsim::MatchPhase::completed, "Completion phase");
        engine.tick_fixed(100);
        check(engine.snapshot().clock.tick == 54000, "Tick after completion");
        fsim::MatchEngine individual(fixture()), batch(fixture());
        for (int i = 0; i < 54000; ++i) individual.tick_fixed();
        batch.tick_fixed(54000);
        check(individual.snapshot().clock.tick == batch.snapshot().clock.tick, "Tick partition invariance");
        check(individual.replay().export_fsim("{}") == batch.replay().export_fsim("{}"), "Simulation and replay invariant under tick batching");
        check(batch.snapshot().last_goal.sequence == batch.snapshot().score[0] + batch.snapshot().score[1], "Goal sequence agrees with score");
        check(batch.snapshot().last_goal.sequence == batch.snapshot().score[0] + batch.snapshot().score[1], "Goal events match score, including a legitimate 0-0");
        check(batch.archive().size() == 18001 && batch.archive().at(0).tick == 0, "Full archive preserves kickoff through full time");
        for (const auto count : batch.snapshot().statistics.actions) check(count > 0, "Full match exercises all five action categories");
        for (std::size_t formation = 0; formation < fsim::schema::formations.size(); ++formation) {
            auto input = fixture(); input.teams[0].tactics.formation = static_cast<fsim::Formation>(formation);
            fsim::MatchEngine all_formations(input);
            for (const auto& player : all_formations.snapshot().players) check(std::abs(player.position.x) <= 52.5 && std::abs(player.position.z) <= 34, "Formation outside field");
        }
        invalid([](auto& i) { i.teams[0].players[0].attributes.speed = std::numeric_limits<double>::quiet_NaN(); });
        invalid([](auto& i) { i.teams[0].players[0].attributes.stamina = 101; });
        invalid([](auto& i) { i.teams[0].lineup[1] = i.teams[0].lineup[0]; });
        invalid([](auto& i) { i.teams[1].id = i.teams[0].id; });
        invalid([](auto& i) { i.teams[0].tactics.specialists[0] = "missing"; });
        invalid([](auto& i) { i.teams[0].tactics.formation = static_cast<fsim::Formation>(99); });
        invalid([](auto& i) { i.teams[0].tactics.pressing = fsim::Pressing::high; });
        invalid([](auto& i) { i.teams[0].tactics.tempo_intensity = 0; });
        std::cout << "PASS: fixed ticks, 6x clock, halftime, full time, 7 formations, validation\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
