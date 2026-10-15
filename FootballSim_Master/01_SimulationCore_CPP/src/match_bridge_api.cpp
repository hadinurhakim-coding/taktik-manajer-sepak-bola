#include "match_bridge_api.h"
#include "MatchEngine.h"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace {
using json = nlohmann::json;
std::mutex registry_mutex;
std::unordered_map<fsim_match_handle, std::unique_ptr<fsim::MatchEngine>> matches;
fsim_match_handle next_handle = 1;
thread_local char last_error[512]{};

void error_text(const char* text) noexcept { std::snprintf(last_error, sizeof(last_error), "%s", text); }
template<class F> int32_t guarded(F&& operation) noexcept {
    last_error[0] = '\0';
    try { return operation(); }
    catch (const json::exception& error) { error_text(error.what()); return FSIM_INVALID_ARGUMENT; }
    catch (const std::invalid_argument& error) { error_text(error.what()); return FSIM_INVALID_ARGUMENT; }
    catch (const std::exception& error) { error_text(error.what()); return FSIM_INTERNAL_ERROR; }
    catch (...) { error_text("Unknown C++ exception"); return FSIM_INTERNAL_ERROR; }
}
void require(bool value, const char* text) { if (!value) throw std::invalid_argument(text); }
std::string string_value(const json& value) {
    require(value.is_string(), "Expected a string");
    auto result = value.get<std::string>();
    require(result.size() <= 256 && result.find('\0') == std::string::npos, "String exceeds 256 UTF-8 bytes or contains NUL");
    return result;
}
double number(const json& value) {
    require(value.is_number(), "Expected a finite number");
    const double result = value.get<double>();
    require(std::isfinite(result), "Nonfinite number");
    return result;
}
std::uint32_t integer(const json& value) {
    require(value.is_number_integer(), "Expected a nonnegative integer");
    if (value.is_number_unsigned()) {
        const auto result = value.get<std::uint64_t>();
        require(result <= std::numeric_limits<std::uint32_t>::max(), "Integer exceeds uint32");
        return static_cast<std::uint32_t>(result);
    }
    const auto result = value.get<std::int64_t>();
    require(result >= 0 && result <= std::numeric_limits<std::uint32_t>::max(), "Integer outside uint32 range");
    return static_cast<std::uint32_t>(result);
}
template<class E, std::size_t N> E enum_value(const json& value, const std::array<std::string_view, N>& names) {
    const auto name = string_value(value);
    const auto found = std::find(names.begin(), names.end(), name);
    require(found != names.end(), "Unknown schema enum value");
    return static_cast<E>(found - names.begin());
}
std::uint32_t level(std::uint32_t value) { return value <= 33 ? 0U : value <= 66 ? 1U : 2U; }

fsim::MatchInput parse_input(const char* text, size_t size) {
    const auto document = json::parse(text, text + size, [](int depth, json::parse_event_t, json&) {
        require(depth <= 64, "JSON nesting exceeds 64"); return true;
    });
    require(document.at("input_version") == "fsim.match_input.v1", "Unsupported input_version");
    const auto& teams = document.at("teams");
    require(teams.is_array() && teams.size() == 2, "Exactly two teams are required");
    fsim::MatchInput input;
    const auto seed = string_value(document.at("seed"));
    require(!seed.empty(), "Seed must be a decimal uint64 string");
    const auto parsed = std::from_chars(seed.data(), seed.data() + seed.size(), input.seed);
    require(parsed.ec == std::errc{} && parsed.ptr == seed.data() + seed.size(), "Seed must be a decimal uint64 string");
    constexpr std::array<std::string_view, 4> role_names = {"GK", "DF", "MF", "FW"};
    for (std::size_t ti = 0; ti < 2; ++ti) {
        const auto& source = teams[ti];
        auto& team = input.teams[ti];
        team.id = string_value(source.at("id"));
        team.league_id = string_value(source.at("league_id"));
        team.name = string_value(source.at("name"));
        team.short_name = string_value(source.value("short_name", json("")));
        team.rating = number(source.at("rating"));
        team.fitness = number(source.at("fitness"));
        team.club_revision = integer(source.value("club_revision", json(0)));
        const auto& t = source.at("tactics");
        auto& tactics = team.tactics;
        tactics.formation = enum_value<fsim::Formation>(t.at("formation"), fsim::schema::formations);
        tactics.play_style = enum_value<fsim::PlayStyle>(t.at("play_style"), fsim::schema::play_styles);
        tactics.mentality = enum_value<fsim::Mentality>(t.at("mentality"), fsim::schema::mentalities);
        tactics.pressing = enum_value<fsim::Pressing>(t.at("pressing"), fsim::schema::pressing);
        tactics.tempo = enum_value<fsim::Tempo>(t.at("tempo"), fsim::schema::tempos);
        tactics.pressing_intensity = integer(t.value("pressing_intensity", json(static_cast<unsigned>(tactics.pressing) * 50)));
        tactics.tempo_intensity = integer(t.value("tempo_intensity", json(static_cast<unsigned>(tactics.tempo) * 50)));
        tactics.width = integer(t.value("width", json(50)));
        require(level(tactics.pressing_intensity) == static_cast<unsigned>(tactics.pressing) && level(tactics.tempo_intensity) == static_cast<unsigned>(tactics.tempo), "Tactical enum disagrees with numeric 0..33/34..66/67..100 bucket");
        const auto& specialists = t.at("specialists");
        for (std::size_t i = 0; i < fsim::schema::specialists.size(); ++i) {
            const std::string key(fsim::schema::specialists[i]);
            tactics.specialists[i] = string_value(specialists.at(key));
            if (t.contains("specialist_backups") && t["specialist_backups"].contains(key))
                tactics.specialist_backups[i] = string_value(t["specialist_backups"][key]);
        }
        const auto& lineup = source.at("lineup");
        require(lineup.is_array() && lineup.size() == 11, "Lineup must contain 11 player IDs");
        for (std::size_t i = 0; i < 11; ++i) team.lineup[i] = string_value(lineup[i]);
        const auto& players = source.at("players");
        require(players.is_array() && players.size() >= 11 && players.size() <= 64, "Roster must have 11..64 players");
        for (const auto& p : players) {
            fsim::Player player;
            player.id = string_value(p.at("id")); player.name = string_value(p.at("name"));
            player.number = integer(p.at("number"));
            player.role = enum_value<fsim::Role>(p.at("role"), role_names);
            player.primary_position = string_value(p.value("primary_position", json("")));
            if (p.contains("secondary_positions")) {
                require(p["secondary_positions"].is_array(), "secondary_positions must be an array");
                for (const auto& position : p["secondary_positions"]) player.secondary_positions.push_back(string_value(position));
            }
            const auto& anchor = p.at("anchor");
            require(anchor.is_array() && anchor.size() == 2, "anchor must be [x,z]");
            player.source_anchor = {number(anchor[0]), number(anchor[1])};
            const auto& a = p.at("attributes");
            player.attributes = {number(a.at("speed")), number(a.at("acceleration")), number(a.at("stamina")), number(a.at("passing")), number(a.at("ball_control")),
                                 number(a.at("finishing")), number(a.at("heading")), number(a.at("tackling")), number(a.at("positioning")), number(a.at("goalkeeping"))};
            team.players.push_back(std::move(player));
        }
    }
    return input;
}
json goal_json(const fsim::MatchEngine& engine) {
    const auto& s = engine.snapshot(); const auto& g = s.last_goal;
    json latest = nullptr;
    if (g.sequence) latest = {{"sequence", g.sequence}, {"tick", g.tick}, {"scoring_team", g.scoring_team},
        {"scorer_index", g.scorer}, {"scorer_id", g.scorer >= 0 ? s.players[static_cast<std::size_t>(g.scorer)].player_id : ""},
        {"own_goal", g.own_goal}, {"crossing", {g.crossing.x, g.crossing.y, g.crossing.z}}};
    return {{"goal_status_version", "fsim.goal_status.v1"}, {"sequence", g.sequence}, {"score", s.score}, {"latest", latest}};
}
json snapshot_json(const fsim::MatchEngine& engine) {
    const auto& s = engine.snapshot();
    json players = json::array();
    for (const auto& player : s.players) players.push_back({{"id", player.player_id}, {"team", player.team_index}, {"lineup_index", player.lineup_index},
                                                         {"position", {player.position.x, player.position.y, player.position.z}},
                                                         {"velocity", {player.velocity.x, player.velocity.y, player.velocity.z}},
                                                         {"action", fsim::ai_action_names[static_cast<std::size_t>(player.action)]}});
    json teams = json::array();
    for (const auto& team : engine.input().teams) teams.push_back({{"id", team.id}, {"name", team.name}, {"formation", fsim::schema::formations[static_cast<std::size_t>(team.tactics.formation)]}, {"lineup", team.lineup}});
    const char* phase = s.phase == fsim::MatchPhase::completed ? "completed" : s.phase == fsim::MatchPhase::second_half ? "second_half" : "first_half";
    json actions = json::object();
    for (std::size_t i = 0; i < fsim::ai_action_names.size(); ++i) actions[std::string(fsim::ai_action_names[i])] = s.statistics.actions[i];
    return {{"snapshot_version", "fsim.match_snapshot.v2"}, {"simulation_scope", "simple_utility_ai_and_ball_physics"},
            {"seed", std::to_string(engine.input().seed)}, {"tick", s.clock.tick}, {"ticks_per_real_second", 60}, {"time_scale", 6.0},
            {"real_elapsed_seconds", s.clock.real_seconds()}, {"game_elapsed_seconds", s.clock.game_seconds()},
            {"real_duration_seconds", 900}, {"game_duration_seconds", 5400}, {"phase", phase},
            {"score", s.score}, {"score_is_final_simulated_result", s.phase == fsim::MatchPhase::completed}, {"teams", teams}, {"players", players},
            {"ball", {s.ball.x, s.ball.y, s.ball.z}}, {"ball_velocity", {s.ball_velocity.x, s.ball_velocity.y, s.ball_velocity.z}},
            {"ball_owner", s.ball_owner}, {"goals", goal_json(engine)}, {"restart_ticks_remaining", s.restart_ticks_remaining},
            {"statistics", {{"actions", actions}, {"shots", s.statistics.shots}, {"shots_on_target", s.statistics.shots_on_target}, {"team_saves", s.statistics.team_saves}, {"saves", s.statistics.saves}, {"interceptions", s.statistics.interceptions},
                            {"restarts", s.statistics.restarts}, {"ai_slices", s.statistics.ai_slices}}},
            {"replay", {{"frames", engine.replay().size()}, {"capacity", fsim::ReplayBuffer::capacity}, {"sample_presentation_seconds", 0.05}, {"window_presentation_seconds", 15}}}};
}
std::vector<std::uint8_t> replay_bytes(const fsim::MatchEngine& engine, bool archive = false) {
    json identities = json::array();
    for (const auto& p : engine.snapshot().players) {
        const auto& roster = engine.input().teams[p.team_index].players;
        const auto found = std::find_if(roster.begin(), roster.end(), [&](const auto& player) { return player.id == p.player_id; });
        identities.push_back({{"id", p.player_id}, {"name", found->name}, {"team", p.team_index}, {"lineup_index", p.lineup_index}});
    }
    json teams = json::array();
    for (const auto& t : engine.input().teams) teams.push_back({{"id", t.id}, {"name", t.name}});
    const auto& r = archive ? engine.archive() : engine.replay();
    json metadata = {{"format", archive ? "fsim.replay.v2" : "fsim.replay.v1"}, {"engine_version", "0.3.1"}, {"seed", std::to_string(engine.input().seed)},
        {"teams", teams}, {"players", identities}, {"actions", fsim::ai_action_names},
        {"coordinates", "metres; x length, y up, z width"}, {"time_base", "presentation"}, {"time_scale", 6.0},
        {"first_tick", r.at(0).tick}, {"last_tick", r.at(r.size()-1).tick}, {"last_goal_status", goal_json(engine)},
        {"flags", {{"goal_since_previous_sample", 1}, {"restart_discontinuity", 2}, {"halftime_discontinuity", 4}}}};
    if (archive) {
        metadata["segments"] = json::array({{{"first_frame", 0}, {"last_frame", r.size()-1}, {"rate", 1.0}, {"hold_seconds", 0.0}, {"kind", "live"}, {"camera", "broadcast"}}});
        metadata["timeline_seconds"] = (r.at(r.size()-1).tick-r.at(0).tick)/10.0;
        metadata["source_ticks_per_second"] = 10;
        metadata["flags"]["shot"] = 8; metadata["flags"]["save"] = 16; metadata["flags"]["big_save"] = 32;
    }
    return r.export_fsim(metadata.dump(), archive ? 2 : 1);
}
json schema_json() {
    using namespace fsim::schema;
    return {{"player_attributes", attributes}, {"formations", formations},
            {"tactics", {{"play_style", play_styles}, {"mentality", mentalities}, {"pressing", pressing}, {"tempo", tempos}}},
            {"specialists", specialists}, {"simulated_actions", {{"on_ball", on_ball}, {"off_ball", off_ball}, {"goalkeeper", goalkeeper}, {"set_pieces", set_pieces}}},
            {"presentation_rules", {{"full_match_minutes", 90}, {"highlight_target_minutes", 15}, {"replay_speed_multiplier", fsim::replay_speed_multiplier},
                                    {"allow_mid_match_control", fsim::allow_mid_match_control}, {"support_offline_replay", fsim::support_offline_replay}}}};
}
int32_t copy_text(std::string_view text, char* output, size_t capacity, size_t* required) noexcept {
    if (!required || (!output && capacity != 0)) return FSIM_INVALID_ARGUMENT;
    *required = text.size() + 1;
    if (capacity < *required || !output) return FSIM_BUFFER_TOO_SMALL;
    std::memcpy(output, text.data(), text.size());
    output[text.size()] = '\0';
    return FSIM_OK;
}
} // namespace

uint32_t FSIM_CALL get_match_api_version(void) { return 1; }
int32_t FSIM_CALL init_match(const char* input_json, size_t input_size, fsim_match_handle* out_handle) {
    if (out_handle) *out_handle = 0;
    return guarded([&]() -> int32_t {
        require(input_json && out_handle && input_size > 0 && input_size <= 1024 * 1024, "init_match requires input (1..1048576 bytes) and out_handle");
        auto engine = std::make_unique<fsim::MatchEngine>(parse_input(input_json, input_size));
        std::lock_guard lock(registry_mutex);
        require(next_handle != 0, "Match handle space exhausted");
        const auto handle = next_handle++;
        matches.emplace(handle, std::move(engine));
        *out_handle = handle;
        return FSIM_OK;
    });
}
int32_t FSIM_CALL tick_match_fixed(fsim_match_handle handle, uint32_t tick_count) {
    return guarded([&]() -> int32_t {
        std::lock_guard lock(registry_mutex);
        const auto found = matches.find(handle);
        if (found == matches.end()) { error_text("Invalid or destroyed match handle"); return FSIM_INVALID_HANDLE; }
        found->second->tick_fixed(tick_count);
        return FSIM_OK;
    });
}
int32_t FSIM_CALL get_match_snapshot(fsim_match_handle handle, char* output, size_t capacity, size_t* required_size) {
    return guarded([&]() -> int32_t {
        std::lock_guard lock(registry_mutex);
        const auto found = matches.find(handle);
        if (found == matches.end()) { error_text("Invalid or destroyed match handle"); return FSIM_INVALID_HANDLE; }
        const auto result = copy_text(snapshot_json(*found->second).dump(), output, capacity, required_size);
        if (result != FSIM_OK) error_text(result == FSIM_BUFFER_TOO_SMALL ? "Snapshot buffer too small" : "Invalid snapshot output pointers");
        return result;
    });
}
int32_t FSIM_CALL get_engine_schema(char* output, size_t capacity, size_t* required_size) {
    return guarded([&]() -> int32_t {
        const auto result = copy_text(schema_json().dump(), output, capacity, required_size);
        if (result != FSIM_OK) error_text(result == FSIM_BUFFER_TOO_SMALL ? "Schema buffer too small" : "Invalid schema output pointers");
        return result;
    });
}
int32_t FSIM_CALL get_match_goal_status(fsim_match_handle handle, char* output, size_t capacity, size_t* required_size) {
    return guarded([&]() -> int32_t {
        std::lock_guard lock(registry_mutex);
        const auto found = matches.find(handle);
        if (found == matches.end()) { error_text("Invalid or destroyed match handle"); return FSIM_INVALID_HANDLE; }
        const auto result = copy_text(goal_json(*found->second).dump(), output, capacity, required_size);
        if (result != FSIM_OK) error_text(result == FSIM_BUFFER_TOO_SMALL ? "Goal buffer too small" : "Invalid goal output pointers");
        return result;
    });
}
int32_t FSIM_CALL export_match_replay(fsim_match_handle handle, uint8_t* output, size_t capacity, size_t* required_size) {
    return guarded([&]() -> int32_t {
        std::lock_guard lock(registry_mutex);
        const auto found = matches.find(handle);
        if (found == matches.end()) { error_text("Invalid or destroyed match handle"); return FSIM_INVALID_HANDLE; }
        require(required_size && (output || capacity == 0), "Invalid replay output pointers");
        const auto bytes = replay_bytes(*found->second);
        *required_size = bytes.size();
        if (!output || capacity < bytes.size()) { error_text("Replay buffer too small"); return FSIM_BUFFER_TOO_SMALL; }
        std::memcpy(output, bytes.data(), bytes.size());
        return FSIM_OK;
    });
}
int32_t FSIM_CALL export_match_archive(fsim_match_handle handle, uint8_t* output, size_t capacity, size_t* required_size) {
    return guarded([&]() -> int32_t {
        std::lock_guard lock(registry_mutex);
        const auto found = matches.find(handle);
        if (found == matches.end()) { error_text("Invalid or destroyed match handle"); return FSIM_INVALID_HANDLE; }
        require(required_size && (output || capacity == 0), "Invalid archive output pointers");
        const auto bytes = replay_bytes(*found->second, true);
        *required_size = bytes.size();
        if (!output || capacity < bytes.size()) return FSIM_BUFFER_TOO_SMALL;
        std::memcpy(output, bytes.data(), bytes.size());
        return FSIM_OK;
    });
}
int32_t FSIM_CALL destroy_match(fsim_match_handle handle) {
    return guarded([&]() -> int32_t {
        std::lock_guard lock(registry_mutex);
        if (matches.erase(handle) != 1) { error_text("Invalid or destroyed match handle"); return FSIM_INVALID_HANDLE; }
        return FSIM_OK;
    });
}
int32_t FSIM_CALL get_match_last_error(char* output, size_t capacity, size_t* required_size) {
    return copy_text(last_error, output, capacity, required_size);
}
