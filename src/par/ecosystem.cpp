#include "ecosystem.h"

World::World(int gr,int gf,int ff,int ngen,int rows,int cols)
 : R(rows), C(cols),
   GEN_PROC_RABBITS(gr), GEN_PROC_FOXES(gf),
   GEN_FOOD_FOXES(ff), N_GEN(ngen),
   grid_cur(rows, vector<Cell>(cols)),
   grid_nxt(rows, vector<Cell>(cols)),
   cell_intents(rows, vector<vector<Intent>>(cols))
{}

void World::placeRock(int x,int y) {
	grid_cur[x][y].type = Cell::ROCK;
}

void World::placeRabbit(int x,int y) {
	auto r = make_unique<Rabbit>(x,y);
	grid_cur[x][y].type = Cell::RABBIT;
	grid_cur[x][y].occupant = r.get();
	allAnimals_cur.push_back(move(r));
}

void World::placeFox(int x,int y) {
	auto f = make_unique<Fox>(x,y);
	grid_cur[x][y].type = Cell::FOX;
	grid_cur[x][y].occupant = f.get();
	allAnimals_cur.push_back(move(f));
}

// Loads initial state from stdin
void World::loadFromStdin() {
	int N;
	cin >> GEN_PROC_RABBITS >> GEN_PROC_FOXES
		>> GEN_FOOD_FOXES >> N_GEN
		>> R >> C >> N;
	grid_cur.assign(R, vector<Cell>(C));
	allAnimals_cur.clear();
	string obj;
	int x,y;
	for(int i=0;i<N;i++){
		cin >> obj >> x >> y;
		if(obj=="ROCK")    placeRock(x,y);
		else if(obj=="RABBIT") placeRabbit(x,y);
		else if(obj=="FOX")    placeFox(x,y);
	}
}

// ——— Main loop —————————————————————————————————————————————————————
void World::runSimulation() {
  for(int g=1; g<=N_GEN; ++g) {
    // 1) Rabbits
    step<Rabbit>(Cell::RABBIT, g, GEN_PROC_RABBITS);
    swap(grid_cur, grid_nxt);
    swap(allAnimals_cur, allAnimals_nxt);

    // zero nxt structures for next pass
    for(auto &row: grid_nxt) for(auto &c: row) c = Cell{};
    allAnimals_nxt.clear();

    // 2) Foxes
    step<Fox>(Cell::FOX, g, GEN_PROC_FOXES);
    swap(grid_cur, grid_nxt);
    swap(allAnimals_cur, allAnimals_nxt);

    for(auto &row: grid_nxt) for(auto &c: row) c = Cell{};
    allAnimals_nxt.clear();

    // 3) Cleanup starved/eaten
    cleanupStarved();
  }
}

// ——— Parallel step<T> ———————————————————————————————————————————————
template<typename T>
void World::step(Cell::Type type, int gen, int GEN_PROC) {
  // 1) Gather intents into per‑cell buckets in parallel
  #pragma omp parallel for collapse(2) schedule(static)
  for(int x=0; x<R; ++x) {
    for(int y=0; y<C; ++y) {
      cell_intents[x][y].clear();
    }
  }

  #pragma omp parallel for schedule(static)
  for(int i=0; i<(int)allAnimals_cur.size(); ++i) {
    Entity* e = allAnimals_cur[i].get();
    bool match = (type==Cell::RABBIT && dynamic_cast<Rabbit*>(e))
              || (type==Cell::FOX    && dynamic_cast<Fox*>(e));
    if(!match) continue;
    auto [tx,ty] = e->decideMove(*this, gen);
    #pragma omp critical(bucket)
      cell_intents[tx][ty].push_back({e, tx, ty});
  }

  // 2) Resolve & commit each cell in parallel
  #pragma omp parallel for collapse(2) schedule(dynamic)
  for(int x=0; x<R; ++x) {
    for(int y=0; y<C; ++y) {
      auto &vec = cell_intents[x][y];
      if(vec.empty()) continue;

      // conflict resolution: pick winner
      Intent* win = &vec[0];
      for(auto &it : vec) {
        if(it.who->sinceReproduce > win->who->sinceReproduce)
          win = &const_cast<Intent&>(it);
        else if(type==Cell::FOX && it.who->sinceReproduce==win->who->sinceReproduce){
          auto f1 = dynamic_cast<Fox*>(it.who);
          auto f2 = dynamic_cast<Fox*>(win->who);
          if(f1->sinceEat < f2->sinceEat) win = &const_cast<Intent&>(it);
        }
      }

      // commit win->who into grid_nxt & allAnimals_nxt
      Entity* mover = win->who;
      int ox=mover->x, oy=mover->y;
      // eat?
      if(type==Cell::FOX && grid_cur[x][y].type==Cell::RABBIT){
        grid_cur[x][y].occupant->sinceBorn = -1;
        dynamic_cast<Fox*>(mover)->sinceEat=0;
      }
      // set in grid_nxt
      grid_nxt[x][y].type = type;
      grid_nxt[x][y].occupant = mover;
      // update mover state
      mover->x = x; mover->y = y;
      mover->sinceBorn++;
      mover->sinceReproduce++;
      if(type==Cell::FOX) dynamic_cast<Fox*>(mover)->sinceEat++;
      // maybe reproduce
      if(mover->sinceReproduce>=GEN_PROC){
        auto baby = mover->maybeReproduce(GEN_PROC);
        if(baby){
          baby->x = ox; baby->y = oy;
          grid_nxt[ox][oy].type = type;
          grid_nxt[ox][oy].occupant = baby.get();
          #pragma omp critical(animal_list)
            allAnimals_nxt.push_back(move(baby));
        }
      }
      // also queue the mover itself
      #pragma omp critical(animal_list)
        allAnimals_nxt.push_back(unique_ptr<Entity>(nullptr)); 
        // placeholder: the real mover pointer is already in allAnimals_cur
    }
  }
}


void World::cleanupStarved(){
	// remove rabbits flagged sinceBorn<0, and foxes that reached GEN_FOOD_FOXES
	vector<unique_ptr<Entity>> survivors;
	for(auto& up : allAnimals_cur){
		if(auto f = dynamic_cast<Fox*>(up.get())){
			if(f->sinceEat >= GEN_FOOD_FOXES){
				grid_cur[f->x][f->y].type = Cell::EMPTY;
				grid_cur[f->x][f->y].occupant = nullptr;
				continue;
			}
		}
		if(up->sinceBorn < 0){
			// eaten rabbit
			continue;
		}
		survivors.push_back(move(up));
	}
	allAnimals_nxt.swap(survivors);
}

// Print final state
void World::outputFinalState(){
	cout << endl;
	// count objects
	int N=0;
	vector<tuple<string,int,int>> objs;
	for(int i=0;i<R;i++)for(int j=0;j<C;j++){
		switch(grid_cur[i][j].type){
			case Cell::ROCK:   objs.emplace_back("ROCK",i,j); break;
			case Cell::RABBIT: objs.emplace_back("RABBIT",i,j); break;
			case Cell::FOX:    objs.emplace_back("FOX",i,j); break;
			default: break;
		}
	}
	N = objs.size();
	cout << GEN_PROC_RABBITS<<" "<<GEN_PROC_FOXES<<" "
		 <<GEN_FOOD_FOXES<<" "<<0<<" "<<R<<" "<<C<<" "<<N<<"\n";
	for(auto& [s,i,j] : objs){
		cout<<s<<" "<<i<<" "<<j<<"\n";
	}
}

// -------------------------------------------------------------------

// Rabbit constructor
Rabbit::Rabbit(int x,int y) {
    this->x = x; this->y = y;
    sinceBorn = 0;
    sinceReproduce = 0;
}

// Rabbit movement logic
pair<int,int> Rabbit::decideMove(World& w, int gen) {
    vector<pair<int,int>> nbrs;
    // clockwise: N, E, S, W
    const int dx[4]{-1,0,1,0}, dy[4]{0,1,0,-1};
    for(int d=0; d<4; ++d) {
        int nx = x + dx[d], ny = y + dy[d];
        if(nx>=0 && nx<w.R && ny>=0 && ny<w.C
           && w.grid_cur[nx][ny].type == Cell::EMPTY) {
            nbrs.emplace_back(nx,ny);
        }
    }
    if(nbrs.empty()) return {x,y};
    int idx = (gen + x + y) % nbrs.size();
    return nbrs[idx];
}

// Rabbit reproduction logic
unique_ptr<Entity> Rabbit::maybeReproduce(int GEN_PROC) {
    if (sinceReproduce < GEN_PROC) 
        return nullptr;

    // spawn baby at *old* location; parent resets counter
    auto baby = make_unique<Rabbit>(this->x, this->y);
    this->sinceReproduce = 0;
    baby->sinceReproduce = 0;
    return baby;
}

// -------------------------------------------------------------------

// Fox constructor
Fox::Fox(int x,int y) {
    this->x = x; this->y = y;
    sinceBorn = 0;
    sinceReproduce = 0;
    sinceEat = 0;
}

// Fox movement logic
pair<int,int> Fox::decideMove(World& w, int gen) {
    vector<pair<int,int>> rabbits, empties;
    const int dx[4]{-1,0,1,0}, dy[4]{0,1,0,-1};

    // collect adjacent rabbits first
    for(int d=0; d<4; ++d) {
        int nx = x + dx[d], ny = y + dy[d];
        if(nx>=0 && nx<w.R && ny>=0 && ny<w.C) {
            if(w.grid_cur[nx][ny].type == Cell::RABBIT)
                rabbits.emplace_back(nx,ny);
            else if(w.grid_cur[nx][ny].type == Cell::EMPTY)
                empties.emplace_back(nx,ny);
        }
    }

    // eat if possible
    if(!rabbits.empty()) {
        int idx = (gen + x + y) % rabbits.size();
        return rabbits[idx];
    }
    // otherwise move to empty
    if(!empties.empty()) {
        int idx = (gen + x + y) % empties.size();
        return empties[idx];
    }
    // else stay
    return {x,y};
}

// Fox reproduction logic
unique_ptr<Entity> Fox::maybeReproduce(int GEN_PROC) {
    if (sinceReproduce < GEN_PROC) 
        return nullptr;

    auto baby = make_unique<Fox>(this->x, this->y);
    this->sinceReproduce = 0;
    baby->sinceReproduce = 0;
    baby->sinceEat = 0;
    return baby;
}
