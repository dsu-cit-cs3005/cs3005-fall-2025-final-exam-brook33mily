#include "RobotBase.h"
#include <cstdlib>
#include <ctime>
#include <set>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

// Optional: uncomment if you still want debug logs
// #include <iostream>

class Robot_SniperBrooke : public RobotBase
{
private:
    bool target_found = false;
    int target_row = -1;
    int target_col = -1;

    int radar_direction = 1;      // Radar scanning direction (1-8)
    bool fixed_radar = false;     // Tracks whether radar is locked on a target
    const int max_range = 20;     // Railgun can reach across the arena; use a big range

    std::set<std::pair<int, int>> obstacles_memory; // Memory of obstacles

    // Helper: Manhattan distance
    int calculate_distance(int row1, int col1, int row2, int col2) const
    {
        return std::abs(row1 - row2) + std::abs(col1 - col2);
    }

    // Update memory of obstacles
    void update_obstacle_memory(const std::vector<RadarObj>& radar_results)
    {
        for (const auto& obj : radar_results)
        {
            if (obj.m_type == 'M' || obj.m_type == 'P' || obj.m_type == 'F')
            {
                obstacles_memory.insert({obj.m_row, obj.m_col});
            }
        }
    }

    // For a railgun: only care about enemy robots in the same row or column
    void find_closest_enemy(const std::vector<RadarObj>& radar_results,
                            int current_row, int current_col)
    {
        target_found = false;
        int closest_distance = std::numeric_limits<int>::max();

        for (const auto& obj : radar_results)
        {
            // Only consider enemy robots
            if (obj.m_type != 'R')
            {
                continue;
            }

            // Only straight-line targets: same row OR same column
            if (obj.m_row != current_row && obj.m_col != current_col)
            {
                continue; // skip diagonals
            }

            int distance = calculate_distance(current_row, current_col,
                                              obj.m_row, obj.m_col);

            if (distance <= max_range && distance > 0 && distance < closest_distance)
            {
                closest_distance = distance;
                target_row = obj.m_row;
                target_col = obj.m_col;
                target_found = true;
                fixed_radar = true;    // lock onto this target
            }
        }
    }

public:
    // move_speed = 0, armor = 4, weapon = railgun
    Robot_SniperBrooke() : RobotBase(0, 4, railgun)
    {
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
        // std::cout << "[SniperBrooke] constructed\n";
    }

    // Always scan; if we have a locked target, keep same direction; otherwise cycle
    virtual void get_radar_direction(int& radar_direction_out) override
    {
        if (fixed_radar && target_found)
        {
            radar_direction_out = radar_direction;
        }
        else
        {
            radar_direction_out = radar_direction;
            radar_direction = (radar_direction % 8) + 1; // 1..8 wraparound
        }

        // std::cout << "[SniperBrooke] get_radar_direction -> " << radar_direction_out << "\n";
    }

    // Use radar info to track obstacles and pick a target
    virtual void process_radar_results(const std::vector<RadarObj>& radar_results) override
    {
        target_found = false;
        int current_row = 0, current_col = 0;
        get_current_location(current_row, current_col);

        update_obstacle_memory(radar_results);
        find_closest_enemy(radar_results, current_row, current_col);

        if (!target_found)
        {
            fixed_radar = false;
        }

        // std::cout << "[SniperBrooke] process_radar_results, target_found=" << target_found << "\n";
    }

    // Sniper shoots directly at the locked target if there is one
    virtual bool get_shot_location(int& shot_row, int& shot_col) override
    {
        // std::cout << "[SniperBrooke] get_shot_location (target_found=" << target_found << ")\n";

        if (!target_found)
        {
            return false; // no shot this turn
        }

        shot_row = target_row;
        shot_col = target_col;
        return true;
    }

    // Sniper does not move
    virtual void get_move_direction(int& move_direction, int& move_distance) override
    {
        move_direction = 0;
        move_distance  = 0;
        // std::cout << "[SniperBrooke] get_move_direction (stay put)\n";
    }
};

// Factory function visible from the shared library
extern "C" RobotBase* create_robot()
{
    return new Robot_SniperBrooke();
}

