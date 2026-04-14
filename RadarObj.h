#pragma once

// This represents a 'cell' in the board. it contains the 'char' indicating what is at that location
// and the row and column. That's it. Don't add to this or change it and everyone's robots can work together.
struct RadarObj 
{

    char m_type;  // 'X', 'R', 'M', 'F', 'P' (Dead Robot, Live Robot, Mound, Flamethrower, Pit)
    int m_row;    // Row of the object
    int m_col;    // Column of the object

    // default constructor so you can make an empty one and fill it.
    RadarObj() {} 
    RadarObj(char type, int row, int col) : m_type(type), m_row(row), m_col(col) {}
};