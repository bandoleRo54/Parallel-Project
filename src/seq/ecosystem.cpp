#include "ecosystem.h"

World::World(int gr, int gf, int ff, int ngen, int rows, int cols):
	GEN_PROC_RABBITS(gr), GEN_PROC_FOXES(gf),
	GEN_FOOD_FOXES(ff), N_GEN(ngen),
	R(rows), C(cols),
	grid(rows, vector<Cell>(cols)) {}

void World::placeRock(int x,int y) {
	grid[x][y].type = Cell::ROCK;
}

void World::placeRabbit(int x,int y) {
	auto r = make_unique<Rabbit>(x,y);
	grid[x][y].type = Cell::RABBIT;
	grid[x][y].occupant = r.get();
	allAnimals.push_back(move(r));
}

void World::placeFox(int x,int y) {
	auto f = make_unique<Fox>(x,y);
	grid[x][y].type = Cell::FOX;
	grid[x][y].occupant = f.get();
	allAnimals.push_back(move(f));
}

// Loads initial state from stdin
void World::loadFromStdin() {
	int N;
	cin >> GEN_PROC_RABBITS >> GEN_PROC_FOXES
		>> GEN_FOOD_FOXES >> N_GEN
		>> R >> C >> N;
	grid.assign(R, vector<Cell>(C));
	allAnimals.clear();
	string obj;
	int x,y;
	for(int i=0;i<N;i++){
		cin >> obj >> x >> y;
		if(obj=="ROCK")    placeRock(x,y);
		else if(obj=="RABBIT") placeRabbit(x,y);
		else if(obj=="FOX")    placeFox(x,y);
	}
}

// The main engine
void World::runSimulation() {
	for(int g=1; g<=N_GEN; ++g){
		step<Rabbit>(Cell::RABBIT, g, GEN_PROC_RABBITS);
		step<Fox>(Cell::FOX, g, GEN_PROC_FOXES);
		// after each step, handle starvation & cleanup
		cleanupStarved();
	}
}

// Template for moving rabbits or foxes
template<typename T> void World::step(Cell::Type type, int gen, int GEN_PROC) {
	struct Intent { Entity* who; int tx,ty; };
	vector<Intent> intents;

	// 1) Each T decides a target
	for(auto& up : allAnimals){
		if( (type==Cell::RABBIT && dynamic_cast<Rabbit*>(up.get()))
		 || (type==Cell::FOX    && dynamic_cast<Fox*>(up.get())) ) {
			auto p = up->decideMove(*this, gen);
			intents.push_back({ up.get(), p.first, p.second });
		}
	}
	// 2) Group intents by (tx,ty), resolve conflicts
	map<pair<int,int>, vector<Intent>> byCell;
	for(auto& it : intents)
		byCell[{it.tx,it.ty}].push_back(it);

	for(auto& [coord, vec] : byCell) {
		int tx=coord.first, ty=coord.second;
		// pick survivor
		Intent* winner = &vec[0];
		for(auto& intent : vec){
			// compare sinceReproduce, additional fox hunger if ties
			if(intent.who->sinceReproduce > winner->who->sinceReproduce)
				winner = &const_cast<Intent&>(intent);
			else if(type==Cell::FOX 
				 && intent.who->sinceReproduce == winner->who->sinceReproduce){
				auto f1 = dynamic_cast<Fox*>(intent.who);
				auto f2 = dynamic_cast<Fox*>(winner->who);
				if(f1->sinceEat < f2->sinceEat)
					winner = &const_cast<Intent&>(intent);
			}
		}
		// apply winner move
		Entity* mover = winner->who;
		int ox=mover->x, oy=mover->y;
		// clear old cell
		grid[ox][oy].type = Cell::EMPTY;
		grid[ox][oy].occupant = nullptr;
		// eat if fox
		if(type==Cell::FOX && grid[tx][ty].type==Cell::RABBIT) {
			// find and mark eaten rabbit to be removed
			grid[tx][ty].occupant->sinceBorn = -1;
			dynamic_cast<Fox*>(mover)->sinceEat = 0;
		}
		// move into new cell
		grid[tx][ty].type = type;
		grid[tx][ty].occupant = mover;
		mover->x = tx; mover->y = ty;
		// bump ages
		mover->sinceBorn++;
		mover->sinceReproduce++;
		if(type==Cell::FOX)
			dynamic_cast<Fox*>(mover)->sinceEat++;
		// handle reproduction
		if(mover->sinceReproduce >= GEN_PROC){
			auto baby = mover->maybeReproduce(GEN_PROC);
			if(baby){
				auto [bx,by] = baby->x==ox && baby->y==oy
					? pair(ox,oy) : pair(ox,oy);
				// place newborn in old spot
				grid[bx][by].type = type;
				grid[bx][by].occupant = baby.get();
				allAnimals.push_back(move(baby));
			}
		}
	}
}

void World::cleanupStarved(){
	// remove rabbits flagged sinceBorn<0, and foxes that reached GEN_FOOD_FOXES
	vector<unique_ptr<Entity>> survivors;
	for(auto& up : allAnimals){
		if(auto f = dynamic_cast<Fox*>(up.get())){
			if(f->sinceEat >= GEN_FOOD_FOXES){
				grid[f->x][f->y].type = Cell::EMPTY;
				grid[f->x][f->y].occupant = nullptr;
				continue;
			}
		}
		if(up->sinceBorn < 0){
			// eaten rabbit
			continue;
		}
		survivors.push_back(move(up));
	}
	allAnimals.swap(survivors);
}

// Print final state
void World::outputFinalState(){
	cout << endl;
	// count objects
	int N=0;
	vector<tuple<string,int,int>> objs;
	for(int i=0;i<R;i++)for(int j=0;j<C;j++){
		switch(grid[i][j].type){
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
           && w.grid[nx][ny].type == Cell::EMPTY) {
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
            if(w.grid[nx][ny].type == Cell::RABBIT)
                rabbits.emplace_back(nx,ny);
            else if(w.grid[nx][ny].type == Cell::EMPTY)
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
