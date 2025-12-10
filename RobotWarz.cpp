#include "Arena.h"

int main() {
    Arena a(20, 20);
    a.loadAllRobots();
    a.runSimulation();
    return 0;
}

