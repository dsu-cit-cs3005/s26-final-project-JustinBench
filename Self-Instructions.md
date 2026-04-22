# Self Instructions

This file is your build order for writing `Arena.cpp` and `Robot_Justin.cpp` yourself.

Each step includes:

- what to add
- why it matters
- a small sample of the code shape to write

The snippets are examples, not a full copy-paste solution. They are here to show the structure you should be aiming for.

## Part 1: Start `Arena.h`

### Step 1: Add the includes you will need

Why:

- `Arena` depends on strings, vectors, random numbers, and the robot API.

Sample:

```cpp
#pragma once

#include <random>
#include <string>
#include <vector>
#include <utility>

#include "RobotBase.h"
```

### Step 2: Create the `Arena` class and `Config` struct

Why:

- The config groups all arena setup values in one place.

Sample:

```cpp
class Arena {
public:
    struct Config {
        int height = 20;
        int width = 20;
        int max_rounds = 200;
        double sleep_interval = 0.0;
        bool game_state_live = false;
        int flamethrowers = 3;
        int pits = 3;
        int mounds = 3;
    };
```

### Step 3: Add a `RobotEntry` struct

Why:

- The arena must track more than just the robot pointer.

Sample:

```cpp
private:
    struct RobotEntry {
        RobotBase* robot = nullptr;
        void* handle = nullptr;
        char glyph = '?';
        bool alive = true;
        int grenades_remaining = 0;
        std::string shared_lib_path;
    };
```

### Step 4: Add Arena member variables

Why:

- These hold the full game state.

Sample:

```cpp
private:
    Config m_config;
    std::vector<RobotEntry> m_robots;
    std::vector<std::vector<char>> m_obstacles;
    std::vector<std::string> m_turn_log;
    std::mt19937 m_rng;
```

### Step 5: Declare the major methods

Why:

- Declaring them first helps you build one piece at a time.

Sample:

```cpp
public:
    Arena();
    bool load_config(const std::string& config_path);
    bool initialize();
    void run();

private:
    bool load_robots();
    void place_obstacles();
    void place_robots();
    bool is_in_bounds(int row, int col) const;
    int find_robot_at(int row, int col, bool include_dead = true) const;
    std::vector<RadarObj> scan_radar_for_robot(int robot_index, int direction) const;
    bool handle_robot_turn(int robot_index, int turn_number);
    bool handle_shot(int robot_index, int shot_row, int shot_col);
    void handle_move(int robot_index, int direction, int distance);
```

## Part 2: Start `Arena.cpp`

### Step 6: Add the includes

Why:

- `Arena.cpp` needs file I/O, dynamic loading, printing, and utility helpers.

Sample:

```cpp
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
```

### Step 7: Add the constructor

Why:

- You want the random number generator seeded once when the arena starts.

Sample:

```cpp
Arena::Arena()
    : m_rng(std::random_device{}())
{
}
```

## Part 3: Parse the Config File

### Step 8: Start `load_config`

Why:

- The arena must load setup values from a config file, not stdin.

Sample:

```cpp
bool Arena::load_config(const std::string& config_path)
{
    std::ifstream config_file(config_path);
    if (!config_file) {
        std::cerr << "Unable to open config file: " << config_path << '\n';
        return false;
    }
```

### Step 9: Read line by line and split on `:`

Why:

- Each config line is key/value data.

Sample:

```cpp
    std::string line;
    while (std::getline(config_file, line)) {
        std::size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
```

### Step 10: Parse the known keys

Why:

- These are the fields required by the spec.

Sample:

```cpp
        if (key == "Max_Rounds") {
            m_config.max_rounds = std::stoi(value);
        } else if (key == "Sleep_interval") {
            m_config.sleep_interval = std::stod(value);
        } else if (key == "Game_State_Live") {
            m_config.game_state_live = (value == "true");
        }
```

For `Arena_Size`, split the value into two numbers:

```cpp
        if (key == "Arena_Size") {
            std::istringstream size_stream(value);
            size_stream >> m_config.height >> m_config.width;
        }
```

### Step 11: Validate the config

Why:

- Invalid config values will cause weird later failures.

Sample:

```cpp
    if (m_config.height <= 0 || m_config.width <= 0 || m_config.max_rounds <= 0) {
        std::cerr << "Config values must be positive.\n";
        return false;
    }

    return true;
}
```

## Part 4: Build the Arena Grid

### Step 12: Implement `initialize`

Why:

- This prepares the full game state before `run()` starts.

Sample:

```cpp
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
```

### Step 13: Add `is_in_bounds`

Why:

- You will reuse this helper everywhere.

Sample:

```cpp
bool Arena::is_in_bounds(int row, int col) const
{
    return row >= 0 &&
           row < m_config.height &&
           col >= 0 &&
           col < m_config.width;
}
```

### Step 14: Add `find_robot_at`

Why:

- Radar, shooting, rendering, and movement all need this.

Sample:

```cpp
int Arena::find_robot_at(int row, int col, bool include_dead) const
{
    for (std::size_t i = 0; i < m_robots.size(); ++i) {
        int robot_row = 0;
        int robot_col = 0;
        m_robots[i].robot->get_current_location(robot_row, robot_col);

        if (robot_row == row && robot_col == col) {
            if (include_dead || m_robots[i].alive) {
                return static_cast<int>(i);
            }
        }
    }

    return -1;
}
```

## Part 5: Place Obstacles

### Step 15: Write `place_obstacles`

Why:

- The arena must begin with `F`, `P`, and `M` cells already placed.

Sample:

```cpp
void Arena::place_obstacles()
{
    auto place_many = [&](int count, char symbol) {
        std::uniform_int_distribution<int> row_dist(0, m_config.height - 1);
        std::uniform_int_distribution<int> col_dist(0, m_config.width - 1);

        int placed = 0;
        while (placed < count) {
            int row = row_dist(m_rng);
            int col = col_dist(m_rng);

            if (m_obstacles[row][col] == '.') {
                m_obstacles[row][col] = symbol;
                ++placed;
            }
        }
    };

    place_many(m_config.flamethrowers, 'F');
    place_many(m_config.pits, 'P');
    place_many(m_config.mounds, 'M');
}
```

## Part 6: Load Robot Source Files

### Step 16: Start `load_robots`

Why:

- The arena must discover robot source files automatically.

Sample:

```cpp
bool Arena::load_robots()
{
    namespace fs = std::filesystem;

    for (const auto& entry : fs::directory_iterator(fs::current_path())) {
        if (!entry.is_regular_file()) {
            continue;
        }
```

### Step 17: Filter names like `Robot_*.cpp`

Why:

- You only want student robot source files.

Sample:

```cpp
        std::string filename = entry.path().filename().string();
        if (!filename.starts_with("Robot_") || !filename.ends_with(".cpp")) {
            continue;
        }
```

### Step 18: Compile each robot into a `.so`

Why:

- The robot must be dynamically loaded at runtime.

Sample:

```cpp
        std::string stem = entry.path().stem().string();
        std::string shared_lib = "lib" + stem + ".so";

        std::string compile_cmd =
            "g++ -shared -fPIC -o " + shared_lib + " " +
            filename + " RobotBase.o -I. -std=c++20";

        if (std::system(compile_cmd.c_str()) != 0) {
            continue;
        }
```

### Step 19: Use `dlopen` and `dlsym`

Why:

- This is how the arena turns a compiled robot library into a live object.

Sample:

```cpp
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
```

### Step 20: Save the robot in `m_robots`

Why:

- You need to track the instance and the library handle.

Sample:

```cpp
        RobotEntry robot_entry;
        robot_entry.robot = robot;
        robot_entry.handle = handle;
        robot_entry.glyph = '@';
        robot_entry.alive = true;

        robot->set_boundaries(m_config.height, m_config.width);
        m_robots.push_back(robot_entry);
    }

    return !m_robots.empty();
}
```

You will probably want to assign different glyphs instead of always `'@'`.

## Part 7: Place Robots

### Step 21: Add a helper for safe spawn cells

Why:

- Robots must not spawn on obstacles or on each other.

Sample:

```cpp
bool Arena::is_cell_free_for_spawn(int row, int col) const
{
    if (!is_in_bounds(row, col)) {
        return false;
    }

    if (m_obstacles[row][col] != '.') {
        return false;
    }

    return find_robot_at(row, col, true) == -1;
}
```

### Step 22: Write `place_robots`

Why:

- This gives each robot a valid starting location.

Sample:

```cpp
void Arena::place_robots()
{
    std::uniform_int_distribution<int> row_dist(0, m_config.height - 1);
    std::uniform_int_distribution<int> col_dist(0, m_config.width - 1);

    for (RobotEntry& entry : m_robots) {
        while (true) {
            int row = row_dist(m_rng);
            int col = col_dist(m_rng);

            if (is_cell_free_for_spawn(row, col)) {
                entry.robot->move_to(row, col);
                break;
            }
        }
    }
}
```

## Part 8: Render the Board

### Step 23: Write `render_board`

Why:

- The printed board is your best debugging tool.

Sample:

```cpp
void Arena::render_board(int turn_number) const
{
    std::cout << "\n=========== starting round " << turn_number << " ===========\n";

    for (int row = 0; row < m_config.height; ++row) {
        for (int col = 0; col < m_config.width; ++col) {
            int robot_index = find_robot_at(row, col, true);

            if (robot_index >= 0) {
                const RobotEntry& entry = m_robots[robot_index];
                std::cout << (entry.alive ? 'R' : 'X') << entry.glyph << ' ';
            } else if (m_obstacles[row][col] != '.') {
                std::cout << m_obstacles[row][col] << "  ";
            } else {
                std::cout << ".  ";
            }
        }
        std::cout << '\n';
    }
}
```

## Part 9: Write the Game Loop

### Step 24: Add `main`

Why:

- The program should run with a config file path.

Sample:

```cpp
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
```

### Step 25: Start `run`

Why:

- This is the main round/turn loop of the game.

Sample:

```cpp
void Arena::run()
{
    int turn_number = 1;

    while (turn_number <= m_config.max_rounds) {
        for (std::size_t i = 0; i < m_robots.size(); ++i) {
            render_board(turn_number);
            handle_robot_turn(static_cast<int>(i), turn_number);
            ++turn_number;
        }
    }
}
```

This is just the starting shape. You will improve it with winner checks, logs, and sleeping.

### Step 26: Add `handle_robot_turn`

Why:

- Each robot’s turn should be handled in one place.

Sample:

```cpp
bool Arena::handle_robot_turn(int robot_index, int turn_number)
{
    RobotEntry& entry = m_robots[robot_index];

    if (!entry.alive) {
        return false;
    }

    int radar_direction = 0;
    entry.robot->get_radar_direction(radar_direction);

    std::vector<RadarObj> radar_results =
        scan_radar_for_robot(robot_index, radar_direction);

    entry.robot->process_radar_results(radar_results);

    int shot_row = 0;
    int shot_col = 0;
    if (entry.robot->get_shot_location(shot_row, shot_col)) {
        return handle_shot(robot_index, shot_row, shot_col);
    }

    int move_direction = 0;
    int move_distance = 0;
    entry.robot->get_move_direction(move_direction, move_distance);
    handle_move(robot_index, move_direction, move_distance);
    return true;
}
```

## Part 10: Implement Radar

### Step 27: Start a direction helper

Why:

- Radar and movement both need the same direction mapping.

Sample:

```cpp
std::pair<int, int> Arena::direction_to_delta(int direction) const
{
    return directions[direction];
}
```

### Step 28: Handle radar direction `0`

Why:

- Direction `0` is the spec’s special “look at the 8 surrounding cells” mode.

Sample:

```cpp
if (direction == 0) {
    for (int dir = 1; dir <= 8; ++dir) {
        auto [dr, dc] = direction_to_delta(dir);
        int row = origin_row + dr;
        int col = origin_col + dc;

        if (!is_in_bounds(row, col)) {
            continue;
        }
```

### Step 29: Add objects to the radar results

Why:

- The robot needs `RadarObj` records, not just characters.

Sample:

```cpp
        char cell_type = cell_type_for_radar(row, col, robot_index);
        if (cell_type != '.') {
            results.emplace_back(cell_type, row, col);
        }
    }

    return results;
}
```

### Step 30: Handle 3-wide directional scans

Why:

- Directions `1` through `8` must scan toward the edge of the board.

Sample:

```cpp
auto [dr, dc] = direction_to_delta(direction);
int side_row = dc;
int side_col = -dr;

for (int step = 1; step < std::max(m_config.height, m_config.width); ++step) {
    for (int offset = -1; offset <= 1; ++offset) {
        int row = origin_row + dr * step + side_row * offset;
        int col = origin_col + dc * step + side_col * offset;

        if (!is_in_bounds(row, col)) {
            continue;
        }
```

## Part 11: Implement Shooting

### Step 31: Start `handle_shot`

Why:

- The arena controls weapon effects and damage.

Sample:

```cpp
bool Arena::handle_shot(int robot_index, int shot_row, int shot_col)
{
    RobotEntry& shooter = m_robots[robot_index];
    WeaponType weapon = shooter.robot->get_weapon();

    std::vector<std::pair<int, int>> affected_cells;
```

### Step 32: Compute affected cells by weapon type

Why:

- Each weapon has its own geometry.

Sample:

```cpp
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
```

### Step 33: Damage robots in those cells

Why:

- Multiple robots may be hit by one attack.

Sample:

```cpp
    for (const auto& [row, col] : affected_cells) {
        int target_index = find_robot_at(row, col, false);
        if (target_index >= 0 && target_index != robot_index) {
            apply_damage_to_robot(target_index, roll_damage(weapon), "weapon hit");
        }
    }

    return true;
}
```

### Step 34: Apply armor reduction

Why:

- Damage must respect the target robot’s armor.

Sample:

```cpp
void Arena::apply_damage_to_robot(int target_index, int base_damage, const std::string& cause)
{
    RobotEntry& target = m_robots[target_index];
    int armor = target.robot->get_armor();
    double reduction = armor * 0.10;
    int actual_damage = static_cast<int>(std::round(base_damage * (1.0 - reduction)));

    target.robot->take_damage(actual_damage);
    target.robot->reduce_armor(1);

    if (target.robot->get_health() <= 0) {
        target.alive = false;
    }
}
```

## Part 12: Implement Movement

### Step 35: Start `handle_move`

Why:

- Movement is resolved by the arena, not the robot.

Sample:

```cpp
void Arena::handle_move(int robot_index, int direction, int distance)
{
    RobotEntry& entry = m_robots[robot_index];

    int current_row = 0;
    int current_col = 0;
    entry.robot->get_current_location(current_row, current_col);

    int capped_distance = std::min(distance, entry.robot->get_move_speed());
    auto [dr, dc] = direction_to_delta(direction);
```

### Step 36: Move one cell at a time

Why:

- That is how you correctly stop on pits, mounds, flames, and robots.

Sample:

```cpp
    for (int step = 1; step <= capped_distance; ++step) {
        int next_row = current_row + dr;
        int next_col = current_col + dc;

        if (!is_in_bounds(next_row, next_col)) {
            break;
        }
```

### Step 37: Stop on blockers, trigger special cells

Why:

- This is the rules logic for obstacles.

Sample:

```cpp
        if (find_robot_at(next_row, next_col, true) >= 0) {
            break;
        }

        char obstacle = m_obstacles[next_row][next_col];
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
        }
    }
}
```

## Part 13: Add Win Checks

### Step 38: Count living robots

Why:

- The game ends when one robot is left alive.

Sample:

```cpp
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
```

### Step 39: End the game when one robot remains

Why:

- This is the win condition from the spec.

Sample:

```cpp
if (living_robot_count() <= 1) {
    std::cout << "Game over.\n";
    return;
}
```

Add that check inside `run()`.

## Part 14: Build `Robot_Justin.cpp`

### Step 40: Add the includes

Why:

- Your robot uses the base class, radar objects, and some STL helpers.

Sample:

```cpp
#include "RobotBase.h"

#include <algorithm>
#include <vector>
```

### Step 41: Start the class

Why:

- Your robot must inherit from `RobotBase`.

Sample:

```cpp
class Robot_Justin : public RobotBase {
private:
    int m_target_row = -1;
    int m_target_col = -1;
    int m_next_radar_direction = 1;
    bool m_moving_down = true;
```

### Step 42: Add the constructor

Why:

- The constructor sets your build and your robot name.

Sample:

```cpp
public:
    Robot_Justin() : RobotBase(3, 4, railgun)
    {
        m_name = "Justin";
    }
```

### Step 43: Implement `get_radar_direction`

Why:

- Your robot needs to decide where to look next.

Sample:

```cpp
    void get_radar_direction(int& radar_direction) override
    {
        radar_direction = m_next_radar_direction;
        m_next_radar_direction = (m_next_radar_direction % 8) + 1;
    }
```

### Step 44: Implement `process_radar_results`

Why:

- This is where the robot picks a target.

Sample:

```cpp
    void process_radar_results(const std::vector<RadarObj>& radar_results) override
    {
        m_target_row = -1;
        m_target_col = -1;

        for (const RadarObj& obj : radar_results) {
            if (obj.m_type == 'R') {
                m_target_row = obj.m_row;
                m_target_col = obj.m_col;
                return;
            }
        }
    }
```

### Step 45: Implement `get_shot_location`

Why:

- The robot should shoot only when it has a target.

Sample:

```cpp
    bool get_shot_location(int& shot_row, int& shot_col) override
    {
        if (m_target_row == -1 || m_target_col == -1) {
            return false;
        }

        shot_row = m_target_row;
        shot_col = m_target_col;
        return true;
    }
```

### Step 46: Implement `get_move_direction`

Why:

- If the robot cannot shoot, it still needs a legal move plan.

Sample:

```cpp
    void get_move_direction(int& direction, int& distance) override
    {
        int current_row = 0;
        int current_col = 0;
        get_current_location(current_row, current_col);

        if (current_col > 0) {
            direction = 7;
            distance = std::min(get_move_speed(), current_col);
            return;
        }

        if (m_moving_down) {
            direction = 5;
            distance = 1;
        } else {
            direction = 1;
            distance = 1;
        }
    }
};
```

This is only a starter movement policy. You can make it smarter later.

### Step 47: Add the exports

Why:

- The arena loads the robot through these functions.

Sample:

```cpp
extern "C" RobotBase* create_robot()
{
    return new Robot_Justin();
}

extern "C" const char* robot_summary()
{
    return "Railgun scout that shoots visible enemies.";
}
```

## Part 15: Test in Small Pieces

### Step 48: Compile early and often

Why:

- Small compile errors are much easier to fix than giant ones.

Sample commands:

```bash
make
./test_robot Robot_Justin.cpp
./RobotWarz config.txt
```

### Step 49: Use temporary debug prints

Why:

- Debug text helps you verify your logic while building.

Sample:

```cpp
std::cout << "Loaded robot: " << robot->m_name << '\n';
std::cout << "Radar direction: " << radar_direction << '\n';
std::cout << "Shot target: " << shot_row << "," << shot_col << '\n';
```

### Step 50: Improve only after correctness

Why:

- A correct simple robot beats a broken “smart” robot every time.

What to improve later:

- smarter target selection
- better hazard memory
- stronger movement paths
- using move speed more effectively
- using radar direction `0` for nearby checks

## Final Reminder

If your code starts feeling messy, stop and split logic into helpers.

Good signs:

- `run()` mostly orchestrates
- `handle_shot()` mostly resolves attacks
- `handle_move()` mostly resolves movement
- `Robot_Justin.cpp` mostly chooses actions

That separation is what makes the project manageable.
