#include "RobotBase.h"

#include <algorithm>
#include <vector>

class Robot_Justin : public RobotBase {
    private:
        int target_row = -1;
        int target_col = -1;
        int next_radar_direction = 1;
        bool moving_down = true;
    public:
        Robot_Justin() : RobotBase(3, 4, railgun) {
            m_name = "Justin";
        }
        void get_radar_direction(int& radar_direction) override {
            radar_direction = next_radar_direction;
            next_radar_direction = (next_radar_direction % 8) + 1;
        }
        void process_radar_results(const std::vector<RadarObj>& radar_results) override {
            target_row = -1;
            target_col = -1;
            for (const auto& obj : radar_results) {
                if (obj.m_type == 'R') {
                    target_row = obj.m_row;
                    target_col = obj.m_col;
                    return;
                }
            }
        }
        bool get_shot_location(int& shot_row, int& shot_col) override {
            if (target_row == -1 || target_col == -1) {
                return false;
            }
            shot_row = target_row;
            shot_col = target_col;
            return true;
        }
        void get_move_direction(int& direction, int& distance) override {
            int current_row = 0;
            int current_col = 0;
            get_current_location(current_row, current_col);
            if (current_col > 0) {
                direction = 7;
                distance = std::min(get_move_speed(), current_col);
                return;
            }

            if (moving_down) {
                if (current_row >= m_board_row_max - 1) {
                    moving_down = false;
                    direction = 1;
                    distance = 1;
                    return;
                }
                direction = 5;
                distance = 1;
            } else {
                if (current_row <= 0) {
                    moving_down = true;
                    direction = 5;
                    distance = 1;
                    return;
                }
                direction = 1;
                distance = 1;
            }
        }
};

extern "C" RobotBase* create_robot() {
    return new Robot_Justin();
}

extern "C" const char* robot_summary() {
    return "It does a thing, probably. If it doesn't...uh...did you all notice how handsome Professor Wastlund looks without his beard?";
}
