#include "RobotBase.h"

class Robot_Justin: public RobotBase {
    private:
        bool moving_down = true;
        int to_shoot_row = -1;
        int to_shoot_col = -1;
        std::vector<RadarObj> known_obstacles;

        bool is_obstacle(int row, int col) const {
            return std::any_of(known_obstacles.begin(), known_obstacles.end(), 
                            [&](const RadarObj& obj) {
                                return obj.row == row && obj.col == col;
                            });
        }

        void clear_target() {
            to_shoot_row = -1;
            to_shoot_col = -1;
        }

        void add_obstacle(const RadarObj& obj) {
            if ((obj.type == 'M' || obj.type == 'P' || obj.type == 'F') && 
                !is_obstacle(obj.row, obj.col)) 
            {
                known_obstacles.push_back(obj);
            }
        }

    public:
        Robot_Justin() : RobotBase(2, 5, railgun) {}
        virtual void get_radar_direction(int& radar_direction) override {
            int current_row, current_col;
            get_current_location(current_row, current_col);
            radar_direction = (current_col > 0) ? 7 : 3;
        }

        virtual void process_radar_results(const std::vector<RadarObj>& radar_results) override {
            clear_target();

            for (const auto& obj : radar_results) {
                add_obstacle(obj);
                if (obj.type == 'R' && to_shoot_row == -1 && to_shoot_col == -1) 
                {
                    to_shoot_row = obj.row;
                    to_shoot_col = obj.col;
                }
            }
        }

        virtual bool get_shot_location(int& shot_row, int& shot_col) override {
            if (to_shoot_row != -1 && to_shoot_col != -1) 
            {
                shot_row = to_shoot_row;
                shot_col = to_shoot_col;
                clear_target();
                return true;
            }
            return false;
        }

    void get_move_direction(int& move_direction, int& move_distance) override {
        int current_row, current_col;
        get_current_location(current_row, current_col);
        int move = get_move_speed();

        if (current_col > 0) {
            move_direction = 7;
            move_distance = std::min(move, current_col);
            return;
        }

        if (moving_down) {
            if (current_row + move < board_row_max) {
                move_direction = 5;
                move_distance = std::min(move, board_row_max - current_row - 1);
            } else {
                moving_down = false;
                move_direction = 1;
                move_distance = 1;
            }
        } else {
            if (current_row - move >= 0) {
                move_direction = 1;
                move_distance = std::min(move, current_row);
            } else {
                moving_down = true;
                move_direction = 5;
                move_distance = 1;
            }
        }
    }
}

extern "C" RobotBase* create_robot() {
    return new Robot_Justin();
}

extern "C" const char* robot_summary() {
    return "It'll do a thing, probably. If he doesn't...uh...who else noticed how handsome professor Wastlund looks without a beard?"
}