#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <utility>

#include "RadarObj.h"

// this works for both movement and radar checks
constexpr std::pair<int, int> directions[] = 
{
    {0, 0},   // index 0 - placeholder
    {-1, 0},  // 1: Up
    {-1, 1},  // 2: Up-right
    {0, 1},   // 3: Right
    {1, 1},   // 4: Down-right
    {1, 0},   // 5: Down
    {1, -1},  // 6: Down-left
    {0, -1},  // 7: Left
    {-1, -1}  // 8: Up-left
};

// read about enums these are really just ints. 0-3.
enum WeaponType { flamethrower, railgun, grenade, hammer };

// don't change anything in here. Understand it though...
class RobotBase 
{
private:
    int m_health;
    int m_armor;
    int m_move;

    WeaponType m_weapon;
    int m_grenades;

    int m_location_row;
    int m_location_col;

public:

    int m_board_row_max;
    int m_board_col_max;
    std::string m_name;
    char m_character;

    RobotBase(int move_in, int armor_in, WeaponType weapon_in);

    int get_health();
    int get_armor();
    int get_move_speed();
    int get_grenades();
    WeaponType get_weapon();
    void set_boundaries(int row_max, int col_max);

    // final methods (final means that these cannot be overridden)
    virtual void get_current_location(int& current_row, int& current_col) final;
    virtual int take_damage(int damage_in) final;
    virtual void move_to(int new_row, int new_col) final;
    virtual void disable_movement() final;
    virtual void reduce_armor(int amount) final;
    virtual void decrement_grenades() final;
    virtual std::string print_stats() const  final;

    // Pure virtual methods ( = 0 means that they must be implemented by derived classes)
    virtual void get_radar_direction(int& radar_direction) = 0;
    virtual void process_radar_results(const std::vector<RadarObj>& radar_results) = 0;
    virtual bool get_shot_location(int& shot_row, int& shot_col) = 0;
    virtual void get_move_direction(int &direction,int &distance) = 0;

    // Virtual destructor
    virtual ~RobotBase();
};


// to aid in the creation of the robots as shared objects.
typedef RobotBase* (*RobotFactory)();


