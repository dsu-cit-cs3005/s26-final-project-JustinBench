#include "RobotBase.h"

class Robot_Justin: public RobotBase {
    public:
        
    private:

}

extern "C" RobotBase* create_robot() {
    return new Robot_Justin();
}

extern "C" const char* robot_summary() {
    return "It'll do a thing, probably. If he doesn't...uh...who else noticed how handsome professor Wastlund looks without a beard?"
}