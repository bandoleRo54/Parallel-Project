#ifndef ECOSYSTEM_H
#define ECOSYSTEM_H

#include <vector>
#include <memory>
#include <map>
#include <iostream>
using namespace std;

// Forward‑declare Entity so World can hold unique_ptr<Entity>
class Entity;

// Cell & Rock must be known to World
struct Cell {
  enum Type { EMPTY, ROCK, RABBIT, FOX } type = EMPTY;
  Entity* occupant = nullptr;
};
struct Rock { };

// World 
class World {
public:
  int R,C;
  int GEN_PROC_RABBITS, GEN_PROC_FOXES, GEN_FOOD_FOXES, N_GEN;
  vector<vector<Cell>> grid;
  vector<unique_ptr<Entity>> allAnimals;

  World(int gr, int gf, int ff, int ngen, int rows, int cols);
  void loadFromStdin();
  void runSimulation();
  void outputFinalState();

private:
  template<typename T> void step(Cell::Type type, int gen, int GEN_PROC);
  void cleanupStarved();
  void placeRock(int x,int y);
  void placeRabbit(int x,int y);
  void placeFox(int x,int y);
};

// Now Entity and its subclasses (they know World exists)
class Entity {
public:
  int x,y, sinceBorn, sinceReproduce;
  virtual ~Entity(){}
  virtual pair<int,int> decideMove(World& w, int gen)=0;
  virtual unique_ptr<Entity> maybeReproduce(int GEN_PROC)=0;
};

class Rabbit : public Entity {
public:
  Rabbit(int x,int y);
  pair<int,int> decideMove(World& w,int gen) override;
  unique_ptr<Entity> maybeReproduce(int GEN_PROC) override;
};

class Fox : public Entity {
public:
  int sinceEat;
  Fox(int x,int y);
  pair<int,int> decideMove(World& w,int gen) override;
  unique_ptr<Entity> maybeReproduce(int GEN_PROC) override;
};

#endif // ECOSYSTEM_H
