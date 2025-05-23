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
#include <stdint.h>

// Sprite definitions
typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} SpriteRect;

enum SpriteIndex {
    SPRITE_PLAYER_GUN,
    SPRITE_PLAYER_AXE,
    SPRITE_PLAYER_PLANT,
    SPRITE_TREE_SMALL,
    SPRITE_TREE_LARGE,
    SPRITE_RABBIT,
    SPRITE_DEER,
    SPRITE_RIFLE,
    SPRITE_AXE,
    SPRITE_COUNT
};

SpriteRect spriteRects[SPRITE_COUNT] = {
    [SPRITE_PLAYER_GUN]   = {  0,  0, 20, 20 },
    [SPRITE_PLAYER_AXE]   = { 20,  0, 20, 20 },
    [SPRITE_PLAYER_PLANT] = { 40,  0, 20, 20 },
    [SPRITE_TREE_SMALL]   = {100,  0, 20, 20 },
    [SPRITE_TREE_LARGE]   = {120,  0, 20, 20 },
    [SPRITE_RABBIT]       = { 40, 20, 20, 20 },
    [SPRITE_DEER]         = { 60, 20, 20, 20 },
    [SPRITE_RIFLE]        = {  0, 40, 20, 20 },
    [SPRITE_AXE]          = { 20, 40, 20, 20 }
};

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
    
    // Sprite sheet
    Texture2D spriteSheet;
    bool spritesLoaded = false;
    
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
        // Draw background grass
        Rectangle rect = {
            static_cast<float>(x * CELL_SIZE), 
            static_cast<float>(y * CELL_SIZE), 
            static_cast<float>(CELL_SIZE), 
            static_cast<float>(CELL_SIZE)
        };
        DrawRectangleRec(rect, DARKGREEN);
        
        switch (cell.type) {
            case CellType::EMPTY:
                // Just grass background
                break;
                
            case CellType::SHRUBBERY:
                // Draw small vegetation as darker green patches
                DrawRectangle(x * CELL_SIZE + 4, y * CELL_SIZE + 4, CELL_SIZE - 8, CELL_SIZE - 8, GREEN);
                break;
                
            case CellType::TREE_SEEDLING:
                if (spritesLoaded) {
                    // Small tree sprite with player color tint
                    Color tint = (cell.playerId >= 0) ? PLAYER_COLORS[cell.playerId % 8] : WHITE;
                    DrawSprite(SPRITE_TREE_SMALL, x * CELL_SIZE, y * CELL_SIZE, tint);
                } else {
                    // Fallback: small colored square
                    Color treeColor = (cell.playerId >= 0) ? PLAYER_COLORS[cell.playerId % 8] : GREEN;
                    DrawRectangle(x * CELL_SIZE + 8, y * CELL_SIZE + 8, 4, 4, treeColor);
                }
                break;
                
            case CellType::TREE_YOUNG:
                if (spritesLoaded) {
                    // Medium tree sprite with player color tint
                    Color tint = (cell.playerId >= 0) ? PLAYER_COLORS[cell.playerId % 8] : WHITE;
                    DrawSprite(SPRITE_TREE_SMALL, x * CELL_SIZE, y * CELL_SIZE, tint);
                } else {
                    // Fallback: medium colored square
                    Color treeColor = (cell.playerId >= 0) ? PLAYER_COLORS[cell.playerId % 8] : GREEN;
                    DrawRectangle(x * CELL_SIZE + 6, y * CELL_SIZE + 6, 8, 8, treeColor);
                }
                break;
                
            case CellType::TREE_MATURE:
                if (spritesLoaded) {
                    // Large tree sprite with player color tint
                    Color tint = (cell.playerId >= 0) ? PLAYER_COLORS[cell.playerId % 8] : WHITE;
                    DrawSprite(SPRITE_TREE_LARGE, x * CELL_SIZE, y * CELL_SIZE, tint);
                } else {
                    // Fallback: large colored square
                    Color treeColor = (cell.playerId >= 0) ? PLAYER_COLORS[cell.playerId % 8] : GREEN;
                    DrawRectangle(x * CELL_SIZE + 2, y * CELL_SIZE + 2, CELL_SIZE - 4, CELL_SIZE - 4, treeColor);
                }
                break;
            
            case CellType::GRAVE: {
                // Draw a tombstone-like shape or colored square for grave
                Color graveColor = (cell.playerId >= 0) ? PLAYER_COLORS[cell.playerId % 8] : GRAY;
                DrawRectangle(x * CELL_SIZE + 6, y * CELL_SIZE + 4, 8, 12, graveColor);
                DrawRectangle(x * CELL_SIZE + 4, y * CELL_SIZE + 10, 12, 6, graveColor);
                break;
            }
            
            case CellType::PLAYER:
            case CellType::ANIMAL:
                // These are handled separately in DrawPlayer and DrawAnimal
                break;
        }
    }

    void DrawPlayer(const Player& player) {
        if (!player.alive) return;
        
        // Draw grass background first
        Rectangle rect = {
            static_cast<float>(player.x * CELL_SIZE), 
            static_cast<float>(player.y * CELL_SIZE), 
            static_cast<float>(CELL_SIZE), 
            static_cast<float>(CELL_SIZE)
        };
        DrawRectangleRec(rect, DARKGREEN);
        
        // Draw appropriate player sprite based on mode
        if (spritesLoaded) {
            SpriteIndex spriteIndex;
            switch (player.mode) {
                case PlayerMode::PLANT:
                    spriteIndex = SPRITE_PLAYER_PLANT;
                    break;
                case PlayerMode::SHOOT:
                    spriteIndex = SPRITE_PLAYER_GUN;
                    break;
                case PlayerMode::CHOP:
                    spriteIndex = SPRITE_PLAYER_AXE;
                    break;
            }
            
            // Draw player sprite with color tint
            DrawSprite(spriteIndex, player.x * CELL_SIZE, player.y * CELL_SIZE, player.color);
        } else {
            // Fallback: colored rectangle with mode indicator
            DrawRectangleRec(rect, player.color);
            
            const char* modeChar = "P";
            if (player.mode == PlayerMode::SHOOT) modeChar = "S";
            else if (player.mode == PlayerMode::CHOP) modeChar = "C";
            
            DrawText(modeChar, player.x * CELL_SIZE + 2, player.y * CELL_SIZE + 2, 16, BLACK);
        }
        
        // Draw direction indicator for shooting mode
        if (player.mode == PlayerMode::SHOOT && (player.lastDirectionX != 0 || player.lastDirectionY != 0)) {
            int centerX = player.x * CELL_SIZE + CELL_SIZE / 2;
            int centerY = player.y * CELL_SIZE + CELL_SIZE / 2;
            int endX = centerX + player.lastDirectionX * 12;
            int endY = centerY + player.lastDirectionY * 12;
            
            // Draw aiming line
            DrawLine(centerX, centerY, endX, endY, RED);
            
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
            DrawTriangle(v1, v2, v3, RED);
        }
    }

    void DrawAnimal(const Animal& animal) {
        // Draw grass background first
        Rectangle rect = {
            static_cast<float>(animal.x * CELL_SIZE), 
            static_cast<float>(animal.y * CELL_SIZE), 
            static_cast<float>(CELL_SIZE), 
            static_cast<float>(CELL_SIZE)
        };
        DrawRectangleRec(rect, DARKGREEN);
        
        // Draw appropriate animal sprite
        if (spritesLoaded) {
            SpriteIndex spriteIndex = (animal.type == AnimalType::RABBIT) ? SPRITE_RABBIT : SPRITE_DEER;
            DrawSprite(spriteIndex, animal.x * CELL_SIZE, animal.y * CELL_SIZE);
        } else {
            // Fallback: colored rectangle
            Color animalColor = (animal.type == AnimalType::RABBIT) ? WHITE : BROWN;
            DrawRectangle(animal.x * CELL_SIZE + 4, animal.y * CELL_SIZE + 4, CELL_SIZE - 8, CELL_SIZE - 8, animalColor);
        }
    }

public:
    RobbanPlanterar() : rng(std::chrono::steady_clock::now().time_since_epoch().count()) {
        InitializeGrid();
        SetupNetworking();
        LoadSprites();
        
        // Add local player
        Player localPlayer;
        localPlayer.id = localPlayerId;
        localPlayer.color = PLAYER_COLORS[localPlayerId % 8];
        players[localPlayerId] = localPlayer;
        SpawnPlayer(localPlayerId);
    }
    
    ~RobbanPlanterar() {
        if (spritesLoaded) {
            UnloadTexture(spriteSheet);
        }
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
        SyncGameState();
        
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
        
        // Show sprite loading status
        if (!spritesLoaded) {
            DrawText("Note: robban.png not found - using fallback graphics", 10, 80, 14, YELLOW);
        }
        
        // Show shooting direction
        int uiOffset = spritesLoaded ? 80 : 100;
        if (players[localPlayerId].mode == PlayerMode::SHOOT) {
            const char* dirText = "No direction";
            if (players[localPlayerId].lastDirectionX > 0) dirText = "Shooting →";
            else if (players[localPlayerId].lastDirectionX < 0) dirText = "Shooting ←";
            else if (players[localPlayerId].lastDirectionY > 0) dirText = "Shooting ↓";
            else if (players[localPlayerId].lastDirectionY < 0) dirText = "Shooting ↑";
            
            DrawText(dirText, 10, uiOffset, 16, YELLOW);
            uiOffset += 20;
        }
        
        // Multiplayer UI
        if (isMultiplayer && networkManager) {
            DrawText(TextFormat("Room: %s", currentRoom.c_str()), 10, uiOffset, 16, WHITE);
            DrawText(TextFormat("Players: %d", networkManager->GetPlayerCount()), 10, uiOffset + 20, 16, WHITE);
            
            if (networkManager->IsHost()) {
                DrawText("HOST", 10, uiOffset + 40, 16, YELLOW);
            }
        } else {
            DrawText("Press H to host, J to join", 10, uiOffset, 16, WHITE);
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