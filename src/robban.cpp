#include "raylib.h"
#include <vector>
#include <map>
#include <random>
#include <chrono>
#include <memory>
#include <string>
#include <algorithm>

// Game constants
const int GRID_WIDTH = 40;
const int GRID_HEIGHT = 30;
const int CELL_SIZE = 20;
const int WINDOW_WIDTH = GRID_WIDTH * CELL_SIZE;
const int WINDOW_HEIGHT = GRID_HEIGHT * CELL_SIZE;
const float TREE_GROWTH_TIME = 10.0f; // seconds
const float ANIMAL_SPAWN_RATE = 0.02f; // probability per frame
const int MAX_ANIMALS = 20;

// Player colors
const Color PLAYER_COLORS[] = {
    BLUE, RED, GREEN, YELLOW, PURPLE, ORANGE, PINK, BROWN
};

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
};

struct Animal {
    AnimalType type;
    int x, y;
    float lastMove = 0.0f;
    float moveDelay = 1.0f;
    int id;
};

class RobbanPlanterar {
private:
    std::vector<std::vector<Cell>> grid;
    std::map<int, Player> players;
    std::vector<Animal> animals;
    int localPlayerId = 0;
    std::mt19937 rng;
    float gameTime = 0.0f;
    int nextAnimalId = 0;

    void InitializeGrid() {
        grid.resize(GRID_HEIGHT, std::vector<Cell>(GRID_WIDTH));
        
        // Add some initial shrubbery
        for (int i = 0; i < 100; i++) {
            int x = rng() % GRID_WIDTH;
            int y = rng() % GRID_HEIGHT;
            if (grid[y][x].type == CellType::EMPTY) {
                grid[y][x].type = CellType::SHRUBBERY;
            }
        }
    }

    void SpawnPlayer(int playerId) {
        Player& player = players[playerId];
        
        // Spawn in random corner
        std::vector<std::pair<int, int>> corners = {
            {0, 0}, {GRID_WIDTH-1, 0}, {0, GRID_HEIGHT-1}, {GRID_WIDTH-1, GRID_HEIGHT-1}
        };
        
        auto corner = corners[rng() % corners.size()];
        player.x = corner.first;
        player.y = corner.second;
        player.alive = true;
        
        // Clear the spawn location
        grid[player.y][player.x].type = CellType::EMPTY;
    }

    void UpdateAnimals() {
        // Spawn new animals
        if (animals.size() < MAX_ANIMALS && (rng() % 1000) < (ANIMAL_SPAWN_RATE * 1000)) {
            Animal animal;
            animal.type = (rng() % 2 == 0) ? AnimalType::RABBIT : AnimalType::DEER;
            animal.x = rng() % GRID_WIDTH;
            animal.y = rng() % GRID_HEIGHT;
            animal.id = nextAnimalId++;
            animal.moveDelay = 0.5f + (rng() % 100) / 100.0f;
            
            if (grid[animal.y][animal.x].type == CellType::EMPTY) {
                animals.push_back(animal);
            }
        }

        // Move and update animals
        for (auto& animal : animals) {
            if (gameTime - animal.lastMove > animal.moveDelay) {
                // Try to move towards food
                int newX = animal.x;
                int newY = animal.y;
                
                // Simple AI: move randomly but prefer cells with food
                std::vector<std::pair<int, int>> moves = {
                    {0, 1}, {0, -1}, {1, 0}, {-1, 0}
                };
                
                std::random_shuffle(moves.begin(), moves.end());
                
                for (auto move : moves) {
                    int testX = animal.x + move.first;
                    int testY = animal.y + move.second;
                    
                    if (testX >= 0 && testX < GRID_WIDTH && testY >= 0 && testY < GRID_HEIGHT) {
                        Cell& cell = grid[testY][testX];
                        
                        // Can eat shrubbery or young trees
                        if (cell.type == CellType::SHRUBBERY || 
                            (cell.type == CellType::TREE_SEEDLING) ||
                            (cell.type == CellType::TREE_YOUNG && cell.growth < 0.5f)) {
                            newX = testX;
                            newY = testY;
                            
                            // Eat the vegetation
                            cell.type = CellType::EMPTY;
                            cell.playerId = -1;
                            cell.growth = 0.0f;
                            break;
                        } else if (cell.type == CellType::EMPTY) {
                            newX = testX;
                            newY = testY;
                        }
                    }
                }
                
                animal.x = newX;
                animal.y = newY;
                animal.lastMove = gameTime;
            }
        }
    }

    void UpdateTrees() {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            for (int x = 0; x < GRID_WIDTH; x++) {
                Cell& cell = grid[y][x];
                
                if (cell.type == CellType::TREE_SEEDLING || cell.type == CellType::TREE_YOUNG) {
                    if (gameTime - cell.lastUpdate > 1.0f) {
                        cell.growth += 1.0f / TREE_GROWTH_TIME;
                        cell.lastUpdate = gameTime;
                        
                        if (cell.growth >= 0.5f && cell.type == CellType::TREE_SEEDLING) {
                            cell.type = CellType::TREE_YOUNG;
                        } else if (cell.growth >= 1.0f && cell.type == CellType::TREE_YOUNG) {
                            cell.type = CellType::TREE_MATURE;
                        }
                    }
                }
            }
        }
    }

    void HandlePlayerAction(int playerId, int targetX, int targetY) {
        if (players.find(playerId) == players.end() || !players[playerId].alive) return;
        
        Player& player = players[playerId];
        
        // Check if target is adjacent or same cell
        int dx = abs(targetX - player.x);
        int dy = abs(targetY - player.y);
        if (dx > 1 || dy > 1) return;
        
        if (targetX < 0 || targetX >= GRID_WIDTH || targetY < 0 || targetY >= GRID_HEIGHT) return;
        
        Cell& cell = grid[targetY][targetX];
        
        switch (player.mode) {
            case PlayerMode::PLANT:
                if (cell.type == CellType::EMPTY || cell.type == CellType::SHRUBBERY) {
                    cell.type = CellType::TREE_SEEDLING;
                    cell.playerId = playerId;
                    cell.growth = 0.0f;
                    cell.lastUpdate = gameTime;
                }
                break;
                
            case PlayerMode::CHOP:
                if (cell.type == CellType::TREE_MATURE) {
                    cell.type = CellType::EMPTY;
                    cell.playerId = -1;
                    cell.growth = 0.0f;
                    player.score += 10;
                }
                break;
                
            case PlayerMode::SHOOT:
                // Check for animals at target location
                for (auto it = animals.begin(); it != animals.end(); ++it) {
                    if (it->x == targetX && it->y == targetY) {
                        player.score += 5;
                        animals.erase(it);
                        break;
                    }
                }
                
                // Check for other players at target location
                for (auto& [id, otherPlayer] : players) {
                    if (id != playerId && otherPlayer.x == targetX && otherPlayer.y == targetY && otherPlayer.alive) {
                        otherPlayer.alive = false;
                        player.score -= 5;
                        
                        // Create grave
                        grid[targetY][targetX].type = CellType::GRAVE;
                        grid[targetY][targetX].playerId = id;
                        
                        // Respawn the killed player
                        SpawnPlayer(id);
                        break;
                    }
                }
                break;
        }
    }

    void DrawCell(int x, int y, const Cell& cell) {
        Rectangle rect = {x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE};
        
        switch (cell.type) {
            case CellType::EMPTY:
                DrawRectangleRec(rect, DARKGREEN);
                break;
                
            case CellType::SHRUBBERY:
                DrawRectangleRec(rect, DARKGREEN);
                DrawRectangle(x * CELL_SIZE + 2, y * CELL_SIZE + 2, CELL_SIZE - 4, CELL_SIZE - 4, GREEN);
                break;
                
            case CellType::TREE_SEEDLING:
            case CellType::TREE_YOUNG:
            case CellType::TREE_MATURE: {
                DrawRectangleRec(rect, DARKGREEN);
                Color treeColor = (cell.playerId >= 0) ? PLAYER_COLORS[cell.playerId % 8] : GREEN;
                
                if (cell.type == CellType::TREE_SEEDLING) {
                    DrawRectangle(x * CELL_SIZE + 8, y * CELL_SIZE + 8, 4, 4, treeColor);
                } else if (cell.type == CellType::TREE_YOUNG) {
                    DrawRectangle(x * CELL_SIZE + 6, y * CELL_SIZE + 6, 8, 8, treeColor);
                } else {
                    DrawRectangle(x * CELL_SIZE + 2, y * CELL_SIZE + 2, CELL_SIZE - 4, CELL_SIZE - 4, treeColor);
                }
                break;
            }
            
            case CellType::GRAVE: {
                DrawRectangleRec(rect, DARKGREEN);
                Color graveColor = (cell.playerId >= 0) ? PLAYER_COLORS[cell.playerId % 8] : GRAY;
                DrawRectangle(x * CELL_SIZE + 4, y * CELL_SIZE + 4, CELL_SIZE - 8, CELL_SIZE - 8, graveColor);
                break;
            }
        }
    }

    void DrawPlayer(const Player& player) {
        if (!player.alive) return;
        
        Rectangle rect = {player.x * CELL_SIZE, player.y * CELL_SIZE, CELL_SIZE, CELL_SIZE};
        DrawRectangleRec(rect, player.color);
        
        // Draw mode indicator
        const char* modeChar = "P";
        if (player.mode == PlayerMode::SHOOT) modeChar = "S";
        else if (player.mode == PlayerMode::CHOP) modeChar = "C";
        
        DrawText(modeChar, player.x * CELL_SIZE + 2, player.y * CELL_SIZE + 2, 16, BLACK);
    }

    void DrawAnimal(const Animal& animal) {
        Rectangle rect = {animal.x * CELL_SIZE, animal.y * CELL_SIZE, CELL_SIZE, CELL_SIZE};
        Color animalColor = (animal.type == AnimalType::RABBIT) ? WHITE : BROWN;
        DrawRectangle(animal.x * CELL_SIZE + 4, animal.y * CELL_SIZE + 4, CELL_SIZE - 8, CELL_SIZE - 8, animalColor);
    }

public:
    RobbanPlanterar() : rng(std::chrono::steady_clock::now().time_since_epoch().count()) {
        InitializeGrid();
        
        // Add local player
        Player localPlayer;
        localPlayer.id = localPlayerId;
        localPlayer.color = PLAYER_COLORS[localPlayerId % 8];
        players[localPlayerId] = localPlayer;
        SpawnPlayer(localPlayerId);
    }

    void Update() {
        gameTime = GetTime();
        
        Player& localPlayer = players[localPlayerId];
        
        // Handle input
        if (IsKeyPressed(KEY_P)) {
            switch (localPlayer.mode) {
                case PlayerMode::PLANT: localPlayer.mode = PlayerMode::SHOOT; break;
                case PlayerMode::SHOOT: localPlayer.mode = PlayerMode::CHOP; break;
                case PlayerMode::CHOP: localPlayer.mode = PlayerMode::PLANT; break;
            }
        }
        
        // Movement
        int newX = localPlayer.x;
        int newY = localPlayer.y;
        
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) newY--;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) newY++;
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) newX--;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) newX++;
        
        if (newX >= 0 && newX < GRID_WIDTH && newY >= 0 && newY < GRID_HEIGHT) {
            localPlayer.x = newX;
            localPlayer.y = newY;
        }
        
        // Action
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mousePos = GetMousePosition();
            int targetX = (int)(mousePos.x / CELL_SIZE);
            int targetY = (int)(mousePos.y / CELL_SIZE);
            HandlePlayerAction(localPlayerId, targetX, targetY);
        }
        
        UpdateAnimals();
        UpdateTrees();
    }

    void Draw() {
        BeginDrawing();
        ClearBackground(DARKGREEN);
        
        // Draw grid
        for (int y = 0; y < GRID_HEIGHT; y++) {
            for (int x = 0; x < GRID_WIDTH; x++) {
                DrawCell(x, y, grid[y][x]);
            }
        }
        
        // Draw animals
        for (const auto& animal : animals) {
            DrawAnimal(animal);
        }
        
        // Draw players
        for (const auto& [id, player] : players) {
            DrawPlayer(player);
        }
        
        // Draw UI
        DrawText(TextFormat("Score: %d", players[localPlayerId].score), 10, 10, 20, WHITE);
        
        const char* modeText = "Plant";
        if (players[localPlayerId].mode == PlayerMode::SHOOT) modeText = "Shoot";
        else if (players[localPlayerId].mode == PlayerMode::CHOP) modeText = "Chop";
        
        DrawText(TextFormat("Mode: %s (P to switch)", modeText), 10, 35, 20, WHITE);
        DrawText("WASD/Arrows: Move, Mouse: Action", 10, 60, 16, WHITE);
        
        EndDrawing();
    }

    void AddPlayer(int playerId) {
        if (players.find(playerId) == players.end()) {
            Player newPlayer;
            newPlayer.id = playerId;
            newPlayer.color = PLAYER_COLORS[playerId % 8];
            players[playerId] = newPlayer;
            SpawnPlayer(playerId);
        }
    }

    void RemovePlayer(int playerId) {
        players.erase(playerId);
    }
};

int main() {
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Robban Planterar");
    SetTargetFPS(60);
    
    RobbanPlanterar game;
    
    while (!WindowShouldClose()) {
        game.Update();
        game.Draw();
    }
    
    CloseWindow();
    return 0;
}