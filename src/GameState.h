#pragma once

#include "NetworkManager.h"
#include "raylib.h"
#include <vector>
#include <map>

enum class CellType {
    EMPTY,
    SHRUBBERY,
    TREE_SEEDLING,
    TREE_YOUNG,
    TREE_MATURE,
    GRAVE,
    PLAYER,
    ANIMAL
};

enum class PlayerMode {
    PLANT,
    SHOOT,
    CHOP
};

enum class AnimalType {
    RABBIT,
    DEER
};

struct Cell {
    CellType type = CellType::EMPTY;
    int playerId = -1;
    float growth = 0.0f;
    float lastUpdate = 0.0f;
};

struct Player {
    int id;
    int x, y;
    PlayerMode mode = PlayerMode::PLANT;
    Color color;
    int score = 0;
    bool alive = true;
    float lastAction = 0.0f;
    int lastDirectionX = 0; // For shooting direction
    int lastDirectionY = 0;
    float lastMove = 0.0f;  // For movement throttling
};

struct Animal {
    AnimalType type;
    int x, y;
    float lastMove = 0.0f;
    float moveDelay = 1.0f;
    int id;
};

struct Bullet {
    int x, y;
    int dirX, dirY;
    int playerId;
    float startTime;
    bool active = true;
};

struct GameState {
    std::vector<std::vector<Cell>> grid;
    std::map<int, Player> players;
    std::vector<Animal> animals;
    std::vector<Bullet> bullets;
};