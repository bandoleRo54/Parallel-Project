#include "ecosystem.h"

int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    World world(0,0,0,0,0,0);
    world.loadFromStdin();
    world.runSimulation();
    world.outputFinalState();
    return 0;
}
