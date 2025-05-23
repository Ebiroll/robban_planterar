#include "raylib.h"
#include "NetworkManager.h"
#include <vector>
#include <map>
#include <random>
#include <chrono>
#include <memory>
#include <string>
#include <algorithm>
#include <iostream>

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

class RobbanPlanterar {
private:
    std::vector<std::vector<Cell>> grid;
    std::map<int, Player> players;
    std::vector<Animal> animals;
    int localPlayerId = 0;
    std::mt19937 rng;
    float gameTime = 0.0f;
    int nextAnimalId = 0;
    int nextPlayerId = 1;
    
    // Networking
    std::unique_ptr<NetworkManager> networkManager;
    bool isMultiplayer = false;
    bool isHost = false;
    std::string currentRoom;
    
    // Game state synchronization
    float lastSyncTime = 0.0f;
    const float SYNC_INTERVAL = 0.1f; // Sync every 100ms

    void SetupNetworking() {
        networkManager = std::make_unique<NetworkManager>();
        
        // Set up network callbacks
        networkManager->SetPlayerJoinCallback([this](int playerId) {
            this->OnPlayerJoin(playerId);
        });
        
        networkManager->SetPlayerLeaveCallback([this](int playerId) {
            this->OnPlayerLeave(playerId);
        });
        
        networkManager->SetPlayerUpdateCallback([this](const PlayerUpdate& update) {
            this->OnPlayerUpdate(update);
        });
        
        networkManager->SetPlayerActionCallback([this](const ActionMessage& action) {
            this->OnPlayerAction(action);
        });
    }
    
    void OnPlayerJoin(int playerId) {
        std::cout << "Player " << playerId << " joined the game" << std::endl;
        AddPlayer(playerId);
    }
    
    void OnPlayerLeave(int playerId) {
        std::cout << "Player " << playerId << " left the game" << std::endl;
        RemovePlayer(playerId);
    }
    
    void OnPlayerUpdate(const PlayerUpdate& update) {
        if (players.find(update.id) != players.end()) {
            Player& player = players[update.id];
            player.x = update.x;
            player.y = update.y;
            player.mode = static_cast<PlayerMode>(update.mode);
            player.score = update.score;
            player.alive = update.alive;
        }
    }
    
    void OnPlayerAction(const ActionMessage& action) {
        HandlePlayerAction(action.playerId, action.targetX, action.targetY);
    }
    
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
                
                std::shuffle(moves.begin(), moves.end(), rng);
                
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
        
        // Prevent spam actions
        if (gameTime - player.lastAction < 0.2f) return;
        player.lastAction = gameTime;
        
        switch (player.mode) {
            case PlayerMode::PLANT: {
                // Plant at player's current location or adjacent cell
                int plantX = targetX != -1 ? targetX : player.x;
                int plantY = targetY != -1 ? targetY : player.y;
                
                // Check if target is adjacent or same cell
                int dx = abs(plantX - player.x);
                int dy = abs(plantY - player.y);
                if (dx > 1 || dy > 1) return;
                
                if (plantX < 0 || plantX >= GRID_WIDTH || plantY < 0 || plantY >= GRID_HEIGHT) return;
                
                Cell& cell = grid[plantY][plantX];
                if (cell.type == CellType::EMPTY || cell.type == CellType::SHRUBBERY) {
                    cell.type = CellType::TREE_SEEDLING;
                    cell.playerId = playerId;
                    cell.growth = 0.0f;
                    cell.lastUpdate = gameTime;
                }
                break;
            }
                
            case PlayerMode::CHOP: {
                // Chop at player's current location or adjacent cell
                int chopX = targetX != -1 ? targetX : player.x;
                int chopY = targetY != -1 ? targetY : player.y;
                
                // Check if target is adjacent or same cell
                int dx = abs(chopX - player.x);
                int dy = abs(chopY - player.y);
                if (dx > 1 || dy > 1) return;
                
                if (chopX < 0 || chopX >= GRID_WIDTH || chopY < 0 || chopY >= GRID_HEIGHT) return;
                
                Cell& cell = grid[chopY][chopX];
                if (cell.type == CellType::TREE_MATURE) {
                    cell.type = CellType::EMPTY;
                    cell.playerId = -1;
                    cell.growth = 0.0f;
                    player.score += 10;
                }
                break;
            }
                
            case PlayerMode::SHOOT: {
                // Shoot in the direction the player last moved
                int bulletX = player.x;
                int bulletY = player.y;
                int dirX = player.lastDirectionX;
                int dirY = player.lastDirectionY;
                
                // If no direction set, default to right
                if (dirX == 0 && dirY == 0) {
                    dirX = 1;
                }
                
                // Trace bullet path until it hits something or goes out of bounds
                for (int range = 1; range <= 10; range++) {
                    int checkX = bulletX + (dirX * range);
                    int checkY = bulletY + (dirY * range);
                    
                    if (checkX < 0 || checkX >= GRID_WIDTH || checkY < 0 || checkY >= GRID_HEIGHT) {
                        break; // Out of bounds
                    }
                    
                    // Check for animals at this location
                    for (auto it = animals.begin(); it != animals.end(); ++it) {
                        if (it->x == checkX && it->y == checkY) {
                            player.score += 5;
                            animals.erase(it);
                            return; // Bullet stops after hitting animal
                        }
                    }
                    
                    // Check for other players at this location
                    for (auto& [id, otherPlayer] : players) {
                        if (id != playerId && otherPlayer.x == checkX && otherPlayer.y == checkY && otherPlayer.alive) {
                            otherPlayer.alive = false;
                            player.score -= 5;
                            
                            // Create grave
                            grid[checkY][checkX].type = CellType::GRAVE;
                            grid[checkY][checkX].playerId = id;
                            
                            // Respawn the killed player
                            SpawnPlayer(id);
                            return; // Bullet stops after hitting player
                        }
                    }
                    
                    // Check for obstacles (trees)
                    Cell& cell = grid[checkY][checkX];
                    if (cell.type == CellType::TREE_MATURE || cell.type == CellType::TREE_YOUNG) {
                        break; // Bullet stops at tree
                    }
                }
                break;
            }
        }
    }

    void DrawCell(int x, int y, const Cell& cell) {
        Rectangle rect = {
            static_cast<float>(x * CELL_SIZE), 
            static_cast<float>(y * CELL_SIZE), 
            static_cast<float>(CELL_SIZE), 
            static_cast<float>(CELL_SIZE)
        };
        
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
            
            case CellType::PLAYER:
            case CellType::ANIMAL:
                // These are handled separately in DrawPlayer and DrawAnimal
                DrawRectangleRec(rect, DARKGREEN);
                break;
        }
    }

    void DrawPlayer(const Player& player) {
        if (!player.alive) return;
        
        Rectangle rect = {
            static_cast<float>(player.x * CELL_SIZE), 
            static_cast<float>(player.y * CELL_SIZE), 
            static_cast<float>(CELL_SIZE), 
            static_cast<float>(CELL_SIZE)
        };
        DrawRectangleRec(rect, player.color);
        
        // Draw mode indicator
        const char* modeChar = "P";
        if (player.mode == PlayerMode::SHOOT) modeChar = "S";
        else if (player.mode == PlayerMode::CHOP) modeChar = "C";
        
        DrawText(modeChar, player.x * CELL_SIZE + 2, player.y * CELL_SIZE + 2, 16, BLACK);
        
        // Draw direction indicator for shooting mode
        if (player.mode == PlayerMode::SHOOT && (player.lastDirectionX != 0 || player.lastDirectionY != 0)) {
            int centerX = player.x * CELL_SIZE + CELL_SIZE / 2;
            int centerY = player.y * CELL_SIZE + CELL_SIZE / 2;
            int endX = centerX + player.lastDirectionX * 8;
            int endY = centerY + player.lastDirectionY * 8;
            
            DrawLine(centerX, centerY, endX, endY, BLACK);
            
            // Draw arrow head
            Vector2 v1, v2, v3;
            if (player.lastDirectionX > 0) { // Right
                v1 = {static_cast<float>(endX), static_cast<float>(endY)};
                v2 = {static_cast<float>(endX-4), static_cast<float>(endY-2)};
                v3 = {static_cast<float>(endX-4), static_cast<float>(endY+2)};
            } else if (player.lastDirectionX < 0) { // Left
                v1 = {static_cast<float>(endX), static_cast<float>(endY)};
                v2 = {static_cast<float>(endX+4), static_cast<float>(endY-2)};
                v3 = {static_cast<float>(endX+4), static_cast<float>(endY+2)};
            } else if (player.lastDirectionY > 0) { // Down
                v1 = {static_cast<float>(endX), static_cast<float>(endY)};
                v2 = {static_cast<float>(endX-2), static_cast<float>(endY-4)};
                v3 = {static_cast<float>(endX+2), static_cast<float>(endY-4)};
            } else if (player.lastDirectionY < 0) { // Up
                v1 = {static_cast<float>(endX), static_cast<float>(endY)};
                v2 = {static_cast<float>(endX-2), static_cast<float>(endY+4)};
                v3 = {static_cast<float>(endX+2), static_cast<float>(endY+4)};
            }
            DrawTriangle(v1, v2, v3, BLACK);
        }
    }

    void DrawAnimal(const Animal& animal) {
        Rectangle rect = {
            static_cast<float>(animal.x * CELL_SIZE), 
            static_cast<float>(animal.y * CELL_SIZE), 
            static_cast<float>(CELL_SIZE), 
            static_cast<float>(CELL_SIZE)
        };
        Color animalColor = (animal.type == AnimalType::RABBIT) ? WHITE : BROWN;
        DrawRectangle(animal.x * CELL_SIZE + 4, animal.y * CELL_SIZE + 4, CELL_SIZE - 8, CELL_SIZE - 8, animalColor);
    }

public:
    RobbanPlanterar() : rng(std::chrono::steady_clock::now().time_since_epoch().count()) {
        InitializeGrid();
        SetupNetworking();
        
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
        
        // Network controls
        if (!isMultiplayer) {
            if (IsKeyPressed(KEY_H)) {
                // Host a game
                currentRoom = "RobbanRoom";
                if (networkManager->CreateRoom(currentRoom)) {
                    isMultiplayer = true;
                    isHost = true;
                    std::cout << "Hosting room: " << networkManager->GetRoomId() << std::endl;
                }
            } else if (IsKeyPressed(KEY_J)) {
                // Join a game (simplified - in real version would show input dialog)
                currentRoom = "RobbanRoom_1234"; // Example room ID
                if (networkManager->JoinRoom(currentRoom)) {
                    isMultiplayer = true;
                    isHost = false;
                    std::cout << "Joining room: " << currentRoom << std::endl;
                }
            }
        }
        
        // Handle input
        if (IsKeyPressed(KEY_P)) {
            switch (localPlayer.mode) {
                case PlayerMode::PLANT: localPlayer.mode = PlayerMode::SHOOT; break;
                case PlayerMode::SHOOT: localPlayer.mode = PlayerMode::CHOP; break;
                case PlayerMode::CHOP: localPlayer.mode = PlayerMode::PLANT; break;
            }
            
            // Send mode change to network
            if (isMultiplayer && networkManager && networkManager->IsConnected()) {
                networkManager->SendPlayerModeChange(localPlayerId, static_cast<int>(localPlayer.mode));
            }
        }
        
        // Movement with throttling for better control
        int newX = localPlayer.x;
        int newY = localPlayer.y;
        int moveX = 0, moveY = 0;
        
        // Check for movement input
        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) { moveY = -1; }
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) { moveY = 1; }
        if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) { moveX = -1; }
        if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) { moveX = 1; }
        
        // Apply movement if within bounds
        if (moveX != 0 || moveY != 0) {
            newX += moveX;
            newY += moveY;
            
            if (newX >= 0 && newX < GRID_WIDTH && newY >= 0 && newY < GRID_HEIGHT) {
                localPlayer.x = newX;
                localPlayer.y = newY;
                
                // Update direction for shooting
                localPlayer.lastDirectionX = moveX;
                localPlayer.lastDirectionY = moveY;
                localPlayer.lastMove = gameTime;
            }
        }
        
        // Action with spacebar
        if (IsKeyPressed(KEY_SPACE)) {
            HandlePlayerAction(localPlayerId, -1, -1); // Use -1 to indicate current position
            
            // Send action to network if multiplayer
            if (isMultiplayer && networkManager && networkManager->IsConnected()) {
                ActionMessage action;
                action.playerId = localPlayerId;
                action.targetX = localPlayer.x;
                action.targetY = localPlayer.y;
                action.actionType = static_cast<int>(localPlayer.mode);
                networkManager->SendPlayerAction(action);
            }
        }
        
        UpdateAnimals();
        UpdateTrees();
        //SyncGameState();
        
        // Process network messages
        if (isMultiplayer && networkManager) {
            networkManager->ProcessMessages();
        }
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
        DrawText("WASD/Arrows: Move, SPACE: Action", 10, 60, 16, WHITE);
        
        // Show shooting direction
        if (players[localPlayerId].mode == PlayerMode::SHOOT) {
            const char* dirText = "No direction";
            if (players[localPlayerId].lastDirectionX > 0) dirText = "Shooting →";
            else if (players[localPlayerId].lastDirectionX < 0) dirText = "Shooting ←";
            else if (players[localPlayerId].lastDirectionY > 0) dirText = "Shooting ↓";
            else if (players[localPlayerId].lastDirectionY < 0) dirText = "Shooting ↑";
            
            DrawText(dirText, 10, 80, 16, YELLOW);
        }
        
        // Multiplayer UI
        if (isMultiplayer && networkManager) {
            int yOffset = players[localPlayerId].mode == PlayerMode::SHOOT ? 105 : 85;
            DrawText(TextFormat("Room: %s", currentRoom.c_str()), 10, yOffset, 16, WHITE);
            DrawText(TextFormat("Players: %d", networkManager->GetPlayerCount()), 10, yOffset + 20, 16, WHITE);
            
            if (networkManager->IsHost()) {
                DrawText("HOST", 10, yOffset + 40, 16, YELLOW);
            }
        } else {
            int yOffset = players[localPlayerId].mode == PlayerMode::SHOOT ? 105 : 85;
            DrawText("Press H to host, J to join", 10, yOffset, 16, WHITE);
        }
        
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
    
    #if 0
    // Networking helper functions
    void SetupNetworking() {
        networkManager = std::make_unique<NetworkManager>();
        
        // Set up network callbacks
        networkManager->SetPlayerJoinCallback([this](int playerId) {
            this->OnPlayerJoin(playerId);
        });
        
        networkManager->SetPlayerLeaveCallback([this](int playerId) {
            this->OnPlayerLeave(playerId);
        });
        
        networkManager->SetPlayerUpdateCallback([this](const PlayerUpdate& update) {
            this->OnPlayerUpdate(update);
        });
        
        networkManager->SetPlayerActionCallback([this](const ActionMessage& action) {
            this->OnPlayerAction(action);
        });
    }
    
    void OnPlayerJoin(int playerId) {
        std::cout << "Player " << playerId << " joined the game" << std::endl;
        AddPlayer(playerId);
    }
    
    void OnPlayerLeave(int playerId) {
        std::cout << "Player " << playerId << " left the game" << std::endl;
        RemovePlayer(playerId);
    }
    
    void OnPlayerUpdate(const PlayerUpdate& update) {
        if (players.find(update.id) != players.end()) {
            Player& player = players[update.id];
            player.x = update.x;
            player.y = update.y;
            player.mode = static_cast<PlayerMode>(update.mode);
            player.score = update.score;
            player.alive = update.alive;
        }
    }
    
    void OnPlayerAction(const ActionMessage& action) {
        HandlePlayerAction(action.playerId, action.targetX, action.targetY);
    }
    
    void SyncGameState() {
        if (!isMultiplayer || !networkManager || !networkManager->IsConnected()) return;
        
        if (gameTime - lastSyncTime > SYNC_INTERVAL) {
            // Send local player update
            Player& localPlayer = players[localPlayerId];
            PlayerUpdate update;
            update.id = localPlayerId;
            update.x = localPlayer.x;
            update.y = localPlayer.y;
            update.mode = static_cast<int>(localPlayer.mode);
            update.score = localPlayer.score;
            update.alive = localPlayer.alive;
            
            networkManager->SendPlayerUpdate(update);
            lastSyncTime = gameTime;
        }
    }
#endif
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