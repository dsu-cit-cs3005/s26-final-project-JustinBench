#pragma once

#include <random>
#include <string>
#include <utility>
#include <vector>

#include "RobotBase.h"

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

    Arena();

    bool load_config(const std::string& config_path);
    bool initialize();
    void run();

private:
    struct RobotEntry {
        RobotBase* robot = nullptr;
        void* handle = nullptr;
        char glyph = '?';
        bool alive = true;
        int grenades_remaining = 10;
        std::string shared_lib_path;
    };

    Config m_config;
    std::vector<RobotEntry> m_robots;
    std::vector<std::vector<char>> m_obstacles;
    std::vector<std::string> m_turn_log;
    std::mt19937 m_rng;

    bool load_robots();
    void place_obstacles();
    void place_robots();
    bool is_in_bounds(int row, int col) const;
    bool is_cell_free_for_spawn(int row, int col) const;
    int find_robot_at(int row, int col, bool include_dead = true) const;
    char cell_type_for_radar(int row, int col, int requesting_index) const;
    std::vector<RadarObj> scan_radar_for_robot(int robot_index, int direction) const;

    bool handle_robot_turn(int robot_index, int turn_number);
    bool handle_shot(int robot_index, int shot_row, int shot_col);
    void handle_move(int robot_index, int direction, int distance);
    void apply_damage_to_robot(int target_index, int base_damage, const std::string& cause);
    int roll_damage(WeaponType weapon);

    std::vector<std::pair<int, int>> compute_line_cells(int start_row,
                                                        int start_col,
                                                        int target_row,
                                                        int target_col) const;
    std::vector<std::pair<int, int>> compute_flamethrower_cells(int start_row,
                                                                int start_col,
                                                                int target_row,
                                                                int target_col) const;
    std::vector<std::pair<int, int>> compute_grenade_cells(int target_row, int target_col) const;
    std::pair<int, int> compute_hammer_cell(int start_row, int start_col, int target_row, int target_col) const;
    std::pair<int, int> direction_to_delta(int direction) const;
    std::pair<int, int> target_to_octant(int from_row, int from_col, int to_row, int to_col) const;

    int living_robot_count() const;
    void render_board(int turn_number) const;
};
