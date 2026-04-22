#include "Arena.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <thread>

namespace {
std::string trim(const std::string& value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }

    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string to_lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}
} // namespace

Arena::Arena()
    : m_rng(std::random_device{}())
{
}

bool Arena::load_config(const std::string& config_path)
{
    std::ifstream config_file(config_path);
    if (!config_file) {
        std::cerr << "Unable to open config file: " << config_path << '\n';
        return false;
    }

    std::string line;
    while (std::getline(config_file, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        const std::string key = trim(line.substr(0, colon));
        const std::string value = trim(line.substr(colon + 1));

        if (key == "Arena_Size") {
            std::istringstream size_stream(value);
            size_stream >> m_config.height >> m_config.width;
        } else if (key == "Max_Rounds") {
            m_config.max_rounds = std::stoi(value);
        } else if (key == "Sleep_interval") {
            m_config.sleep_interval = std::stod(value);
        } else if (key == "Game_State_Live") {
            m_config.game_state_live = (to_lower_copy(value) == "true");
        } else if (key == "Flamethrowers") {
            m_config.flamethrowers = std::stoi(value);
        } else if (key == "Pits") {
            m_config.pits = std::stoi(value);
        } else if (key == "Mounds") {
            m_config.mounds = std::stoi(value);
        }
    }

    if (m_config.height <= 0 || m_config.width <= 0 || m_config.max_rounds <= 0) {
        std::cerr << "Config values must be positive.\n";
        return false;
    }

    return true;
}

bool Arena::initialize()
{
    m_obstacles.assign(
        static_cast<std::size_t>(m_config.height),
        std::vector<char>(static_cast<std::size_t>(m_config.width), '.'));

    place_obstacles();

    if (!load_robots()) {
        return false;
    }

    place_robots();
    return true;
}

bool Arena::is_in_bounds(int row, int col) const
{
    return row >= 0 && row < m_config.height && col >= 0 && col < m_config.width;
}

bool Arena::is_cell_free_for_spawn(int row, int col) const
{
    if (!is_in_bounds(row, col)) {
        return false;
    }

    if (m_obstacles[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] != '.') {
        return false;
    }

    return find_robot_at(row, col, true) == -1;
}

int Arena::find_robot_at(int row, int col, bool include_dead) const
{
    for (std::size_t i = 0; i < m_robots.size(); ++i) {
        int robot_row = 0;
        int robot_col = 0;
        m_robots[i].robot->get_current_location(robot_row, robot_col);

        if (robot_row == row && robot_col == col && (include_dead || m_robots[i].alive)) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

void Arena::place_obstacles()
{
    auto place_many = [&](int count, char symbol) {
        std::uniform_int_distribution<int> row_dist(0, m_config.height - 1);
        std::uniform_int_distribution<int> col_dist(0, m_config.width - 1);

        int placed = 0;
        while (placed < count) {
            const int row = row_dist(m_rng);
            const int col = col_dist(m_rng);

            if (m_obstacles[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] == '.') {
                m_obstacles[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] = symbol;
                ++placed;
            }
        }
    };

    place_many(m_config.flamethrowers, 'F');
    place_many(m_config.pits, 'P');
    place_many(m_config.mounds, 'M');
}

bool Arena::load_robots()
{
    namespace fs = std::filesystem;

    const std::string glyphs = "@#$%!&*+=?";
    std::size_t glyph_index = 0;

    for (const auto& entry : fs::directory_iterator(fs::current_path())) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const std::string filename = entry.path().filename().string();
        if (!filename.starts_with("Robot_") || !filename.ends_with(".cpp")) {
            continue;
        }

        if (glyph_index >= glyphs.size()) {
            break;
        }

        const std::string stem = entry.path().stem().string();
        const std::string shared_lib = "lib" + stem + ".so";
        const std::string compile_cmd =
            "g++ -shared -fPIC -o " + shared_lib + " " + filename + " RobotBase.o -I. -std=c++20";

        if (std::system(compile_cmd.c_str()) != 0) {
            continue;
        }

        void* handle = dlopen(("./" + shared_lib).c_str(), RTLD_LAZY);
        if (!handle) {
            continue;
        }

        RobotFactory create_robot =
            reinterpret_cast<RobotFactory>(dlsym(handle, "create_robot"));
        if (!create_robot) {
            dlclose(handle);
            continue;
        }

        RobotBase* robot = create_robot();
        if (!robot) {
            dlclose(handle);
            continue;
        }

        RobotEntry robot_entry;
        robot_entry.robot = robot;
        robot_entry.handle = handle;
        robot_entry.glyph = glyphs[glyph_index++];
        robot_entry.alive = true;
        robot_entry.shared_lib_path = shared_lib;
        robot_entry.grenades_remaining = (robot->get_weapon() == grenade) ? 10 : 0;

        robot->set_boundaries(m_config.height, m_config.width);
        m_robots.push_back(robot_entry);
    }

    return !m_robots.empty();
}

void Arena::place_robots()
{
    std::uniform_int_distribution<int> row_dist(0, m_config.height - 1);
    std::uniform_int_distribution<int> col_dist(0, m_config.width - 1);

    for (RobotEntry& entry : m_robots) {
        while (true) {
            const int row = row_dist(m_rng);
            const int col = col_dist(m_rng);

            if (is_cell_free_for_spawn(row, col)) {
                entry.robot->move_to(row, col);
                break;
            }
        }
    }
}

char Arena::cell_type_for_radar(int row, int col, int requesting_index) const
{
    const int robot_index = find_robot_at(row, col, true);
    if (robot_index >= 0 && robot_index != requesting_index) {
        return m_robots[static_cast<std::size_t>(robot_index)].alive ? 'R' : 'X';
    }

    return m_obstacles[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
}

std::vector<RadarObj> Arena::scan_radar_for_robot(int robot_index, int direction) const
{
    std::vector<RadarObj> results;

    int origin_row = 0;
    int origin_col = 0;
    m_robots[static_cast<std::size_t>(robot_index)].robot->get_current_location(origin_row, origin_col);

    if (direction == 0) {
        for (int dir = 1; dir <= 8; ++dir) {
            const auto [dr, dc] = direction_to_delta(dir);
            const int row = origin_row + dr;
            const int col = origin_col + dc;

            if (!is_in_bounds(row, col)) {
                continue;
            }

            const char cell_type = cell_type_for_radar(row, col, robot_index);
            if (cell_type != '.') {
                results.emplace_back(cell_type, row, col);
            }
        }

        return results;
    }

    const auto [dr, dc] = direction_to_delta(direction);
    const int side_row = dc;
    const int side_col = -dr;
    std::set<std::pair<int, int>> visited;

    for (int step = 1; step < std::max(m_config.height, m_config.width); ++step) {
        for (int offset = -1; offset <= 1; ++offset) {
            const int row = origin_row + dr * step + side_row * offset;
            const int col = origin_col + dc * step + side_col * offset;

            if (!is_in_bounds(row, col) || !visited.insert({row, col}).second) {
                continue;
            }

            const char cell_type = cell_type_for_radar(row, col, robot_index);
            if (cell_type != '.') {
                results.emplace_back(cell_type, row, col);
            }
        }
    }

    return results;
}

void Arena::render_board(int turn_number) const
{
    std::cout << "\n=========== starting round " << turn_number << " ===========\n";

    for (int row = 0; row < m_config.height; ++row) {
        for (int col = 0; col < m_config.width; ++col) {
            const int robot_index = find_robot_at(row, col, true);
            if (robot_index >= 0) {
                const RobotEntry& entry = m_robots[static_cast<std::size_t>(robot_index)];
                std::cout << (entry.alive ? 'R' : 'X') << entry.glyph << ' ';
            } else if (m_obstacles[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] != '.') {
                std::cout << m_obstacles[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] << "  ";
            } else {
                std::cout << ".  ";
            }
        }

        std::cout << '\n';
    }
}

void Arena::print_turn_log() const
{
    for (const std::string& line : m_turn_log) {
        std::cout << line << '\n';
    }
}

int Arena::living_robot_count() const
{
    int living = 0;

    for (const RobotEntry& entry : m_robots) {
        if (entry.alive) {
            ++living;
        }
    }

    return living;
}

void Arena::run()
{
    int round_number = 1;

    std::cout << "*******************\n";
    std::cout << " R O B O T W A R Z \n";
    std::cout << "*******************\n";
    std::cout << "Arena: " << m_config.height << " x " << m_config.width
              << "  Robots: " << m_robots.size()
              << "  Max rounds: " << m_config.max_rounds << '\n';
    std::cout << "Obstacles - F:" << m_config.flamethrowers
              << " P:" << m_config.pits
              << " M:" << m_config.mounds << "\n";

    while (round_number <= m_config.max_rounds) {
        render_board(round_number);

        if (living_robot_count() <= 1) {
            for (const RobotEntry& entry : m_robots) {
                if (entry.alive) {
                    std::cout << "Winner: " << entry.robot->m_name
                              << " (" << entry.glyph << ")\n";
                    return;
                }
            }

            std::cout << "Game over.\n";
            return;
        }

        for (std::size_t i = 0; i < m_robots.size(); ++i) {
            m_turn_log.clear();
            handle_robot_turn(static_cast<int>(i), round_number);
            print_turn_log();
        }

        if (m_config.game_state_live && m_config.sleep_interval > 0.0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(m_config.sleep_interval));
        }

        ++round_number;
    }

    std::cout << "Reached max rounds.\n";
}

bool Arena::handle_robot_turn(int robot_index, int turn_number)
{
    RobotEntry& entry = m_robots[static_cast<std::size_t>(robot_index)];
    RobotBase* robot = entry.robot;

    int row = 0;
    int col = 0;
    robot->get_current_location(row, col);

    std::ostringstream intro;
    intro << robot->m_name << " [" << entry.glyph << "] turn " << turn_number
          << " at (" << row << "," << col << ") "
          << "H:" << robot->get_health()
          << " A:" << robot->get_armor()
          << " M:" << robot->get_move_speed();
    m_turn_log.push_back(intro.str());

    if (!entry.alive) {
        m_turn_log.push_back("  robot is out and skips its turn");
        return false;
    }

    int radar_direction = 0;
    robot->get_radar_direction(radar_direction);
    std::vector<RadarObj> radar_results = scan_radar_for_robot(robot_index, radar_direction);
    robot->process_radar_results(radar_results);

    if (radar_results.empty()) {
        m_turn_log.push_back("  radar found nothing");
    } else {
        std::ostringstream radar_log;
        radar_log << "  radar:";
        for (const RadarObj& obj : radar_results) {
            radar_log << " " << obj.m_type << "@(" << obj.m_row << "," << obj.m_col << ")";
        }
        m_turn_log.push_back(radar_log.str());
    }

    int shot_row = 0;
    int shot_col = 0;
    if (robot->get_shot_location(shot_row, shot_col)) {
        return handle_shot(robot_index, shot_row, shot_col);
    }

    int move_direction = 0;
    int move_distance = 0;
    robot->get_move_direction(move_direction, move_distance);

    if (move_direction < 0 || move_direction > 8 || move_distance <= 0 || robot->get_move_speed() <= 0) {
        m_turn_log.push_back("  no action taken");
        return false;
    }

    handle_move(robot_index, move_direction, move_distance);
    return true;
}

bool Arena::handle_shot(int robot_index, int shot_row, int shot_col)
{
    RobotEntry& shooter = m_robots[static_cast<std::size_t>(robot_index)];
    WeaponType weapon = shooter.robot->get_weapon();

    int start_row = 0;
    int start_col = 0;
    shooter.robot->get_current_location(start_row, start_col);

    if (weapon == grenade) {
        if (shooter.grenades_remaining <= 0) {
            m_turn_log.push_back("  tried to fire grenade launcher with no grenades left");
            return false;
        }

        --shooter.grenades_remaining;
        shooter.robot->decrement_grenades();
    }

    std::vector<std::pair<int, int>> affected_cells;
    switch (weapon) {
    case railgun:
        affected_cells = compute_line_cells(start_row, start_col, shot_row, shot_col);
        break;
    case flamethrower:
        affected_cells = compute_flamethrower_cells(start_row, start_col, shot_row, shot_col);
        break;
    case grenade:
        affected_cells = compute_grenade_cells(shot_row, shot_col);
        break;
    case hammer:
        affected_cells.push_back(compute_hammer_cell(start_row, start_col, shot_row, shot_col));
        break;
    }

    std::ostringstream action;
    action << "  fires ";
    switch (weapon) {
    case railgun: action << "railgun"; break;
    case flamethrower: action << "flamethrower"; break;
    case grenade: action << "grenade launcher"; break;
    case hammer: action << "hammer"; break;
    }
    action << " at (" << shot_row << "," << shot_col << ")";
    m_turn_log.push_back(action.str());

    std::set<int> hit_targets;
    for (const auto& [row, col] : affected_cells) {
        const int target_index = find_robot_at(row, col, false);
        if (target_index >= 0 && target_index != robot_index && hit_targets.insert(target_index).second) {
            apply_damage_to_robot(target_index, roll_damage(weapon), "weapon hit");
        }
    }

    if (hit_targets.empty()) {
        m_turn_log.push_back("  shot hit nothing");
    }

    return true;
}

void Arena::apply_damage_to_robot(int target_index, int base_damage, const std::string& cause)
{
    RobotEntry& target = m_robots[static_cast<std::size_t>(target_index)];
    const int armor = target.robot->get_armor();
    const double reduction = std::clamp(armor * 0.10, 0.0, 0.90);
    const int actual_damage =
        std::max(1, static_cast<int>(std::lround(base_damage * (1.0 - reduction))));

    target.robot->take_damage(actual_damage);
    target.robot->reduce_armor(1);

    std::ostringstream hit_log;
    hit_log << "    " << target.robot->m_name
            << " takes " << actual_damage
            << " damage from " << cause
            << " and is now H:" << target.robot->get_health()
            << " A:" << target.robot->get_armor();
    m_turn_log.push_back(hit_log.str());

    if (target.robot->get_health() <= 0) {
        target.alive = false;
        m_turn_log.push_back("    " + target.robot->m_name + " is destroyed");
    }
}

int Arena::roll_damage(WeaponType weapon)
{
    switch (weapon) {
    case railgun: {
        std::uniform_int_distribution<int> dist(10, 20);
        return dist(m_rng);
    }
    case hammer: {
        std::uniform_int_distribution<int> dist(50, 60);
        return dist(m_rng);
    }
    case grenade: {
        std::uniform_int_distribution<int> dist(10, 40);
        return dist(m_rng);
    }
    case flamethrower:
    default: {
        std::uniform_int_distribution<int> dist(30, 50);
        return dist(m_rng);
    }
    }
}

void Arena::handle_move(int robot_index, int direction, int distance)
{
    RobotEntry& entry = m_robots[static_cast<std::size_t>(robot_index)];

    if (direction <= 0 || direction > 8 || distance <= 0 || entry.robot->get_move_speed() <= 0) {
        m_turn_log.push_back("  no action taken");
        return;
    }

    int current_row = 0;
    int current_col = 0;
    entry.robot->get_current_location(current_row, current_col);

    const int capped_distance = std::min(distance, entry.robot->get_move_speed());
    const auto [dr, dc] = direction_to_delta(direction);

    for (int step = 1; step <= capped_distance; ++step) {
        const int next_row = current_row + dr;
        const int next_col = current_col + dc;

        if (!is_in_bounds(next_row, next_col)) {
            break;
        }

        if (find_robot_at(next_row, next_col, true) >= 0) {
            break;
        }

        const char obstacle = m_obstacles[static_cast<std::size_t>(next_row)][static_cast<std::size_t>(next_col)];
        if (obstacle == 'M') {
            break;
        }

        current_row = next_row;
        current_col = next_col;
        entry.robot->move_to(current_row, current_col);

        if (obstacle == 'P') {
            entry.robot->disable_movement();
            break;
        }

        if (obstacle == 'F') {
            apply_damage_to_robot(robot_index, roll_damage(flamethrower), "arena flamethrower");
            if (!entry.alive) {
                m_obstacles[static_cast<std::size_t>(current_row)][static_cast<std::size_t>(current_col)] = '.';
                break;
            }
        }
    }

    int end_row = 0;
    int end_col = 0;
    entry.robot->get_current_location(end_row, end_col);

    std::ostringstream move_log;
    move_log << "  moves to (" << end_row << "," << end_col << ")";
    if (entry.robot->get_move_speed() == 0) {
        move_log << " and is trapped in a pit";
    }
    m_turn_log.push_back(move_log.str());
}

std::vector<std::pair<int, int>> Arena::compute_line_cells(int start_row,
                                                           int start_col,
                                                           int target_row,
                                                           int target_col) const
{
    std::vector<std::pair<int, int>> cells;

    const int delta_row = target_row - start_row;
    const int delta_col = target_col - start_col;
    const int steps = std::max(std::abs(delta_row), std::abs(delta_col));
    if (steps == 0) {
        return cells;
    }

    const double row_inc = static_cast<double>(delta_row) / static_cast<double>(steps);
    const double col_inc = static_cast<double>(delta_col) / static_cast<double>(steps);

    double row = static_cast<double>(start_row);
    double col = static_cast<double>(start_col);
    std::set<std::pair<int, int>> visited;

    for (;;) {
        row += row_inc;
        col += col_inc;

        const int actual_row = static_cast<int>(std::lround(row));
        const int actual_col = static_cast<int>(std::lround(col));

        if (!is_in_bounds(actual_row, actual_col)) {
            break;
        }

        if (visited.insert({actual_row, actual_col}).second) {
            cells.emplace_back(actual_row, actual_col);
        }
    }

    return cells;
}

std::vector<std::pair<int, int>> Arena::compute_flamethrower_cells(int start_row,
                                                                   int start_col,
                                                                   int target_row,
                                                                   int target_col) const
{
    std::vector<std::pair<int, int>> cells;

    const auto [dr, dc] = target_to_octant(start_row, start_col, target_row, target_col);
    if (dr == 0 && dc == 0) {
        return cells;
    }

    const int side_row = dc;
    const int side_col = -dr;
    std::set<std::pair<int, int>> visited;

    for (int step = 1; step <= 4; ++step) {
        for (int offset = -1; offset <= 1; ++offset) {
            const int row = start_row + dr * step + side_row * offset;
            const int col = start_col + dc * step + side_col * offset;

            if (is_in_bounds(row, col) && visited.insert({row, col}).second) {
                cells.emplace_back(row, col);
            }
        }
    }

    return cells;
}

std::vector<std::pair<int, int>> Arena::compute_grenade_cells(int target_row, int target_col) const
{
    std::vector<std::pair<int, int>> cells;

    for (int row = target_row - 1; row <= target_row + 1; ++row) {
        for (int col = target_col - 1; col <= target_col + 1; ++col) {
            if (is_in_bounds(row, col)) {
                cells.emplace_back(row, col);
            }
        }
    }

    return cells;
}

std::pair<int, int> Arena::compute_hammer_cell(int start_row, int start_col, int target_row, int target_col) const
{
    const auto [dr, dc] = target_to_octant(start_row, start_col, target_row, target_col);
    return {start_row + dr, start_col + dc};
}

std::pair<int, int> Arena::direction_to_delta(int direction) const
{
    if (direction < 0 || direction > 8) {
        return {0, 0};
    }

    return directions[direction];
}

std::pair<int, int> Arena::target_to_octant(int from_row, int from_col, int to_row, int to_col) const
{
    const int dr = (to_row > from_row) ? 1 : ((to_row < from_row) ? -1 : 0);
    const int dc = (to_col > from_col) ? 1 : ((to_col < from_col) ? -1 : 0);
    return {dr, dc};
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>\n";
        return 1;
    }

    Arena arena;
    if (!arena.load_config(argv[1])) {
        return 1;
    }

    if (!arena.initialize()) {
        return 1;
    }

    arena.run();
    return 0;
}
