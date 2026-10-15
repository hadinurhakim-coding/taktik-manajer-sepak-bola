#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "BallPhysics.h"
#include "MatchAI.h"
#include "ReplayBuffer.h"

namespace fsim {
namespace schema {
inline constexpr std::array<std::string_view, 10> attributes = {"speed", "acceleration", "stamina", "passing", "ball_control", "finishing", "heading", "tackling", "positioning", "goalkeeping"};
inline constexpr std::array<std::string_view, 7> formations = {"4-2-3-1", "4-3-3", "4-4-2", "3-5-2", "3-4-2-1", "3-4-1-2", "5-3-2"};
inline constexpr std::array<std::string_view, 6> play_styles = {"balanced", "possession", "counter_attack", "high_press", "wing_play", "direct_pass"};
inline constexpr std::array<std::string_view, 3> mentalities = {"defensive", "balanced", "attacking"};
inline constexpr std::array<std::string_view, 3> pressing = {"low", "medium", "high"};
inline constexpr std::array<std::string_view, 3> tempos = {"slow", "normal", "fast"};
inline constexpr std::array<std::string_view, 5> specialists = {"captain", "penalty_taker", "free_kick_taker", "corner_left_taker", "corner_right_taker"};
inline constexpr std::array<std::string_view, 8> on_ball = {"short_pass", "one_two_pass", "dribble", "cross", "header", "shot", "long_shot", "tackle_duel"};
inline constexpr std::array<std::string_view, 5> off_ball = {"provide_pass_option", "maintain_width", "support_attack", "press_carrier", "cover_space"};
inline constexpr std::array<std::string_view, 7> goalkeeper = {"ready_stance", "adjust_position", "dive_left", "dive_right", "catch_high", "catch_low", "distribute_ball"};
inline constexpr std::array<std::string_view, 6> set_pieces = {"kick_off", "corner_kick", "throw_in", "free_kick", "penalty_kick", "goal_kick"};
} // namespace schema

enum class Formation : std::uint8_t { f4231, f433, f442, f352, f3421, f3412, f532 };
enum class PlayStyle : std::uint8_t { balanced, possession, counter_attack, high_press, wing_play, direct_pass };
enum class Mentality : std::uint8_t { defensive, balanced, attacking };
enum class Pressing : std::uint8_t { low, medium, high };
enum class Tempo : std::uint8_t { slow, normal, fast };
enum class Specialist : std::uint8_t { captain, penalty_taker, free_kick_taker, corner_left_taker, corner_right_taker };
enum class Role : std::uint8_t { goalkeeper, defender, midfielder, forward };
enum class OnBallAction : std::uint8_t { short_pass, one_two_pass, dribble, cross, header, shot, long_shot, tackle_duel };
enum class OffBallAction : std::uint8_t { provide_pass_option, maintain_width, support_attack, press_carrier, cover_space };
enum class GoalkeeperAction : std::uint8_t { ready_stance, adjust_position, dive_left, dive_right, catch_high, catch_low, distribute_ball };
enum class SetPiece : std::uint8_t { kick_off, corner_kick, throw_in, free_kick, penalty_kick, goal_kick };


struct PlayerAttributes {
    double speed{}, acceleration{}, stamina{}, passing{}, ball_control{};
    double finishing{}, heading{}, tackling{}, positioning{}, goalkeeping{};
    [[nodiscard]] std::array<double, 10> values() const noexcept;
};

struct Player {
    std::string id, name; // preserve SQLite string IDs, never hash them into numeric IDs
    std::uint32_t number{};
    Role role{Role::midfielder};
    std::string primary_position;
    std::vector<std::string> secondary_positions;
    Vec2 source_anchor{};
    PlayerAttributes attributes{};
};

struct Tactics {
    Formation formation{Formation::f4231};
    PlayStyle play_style{PlayStyle::balanced};
    Mentality mentality{Mentality::balanced};
    Pressing pressing{Pressing::medium};
    Tempo tempo{Tempo::normal};
    // Preserve old 0..100 values alongside the new three-level enums.
    std::uint32_t pressing_intensity{50}, tempo_intensity{50}, width{50};
    std::array<std::string, 5> specialists{}; // main assignments: active lineup IDs
    std::array<std::string, 5> specialist_backups{}; // optional roster IDs
};

struct TeamInput {
    std::string id, league_id, name, short_name;
    double rating{}, fitness{};
    std::uint32_t club_revision{};
    Tactics tactics{};
    std::vector<Player> players; // full roster, including bench
    std::array<std::string, 11> lineup{}; // ordered selection from players
};

struct MatchInput {
    std::uint64_t seed{}; // deterministic PRNG for kick error, tackles and catches
    std::array<TeamInput, 2> teams; // home, away
};

struct DualClock {
    static constexpr std::uint64_t ticks_per_real_second = 60;
    static constexpr std::uint64_t real_duration_seconds = 900;
    static constexpr std::uint64_t game_duration_seconds = 5400;
    static constexpr double scale = 6.0;
    static constexpr std::uint64_t total_ticks = ticks_per_real_second * real_duration_seconds;
    std::uint64_t tick{};
    [[nodiscard]] double real_seconds() const noexcept { return static_cast<double>(tick) / 60.0; }
    [[nodiscard]] double game_seconds() const noexcept { return static_cast<double>(tick) / 10.0; }
};

enum class MatchPhase : std::uint8_t { first_half, second_half, completed };
struct PlayerState {
    std::string player_id;
    std::uint32_t team_index{}, lineup_index{};
    Vec3 position{};
    Vec3 velocity{};
    AIAction action{AIAction::off_ball};
};
struct GoalEvent {
    std::uint64_t sequence{}, tick{};
    std::uint32_t scoring_team{};
    std::int32_t scorer{-1};
    bool own_goal{};
    Vec3 crossing{};
};
struct MatchStatistics {
    std::array<std::uint64_t, 5> actions{}; // executed kicks/tackles; movement decisions
    std::uint64_t saves{}, interceptions{}, restarts{}, ai_slices{};
};
struct MatchSnapshot {
    DualClock clock{};
    MatchPhase phase{MatchPhase::first_half};
    std::array<std::uint32_t, 2> score{};
    std::array<PlayerState, 22> players;
    Vec3 ball{0.0, 0.11, 0.0};
    Vec3 ball_velocity{};
    std::int32_t ball_owner{-1};
    GoalEvent last_goal{};
    MatchStatistics statistics{};
    std::uint32_t restart_ticks_remaining{};
};

// Stage 2.1: simple utility AI/ball simulation, not a full football rules engine.
class MatchEngine final {
public:
    explicit MatchEngine(MatchInput input);
    void tick_fixed(std::uint32_t tick_count = 1);
    [[nodiscard]] const MatchSnapshot& snapshot() const noexcept { return state_; }
    [[nodiscard]] const MatchInput& input() const noexcept { return input_; }
    [[nodiscard]] const ReplayBuffer& replay() const noexcept { return replay_; }
private:
    void step();
    void kickoff(std::uint32_t team);
    void take_possession(std::int32_t player);
    void restart_out(const BallStepResult&);
    void decide_and_act();
    void move_players();
    void update_free_ball();
    double random_unit() noexcept;
    double direction(std::uint32_t team) const noexcept;
    MatchInput input_;
    MatchSnapshot state_;
    BallState ball_;
    ReplayBuffer replay_;
    std::array<PlayerAttributes, 22> attributes_{};
    std::array<Vec3, 22> formation_{};
    std::array<AIDecision, 22> decisions_{};
    std::array<std::uint32_t, 22> cooldown_{};
    std::uint64_t rng_{}, possession_since_{}, kicked_at_{};
    std::int32_t last_touch_{-1};
    std::uint32_t kickoff_team_{}, replay_flags_{};
    bool pending_kickoff_{}, shot_in_flight_{};
};

inline constexpr double replay_speed_multiplier = 0.5; // presentation only, not the 6x clock
inline constexpr bool allow_mid_match_control = false;
inline constexpr bool support_offline_replay = true;
} // namespace fsim
