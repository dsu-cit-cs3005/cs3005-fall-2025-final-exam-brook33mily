#pragma once
#include <vector>
#include <string>
#include <dlfcn.h>
#include "RobotBase.h"
#include "RadarObj.h"

// Obstacles
enum class ObstacleType { Empty, FlameTrap, Pit, Mound };

struct LoadedRobot {
    RobotBase* robot = nullptr;
    void* handle = nullptr;     // dlopen handle
    int row = 0, col = 0;
    bool alive = true;
};

class Arena {
public:
    Arena(int rows = 20, int cols = 20);
    ~Arena();

    void loadAllRobots();
    void runSimulation();


private:
    int m_rows, m_cols;
    std::vector<std::vector<ObstacleType>> board;
    std::vector<LoadedRobot> robots;

    void placeObstacles();
    void placeRobotsRandomly();

    std::vector<RadarObj> buildRadar(const LoadedRobot& lr) const;

    void doMovement(LoadedRobot& lr);
    void doShooting(LoadedRobot& lr);

    void applyDamage(LoadedRobot& target, WeaponType w);
    bool inBounds(int r, int c) const;

    LoadedRobot* robotAt(int r, int c);
    const LoadedRobot* robotAt(int r, int c) const; 

    void printBoard(int round) const;
    bool gameOver() const;

    void compileRobot(const std::string& file);
    RobotBase* loadRobotSO(const std::string& sofile, void*& handle);


};

