#include "Arena.h"
#include <iostream>
#include <filesystem>
#include <random>
#include <cstdlib>

using namespace std;

Arena::Arena(int rows, int cols) : m_rows(rows), m_cols(cols) {
    board.resize(rows, vector<ObstacleType>(cols, ObstacleType::Empty));
    placeObstacles();
}

Arena::~Arena() {
    for (auto& lr : robots) {
        if (lr.robot) delete lr.robot;
        if (lr.handle) dlclose(lr.handle);
    }
}

bool Arena::inBounds(int r, int c) const {
    return r >= 0 && r < m_rows && c >= 0 && c < m_cols;
}

LoadedRobot* Arena::robotAt(int r, int c) {
    for (auto& lr : robots)
        if (lr.row == r && lr.col == c && lr.alive)
            return &lr;
    return nullptr;
}


const LoadedRobot* Arena::robotAt(int r, int c) const {
    for (const auto& lr : robots)
        if (lr.row == r && lr.col == c && lr.alive)
            return &lr;
    return nullptr;
}

// ---------------- OBSTACLES ------------------
void Arena::placeObstacles() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, 99);

    for (int r = 0; r < m_rows; r++) {
        for (int c = 0; c < m_cols; c++) {

            int roll = dist(gen);

            if (roll < 3)
                board[r][c] = ObstacleType::FlameTrap;
            else if (roll < 5)
                board[r][c] = ObstacleType::Pit;
            else if (roll < 8)
                board[r][c] = ObstacleType::Mound;
        }
    }
}

// ---------------- ROBOT LOADING ------------------
void Arena::compileRobot(const string& file) {
    string name = file.substr(0, file.find(".cpp"));
    string sofile = "lib" + name + ".so";

    string cmd =
        "g++ -shared -fPIC -std=c++20 -o " + sofile + " " + file + " RobotBase.o -I.";

    cout << "Compiling " << file << "...\n";
    system(cmd.c_str());
}

RobotBase* Arena::loadRobotSO(const string& sofile, void*& handle) {
    handle = dlopen(sofile.c_str(), RTLD_LAZY);
    if (!handle) {
        cerr << "Failed to load " << sofile << ": " << dlerror() << "\n";
        return nullptr;
    }

    RobotFactory create_robot = (RobotFactory)dlsym(handle, "create_robot");
    if (!create_robot) {
        cerr << "Missing create_robot() in " << sofile << "\n";
        dlclose(handle);
        return nullptr;
    }
    return create_robot();
}

// Find all Robot_*.cpp and load
void Arena::loadAllRobots() {
    namespace fs = std::filesystem;

    for (const auto& f : fs::directory_iterator(".")) {
        if (!f.is_regular_file()) continue;

        string name = f.path().filename().string();

        if (name.rfind("Robot_", 0) == 0 && name.ends_with(".cpp")) {
            compileRobot(name);

            string sofile = "lib" + name.substr(0, name.find(".cpp")) + ".so";
            LoadedRobot lr;

            lr.robot = loadRobotSO(sofile, lr.handle);
            if (!lr.robot) continue;

            lr.robot->set_boundaries(m_rows, m_cols);
            robots.push_back(lr);
        }
    }

    placeRobotsRandomly();
}

// ---------------- PLACE ROBOTS ------------------
void Arena::placeRobotsRandomly() {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> distR(0, m_rows - 1);
    uniform_int_distribution<> distC(0, m_cols - 1);

    for (auto& lr : robots) {
        while (true) {
            int r = distR(gen);
            int c = distC(gen);

            if (!robotAt(r, c) && board[r][c] == ObstacleType::Empty) {
                lr.row = r;
                lr.col = c;
                lr.robot->move_to(r, c);
                break;
            }
        }
    }
}

// ------------------ RADAR --------------------
vector<RadarObj> Arena::buildRadar(const LoadedRobot& lr) const {
    vector<RadarObj> v;

    for (int r = 0; r < m_rows; r++) {
        for (int c = 0; c < m_cols; c++) {
            char type = '.';

            if (robotAt(r, c))
                type = 'R';
            else {
                switch (board[r][c]) {
                case ObstacleType::FlameTrap: type = 'F'; break;
                case ObstacleType::Pit: type = 'P'; break;
                case ObstacleType::Mound: type = 'M'; break;
                default: break;
                }
            }
            v.emplace_back(type, r, c);
        }
    }

    return v;
}

// ------------------ MOVEMENT --------------------
void Arena::doMovement(LoadedRobot& lr) {
    if (!lr.alive) return;

    int dir = 0, dist = 0;
    lr.robot->get_move_direction(dir, dist);

    if (dist == 0 || dir == 0) return;

    auto [dr, dc] = directions[dir];

    for (int i = 0; i < dist; i++) {
        int nr = lr.row + dr;
        int nc = lr.col + dc;

        if (!inBounds(nr, nc)) break;

        if (robotAt(nr, nc)) break;

        if (board[nr][nc] == ObstacleType::Mound) break;

        lr.row = nr;
        lr.col = nc;
        lr.robot->move_to(nr, nc);

        // Pit? robot stuck permanently
        if (board[nr][nc] == ObstacleType::Pit) {
            lr.robot->disable_movement();
            break;
        }

        // Flame trap damage
        if (board[nr][nc] == ObstacleType::FlameTrap) {
            applyDamage(lr, WeaponType::flamethrower);
        }
    }
}

// ------------------ SHOOTING --------------------
void Arena::applyDamage(LoadedRobot& target, WeaponType w) {
    if (!target.alive) return;

    int base = 10;
    if (w == WeaponType::railgun) base = 15;
    if (w == WeaponType::hammer) base = 20;
    if (w == WeaponType::flamethrower) base = 12;
    if (w == WeaponType::grenade) base = 18;

    if (target.robot->get_armor() > 0) {
        target.robot->reduce_armor(1);
        base -= 4;
    }
    if (base < 1) base = 1;

    int newHP = target.robot->take_damage(base);
    if (newHP <= 0) target.alive = false;
}

void Arena::doShooting(LoadedRobot& lr) {
    if (!lr.alive) return;

    int sr, sc;
    if (!lr.robot->get_shot_location(sr, sc)) return;

    WeaponType w = lr.robot->get_weapon();

    // ----- Flamethrower -----
    if (w == WeaponType::flamethrower) {
        int r0 = lr.row, c0 = lr.col;

        for (int dr = -1; dr <= 1; dr++) {
            for (int i = 1; i <= 4; i++) {
                int nr = r0 + i;
                int nc = c0 + dr;
                if (inBounds(nr, nc)) {
                    if (auto* t = robotAt(nr, nc)) {
                        applyDamage(*t, w);
                    }
                }
            }
        }
    }

    // ----- Hammer -----
    else if (w == WeaponType::hammer) {
        if (LoadedRobot* t = robotAt(sr, sc)) {
            applyDamage(*t, w);
        }
    }

    // ----- Railgun -----
    else if (w == WeaponType::railgun) {
        int r0 = lr.row, c0 = lr.col;

        int dr = (sr > r0) ? 1 : (sr < r0 ? -1 : 0);
        int dc = (sc > c0) ? 1 : (sc < c0 ? -1 : 0);

        int r = r0 + dr, c = c0 + dc;
        while (inBounds(r, c)) {
            if (auto* t = robotAt(r, c)) applyDamage(*t, w);
            r += dr;
            c += dc;
        }
    }

    // ----- Grenade -----
    else if (w == WeaponType::grenade) {
        if (lr.robot->get_grenades() == 0) return;

        lr.robot->decrement_grenades();

        for (int dr = -1; dr <= 1; dr++)
            for (int dc = -1; dc <= 1; dc++)
                if (inBounds(sr + dr, sc + dc))
                    if (auto* t = robotAt(sr + dr, sc + dc))
                        applyDamage(*t, w);
    }
}

// ------------------ BOARD PRINTING -----------------
void Arena::printBoard(int round) const {
    cout << "\n=========== ROUND " << round << " ===========\n";
    cout << "   ";
    for (int c = 0; c < m_cols; c++) cout << c % 10 << " ";
    cout << "\n";

    for (int r = 0; r < m_rows; r++) {
        cout << (r < 10 ? " " : "") << r << " ";
        for (int c = 0; c < m_cols; c++) {
            const LoadedRobot* rob = nullptr;
            for (auto& lr : robots)
                if (lr.row == r && lr.col == c && lr.alive)
                    rob = &lr;

            if (rob) cout << 'R' << " ";
            else if (board[r][c] == ObstacleType::FlameTrap) cout << "F ";
            else if (board[r][c] == ObstacleType::Pit) cout << "P ";
            else if (board[r][c] == ObstacleType::Mound) cout << "M ";
            else cout << ". ";
        }
        cout << "\n";
    }
}

// ------------------ SIMULATION LOOP -----------------
bool Arena::gameOver() const {
    int aliveCount = 0;
    for (auto& lr : robots)
        if (lr.alive) aliveCount++;
    return aliveCount <= 1;
}

void Arena::runSimulation() {
    int round = 0;
    while (!gameOver() && round < 500) {
        printBoard(round);

        for (auto& lr : robots) {
            if (!lr.alive) continue;

            // Radar
            auto radar = buildRadar(lr);
            int dir;
            lr.robot->get_radar_direction(dir);
            lr.robot->process_radar_results(radar);

            doMovement(lr);
            doShooting(lr);
        }

        round++;
    }

    printBoard(round);
    cout << "\n===== GAME OVER =====\n";

    for (auto& lr : robots)
        if (lr.alive)
            cout << "WINNER: " << lr.robot->print_stats() << "\n";
}

