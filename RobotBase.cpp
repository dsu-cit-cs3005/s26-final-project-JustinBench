#include "RobotBase.h"
#include <iostream>
#include <string>
#include <sstream>


//overload the << operator to print the weapon type - handy.
std::ostream& operator<<(std::ostream& os, const WeaponType& weapon)
{
    switch (weapon)
    {
        case flamethrower: os << "flamethrower"; break;
        case railgun:      os << "railgun";      break;
        case grenade:      os << "grenade";      break;
        case hammer:       os << "hammer";       break;
        default:           os << "unknown";      break;
    }

    return os;
}

// Constructor - Notice that you can't set move speed more than 5
RobotBase::RobotBase(int move_in, int armor_in, WeaponType weapon_in)
    : m_health(100), m_weapon(weapon_in), m_name("Blank_Robot")
{
    //set the number of starting grenades
    m_grenades = 0;
    if(weapon_in == grenade)
    {
        m_grenades = 15;
    }


    // Validate move input
    if (move_in < 2)
    {
        m_move = 2;
    }
    else if (move_in > 5)
    {
        m_move = 5;
    }
    else
    {
        m_move = move_in;
    }

    // Calculate maximum armor based on the move value
    int max_armor = 7 - m_move;

    // Validate armor input
    if (armor_in < 0)
    {
        m_armor = 0;
    }
    else if (armor_in > max_armor)
    {
        m_armor = max_armor;
    }
    else
    {
        m_armor = armor_in;
    }

    // blank out location
    m_location_row = 0;
    m_location_col = 0;

}

// Getters - because you're not allowed to manipulate the robots internal data directly.
// this is a good example of why you need private member variables.

// Get the robot's current health
int RobotBase::get_health()
{
    return m_health;
}

// Get the robot's armor level
int RobotBase::get_armor()
{
    return m_armor;
}

// Get the robot's movement range
int RobotBase::get_move_speed()
{
    return m_move;
}

// Get the robot's weapon type
WeaponType RobotBase::get_weapon()
{
    return m_weapon;
}

int RobotBase::get_grenades()
{
    return m_grenades;
}

void RobotBase::decrement_grenades()
{
    m_grenades--;
    if(m_grenades < 0)
        m_grenades = 0;

}

// Get the robot's current location
void RobotBase::get_current_location(int& current_row, int& current_col)
{
    current_row = m_location_row;
    current_col = m_location_col;
}


// Apply damage to the robot and reduce its health
int RobotBase::take_damage(int damage_in)
{
    m_health -= damage_in;
    if (m_health < 0)
    {
        m_health = 0; 
    }
    return m_health;
}

// Set the robot's next location
void RobotBase::move_to(int new_row, int new_col)
{
    m_location_row = new_row;
    m_location_col = new_col;
}

// Disable the robot's movement
void RobotBase::disable_movement()
{
    m_move = 0;
}

void RobotBase::reduce_armor(int amount)
{
    m_armor = m_armor - amount;
    if(m_armor < 0)
        m_armor = 0;

}

//set the arena size
void RobotBase::set_boundaries(int row_max, int col_max)
{
    m_board_row_max = row_max;
    m_board_col_max = col_max;
}

std::string RobotBase::print_stats() const {

    // Construct the robot's statistics as a string
    std::ostringstream stats;
    stats << m_name << ": ";
    stats << "  H: " << m_health;
    stats << "  W: " << m_weapon;
    stats << "  A: " << m_armor;
    stats << "  M: " << m_move;
    stats << "  at: (" << m_location_row << "," << m_location_col << ") ";

    return stats.str();
}


// Destructor
RobotBase::~RobotBase()
{
    // No additional cleanup required
}