#include "raylib.h"
#include "NetworkManager.h"
#include "GameState.h"
#include <vector>
#include <map>
#include <random>
#include <chrono>
#include <memory>
#include <string>
#include <algorithm>
#include <iostream>
#include <stdint.h>

// Normal game mode

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
    // Let's start with very simple coordinates from the top-left
    // Using small squares that should definitely be safe
    [SPRITE_PLAYER_GUN]   = {   32,   480,  350,  320 },  // Top-left corner
    [SPRITE_PLAYER_AXE]   = {  446,   480,  342,  320 },  // Next to it  
    [SPRITE_PLAYER_PLANT] = { 780,   480,  336,  327 },  // Next to that
    [SPRITE_TREE_SMALL]   = { 100,   0,  210,  368 },  // Small tree
    [SPRITE_TREE_LARGE]   = { 380,   30,  280,  400 },  // Large tree
    [SPRITE_RABBIT]       = { 780,  840,  206,  180 },  
    [SPRITE_DEER]         = { 1070, 790,  265,  230 },  
    [SPRITE_RIFLE]        = { 30,  874,  310,  145 },  
    [SPRITE_AXE]          = { 690,  530,  60,  224 }  
};


#ifdef UNIT_TEST
// Unit test mode - just display all sprites for debugging
class SpriteTest {
private:
    Texture2D spriteSheet;
    bool spritesLoaded = false;
    
public:
    SpriteTest() {
        spriteSheet = LoadTexture("robban.png");
        if (spriteSheet.id > 0) {
            spritesLoaded = true;
            std::cout << "Sprite test: loaded " << spriteSheet.width << "x" << spriteSheet.height << std::endl;
        }
    }
    
    ~SpriteTest() {
        if (spritesLoaded) UnloadTexture(spriteSheet);
    }
    
    void Draw() {
        BeginDrawing();
        ClearBackground(DARKGRAY);
        
        if (!spritesLoaded) {
            DrawText("robban.png not found!", 10, 10, 20, RED);
            EndDrawing();
            return;
        }
        
        DrawText(TextFormat("Sprite Sheet: %dx%d", spriteSheet.width, spriteSheet.height), 10, 10, 20, WHITE);
        DrawText("Press ESC to quit", 10, 35, 16, WHITE);
        
        // Draw all sprites in a grid with labels
        const char* spriteNames[] = {
            "PLAYER_GUN", "PLAYER_AXE", "PLAYER_PLANT", "TREE_SMALL", "TREE_LARGE",
            "RABBIT", "DEER", "RIFLE", "AXE"
        };
        
        for (int i = 0; i < SPRITE_COUNT; i++) {
            int row = i / 3;
            int col = i % 3;
            int x = 50 + col * 300;
            int y = 80 + row * 200;
            
            SpriteRect rect = spriteRects[i];
            
            // Draw sprite info
            DrawText(TextFormat("%s", spriteNames[i]), x, y - 20, 12, WHITE);
            DrawText(TextFormat("(%d,%d) %dx%d", rect.x, rect.y, rect.width, rect.height), x, y - 5, 10, LIGHTGRAY);
            
            // Draw the sprite
            if (rect.x + rect.width <= spriteSheet.width && rect.y + rect.height <= spriteSheet.height) {
                Rectangle source = {
                    static_cast<float>(rect.x), static_cast<float>(rect.y),
                    static_cast<float>(rect.width), static_cast<float>(rect.height)
                };
                Rectangle dest = {
                    static_cast<float>(x), static_cast<float>(y),
                    128.0f, 128.0f  // Fixed display size
                };
                
                DrawTexturePro(spriteSheet, source, dest, {0, 0}, 0.0f, WHITE);
                DrawRectangleLines(x, y, 128, 128, GREEN);
            } else {
                DrawRectangle(x, y, 128, 128, RED);
                DrawText("OUT OF BOUNDS", x + 10, y + 60, 12, WHITE);
            }
        }
        
        EndDrawing();
    }
};

int main() {
    InitWindow(1000, 800, "Sprite Test - Robban Planterar");
    SetTargetFPS(60);
    
    SpriteTest test;
    
    while (!WindowShouldClose()) {
        test.Draw();
    }
    
    CloseWindow();
    return 0;
}
#else


// Game constants
const int GRID_WIDTH = 30;     // Reduced from 40
const int GRID_HEIGHT = 20;    // Reduced from 30
const int CELL_SIZE = 40;      // Doubled from 20
const int WINDOW_WIDTH = GRID_WIDTH * CELL_SIZE;   // Now 1200px
const int WINDOW_HEIGHT = GRID_HEIGHT * CELL_SIZE; // Now 800px
const float TREE_GROWTH_TIME = 10.0f; // seconds
const float ANIMAL_SPAWN_RATE = 0.02f; // probability per frame
const int MAX_ANIMALS = 15;    // Reduced proportionally

// Player colors
const Color PLAYER_COLORS[] = {
    BLUE, RED, GREEN, YELLOW, PURPLE, ORANGE, PINK, BROWN
};


class RobbanPlanterar {
private:
    GameState gameState;
    int localPlayerId = 0;
    std::mt19937 rng;
    float gameTime = 0.0f;
    int nextAnimalId = 0;
    // Sprite sheet
    Texture2D spriteSheet;
    bool spritesLoaded = false;
    
    // Sound effects
    Sound shootSound;
    Sound axeSound;
    bool soundsLoaded = false;
    bool audioResumed = false;
    
    // Networking
    std::unique_ptr<NetworkManager> networkManager;
    bool isMultiplayer = false;
    bool isHost = false;
    std::string currentRoom;
    
    // Game state synchronization

    // Sprite helper functions - MUST be defined before other functions that use them
    void LoadSprites() {
        spriteSheet = LoadTexture("robban.png");
        if (spriteSheet.id > 0) {
            spritesLoaded = true;
            std::cout << "Sprite sheet loaded successfully: " << spriteSheet.width << "x" << spriteSheet.height << std::endl;
        } else {
            std::cout << "Warning: Could not load robban.png, using fallback graphics" << std::endl;
            spritesLoaded = false;
        }
    }
    
    void LoadSounds() {
        // Initialize audio device
        InitAudioDevice();
        
        // Load sound effects
        shootSound = LoadSound("souds/shoot.wav");
        axeSound = LoadSound("souds/axe-cut-1.wav");
        
        // Check if sounds loaded successfully
        if (shootSound.frameCount > 0 && axeSound.frameCount > 0) {
            soundsLoaded = true;
            std::cout << "Sound effects loaded successfully" << std::endl;
        } else {
            soundsLoaded = false;
            std::cout << "Warning: Could not load sound effects" << std::endl;
        }
    }
    
    void DrawSprite(SpriteIndex index, int x, int y, Color tint = WHITE, bool flipX = false) {
        if (!spritesLoaded || spriteSheet.id == 0) return;
        
        SpriteRect rect = spriteRects[index];
        
        // Validate sprite coordinates are within texture bounds
        if (rect.x + rect.width > spriteSheet.width || rect.y + rect.height > spriteSheet.height) {
            std::cout << "Warning: Sprite " << index << " coordinates (" << rect.x << "," << rect.y
                     << " " << rect.width << "x" << rect.height << ") out of bounds for texture "
                     << spriteSheet.width << "x" << spriteSheet.height << std::endl;
            return;
        }
        
        Rectangle source = {
            static_cast<float>(rect.x),
            static_cast<float>(rect.y),
            static_cast<float>(rect.width) * (flipX ? -1.0f : 1.0f),  // Negative width flips horizontally
            static_cast<float>(rect.height)
        };
        Rectangle dest = {
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<float>(CELL_SIZE),
            static_cast<float>(CELL_SIZE)
        };
        
        // Debug: print sprite draw info for first few draws
        static int debugCount = 0;
        if (debugCount < 5) {
            std::cout << "Drawing sprite " << index << " from (" << rect.x << "," << rect.y
                     << ") size " << rect.width << "x" << rect.height << " to (" << x << "," << y << ")" << std::endl;
            debugCount++;
        }
        
        DrawTexturePro(spriteSheet, source, dest, {0, 0}, 0.0f, tint);
    }

    void SetupNetworking() {
        networkManager = std::make_unique<NetworkManager>();
        
        // Set up network callbacks
        networkManager->SetPlayerIdAssignedCallback([this](int playerId) {
            this->localPlayerId = playerId;
            std::cout << "Assigned player ID: " << playerId << std::endl;
        });

        networkManager->SetPlayerJoinCallback([this](int playerId) {
            this->OnPlayerJoin(playerId);
        });
        
        networkManager->SetPlayerLeaveCallback([this](int playerId) {
            this->OnPlayerLeave(playerId);
        });
        
        networkManager->SetPlayerUpdateCallback([this](const Player& update) {
            this->OnPlayerUpdate(update);
        });
        
        networkManager->SetFullGameStateCallback([this](const GameState& state) {
            this->OnFullGameState(state);
        });
        
        networkManager->SetPlayerActionCallback([this](const ActionMessage& action) {
            this->OnPlayerAction(action);
        });
    }
    
    void OnPlayerJoin(int playerId) {
        std::cout << "Player " << playerId << " joined the game" << std::endl;
        AddPlayer(playerId);

        if (networkManager->IsHost()) {
            networkManager->SendGameState(gameState);
            networkManager->AssignPlayerId(playerId);
            std::cout << "Sent game state and assigned ID to new player " << playerId << std::endl;
        }
    }
    
    void OnPlayerLeave(int playerId) {
        std::cout << "Player " << playerId << " left the game" << std::endl;
        RemovePlayer(playerId);
    }
    
    void OnPlayerUpdate(const Player& update) {
        if (gameState.players.find(update.id) != gameState.players.end()) {
            Player& player = gameState.players[update.id];
            player.x = update.x;
            player.y = update.y;
            player.mode = update.mode;
            player.score = update.score;
            player.alive = update.alive;
            std::cout << "Updated player " << update.id << " position: (" << update.x << "," << update.y << ")" << std::endl;
        } else {
            // If player doesn't exist yet, add them
            std::cout << "Adding new player " << update.id << " from network update" << std::endl;
            AddPlayer(update.id);
            Player& player = gameState.players[update.id];
            player.x = update.x;
            player.y = update.y;
            player.mode = update.mode;
            player.score = update.score;
            player.alive = update.alive;
        }
    }
    
    void OnPlayerAction(const ActionMessage& action) {
        HandlePlayerAction(action.playerId, action.targetX, action.targetY);
    }
    
    void OnFullGameState(const GameState& state) {
        std::cout << "[Game] Applying full game state..." << std::endl;
        gameState = state;
    }
    
    void InitializeGrid() {
        gameState.grid.resize(GRID_HEIGHT, std::vector<Cell>(GRID_WIDTH));
        
        // Add some initial shrubbery (reduced for smaller grid)
        for (int i = 0; i < 60; i++) {  // Reduced from 100
            int x = rng() % GRID_WIDTH;
            int y = rng() % GRID_HEIGHT;
            if (gameState.grid[y][x].type == CellType::EMPTY) {
                gameState.grid[y][x].type = CellType::SHRUBBERY;
            }
        }
    }

    void SpawnPlayer(int playerId) {
        Player& player = gameState.players[playerId];
        
        // Spawn in random corner
        std::vector<std::pair<int, int>> corners = {
            {0, 0}, {GRID_WIDTH-1, 0}, {0, GRID_HEIGHT-1}, {GRID_WIDTH-1, GRID_HEIGHT-1}
        };
        
        auto corner = corners[rng() % corners.size()];
        player.x = corner.first;
        player.y = corner.second;
        player.alive = true;
        
        // Clear the spawn location
        gameState.grid[player.y][player.x].type = CellType::EMPTY;
    }

    void UpdateBullets() {
        const float BULLET_SPEED = 8.0f; // cells per second
        const float BULLET_LIFETIME = 2.0f; // seconds
        
        for (auto it = gameState.bullets.begin(); it != gameState.bullets.end();) {
            Bullet& bullet = *it;
            
            // Remove old bullets
            if (gameTime - bullet.startTime > BULLET_LIFETIME) {
                it = gameState.bullets.erase(it);
                continue;
            }
            
            if (!bullet.active) {
                it = gameState.bullets.erase(it);
                continue;
            }
            
            // Calculate how far the bullet should have traveled
            float travelTime = gameTime - bullet.startTime;
            float distance = travelTime * BULLET_SPEED;
            
            int newX = bullet.x + static_cast<int>(bullet.dirX * distance);
            int newY = bullet.y + static_cast<int>(bullet.dirY * distance);
            
            // Check bounds
            if (newX < 0 || newX >= GRID_WIDTH || newY < 0 || newY >= GRID_HEIGHT) {
                it = gameState.bullets.erase(it);
                continue;
            }
            
            // Check for hits with animals
            for (auto animalIt = gameState.animals.begin(); animalIt != gameState.animals.end(); ++animalIt) {
                if (animalIt->x == newX && animalIt->y == newY) {
                    // Hit animal
                    if (gameState.players.find(bullet.playerId) != gameState.players.end()) {
                        gameState.players[bullet.playerId].score += 5;
                    }
                    gameState.animals.erase(animalIt);
                    bullet.active = false;
                    break;
                }
            }
            
            if (!bullet.active) {
                it = gameState.bullets.erase(it);
                continue;
            }
            
            // Check for hits with other players
            for (auto& [id, otherPlayer] : gameState.players) {
                if (id != bullet.playerId && otherPlayer.x == newX && otherPlayer.y == newY && otherPlayer.alive) {
                    otherPlayer.alive = false;
                    if (gameState.players.find(bullet.playerId) != gameState.players.end()) {
                        gameState.players[bullet.playerId].score -= 5;
                    }
                    
                    // Create grave
                    gameState.grid[newY][newX].type = CellType::GRAVE;
                    gameState.grid[newY][newX].playerId = id;
                    
                    // Respawn the killed player
                    SpawnPlayer(id);
                    bullet.active = false;
                    break;
                }
            }
            
            if (!bullet.active) {
                it = gameState.bullets.erase(it);
                continue;
            }
            
            // Check for obstacles (trees)
            Cell& cell = gameState.grid[newY][newX];
            if (cell.type == CellType::TREE_MATURE || cell.type == CellType::TREE_YOUNG) {
                bullet.active = false;
                it = gameState.bullets.erase(it);
                continue;
            }
            
            ++it;
        }
    }
    
    void UpdateAnimals() {
        // Spawn new animals
        if (gameState.animals.size() < MAX_ANIMALS && (rng() % 1000) < (ANIMAL_SPAWN_RATE * 1000)) {
            Animal animal;
            animal.type = (rng() % 2 == 0) ? AnimalType::RABBIT : AnimalType::DEER;
            animal.x = rng() % GRID_WIDTH;
            animal.y = rng() % GRID_HEIGHT;
            animal.id = nextAnimalId++;
            animal.moveDelay = 0.5f + (rng() % 100) / 100.0f;
            
            if (gameState.grid[animal.y][animal.x].type == CellType::EMPTY) {
                gameState.animals.push_back(animal);
            }
        }

        // Move and update animals
        for (auto& animal : gameState.animals) {
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
                        Cell& cell = gameState.grid[testY][testX];
                        
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
                Cell& cell = gameState.grid[y][x];
                
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
        if (gameState.players.find(playerId) == gameState.players.end() || !gameState.players[playerId].alive) return;
        
        Player& player = gameState.players[playerId];
        
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
                
                Cell& cell = gameState.grid[plantY][plantX];
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
                
                Cell& cell = gameState.grid[chopY][chopX];
                if (cell.type == CellType::TREE_MATURE) {
                    cell.type = CellType::EMPTY;
                    cell.playerId = -1;
                    cell.growth = 0.0f;
                    player.score += 10;
                    
                    // Play axe sound effect
                    if (soundsLoaded && audioResumed) {
                        PlaySound(axeSound);
                    }
                }
                break;
            }
                
            case PlayerMode::SHOOT: {
                // Fire a bullet in the direction the player last moved
                int dirX = player.lastDirectionX;
                int dirY = player.lastDirectionY;
                
                // If no direction set, default to right
                if (dirX == 0 && dirY == 0) {
                    dirX = 1;
                }
                
                // Create bullet
                Bullet bullet;
                bullet.x = player.x;
                bullet.y = player.y;
                bullet.dirX = dirX;
                bullet.dirY = dirY;
                bullet.playerId = playerId;
                bullet.startTime = gameTime;
                bullet.active = true;
                
                gameState.bullets.push_back(bullet);
                
                // Play shoot sound effect
                if (soundsLoaded && audioResumed) {
                    PlaySound(shootSound);
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
                // Draw more detailed vegetation as multiple green patches
                DrawRectangle(x * CELL_SIZE + 8, y * CELL_SIZE + 8, CELL_SIZE - 16, CELL_SIZE - 16, GREEN);
                DrawRectangle(x * CELL_SIZE + 4, y * CELL_SIZE + 12, 8, 8, LIME);
                DrawRectangle(x * CELL_SIZE + CELL_SIZE - 12, y * CELL_SIZE + 6, 6, 6, LIME);
                break;
                
            case CellType::TREE_SEEDLING:
                if (spritesLoaded) {
                    // Small tree sprite with player color tint
                    Color tint = (cell.playerId >= 0) ? PLAYER_COLORS[cell.playerId % 8] : WHITE;
                    DrawSprite(SPRITE_TREE_SMALL, x * CELL_SIZE, y * CELL_SIZE, tint);
                } else {
                    // Better fallback: small tree shape
                    Color treeColor = (cell.playerId >= 0) ? PLAYER_COLORS[cell.playerId % 8] : GREEN;
                    // Draw a small tree-like shape
                    DrawRectangle(x * CELL_SIZE + 18, y * CELL_SIZE + 28, 4, 8, BROWN); // trunk
                    DrawCircle(x * CELL_SIZE + 20, y * CELL_SIZE + 24, 8, treeColor);   // leaves
                }
                break;
                
            case CellType::TREE_YOUNG:
                if (spritesLoaded) {
                    // Medium tree sprite with player color tint
                    Color tint = (cell.playerId >= 0) ? PLAYER_COLORS[cell.playerId % 8] : WHITE;
                    DrawSprite(SPRITE_TREE_SMALL, x * CELL_SIZE, y * CELL_SIZE, tint);
                } else {
                    // Better fallback: medium tree shape
                    Color treeColor = (cell.playerId >= 0) ? PLAYER_COLORS[cell.playerId % 8] : GREEN;
                    // Draw a medium tree-like shape
                    DrawRectangle(x * CELL_SIZE + 16, y * CELL_SIZE + 24, 8, 12, BROWN); // trunk
                    DrawCircle(x * CELL_SIZE + 20, y * CELL_SIZE + 18, 12, treeColor);   // leaves
                }
                break;
                
            case CellType::TREE_MATURE:
                if (spritesLoaded) {
                    // Large tree sprite with player color tint
                    Color tint = (cell.playerId >= 0) ? PLAYER_COLORS[cell.playerId % 8] : WHITE;
                    DrawSprite(SPRITE_TREE_LARGE, x * CELL_SIZE, y * CELL_SIZE, tint);
                } else {
                    // Better fallback: large tree shape
                    Color treeColor = (cell.playerId >= 0) ? PLAYER_COLORS[cell.playerId % 8] : GREEN;
                    // Draw a large tree-like shape
                    DrawRectangle(x * CELL_SIZE + 14, y * CELL_SIZE + 20, 12, 16, BROWN); // trunk
                    DrawCircle(x * CELL_SIZE + 20, y * CELL_SIZE + 12, 16, treeColor);     // leaves
                    DrawCircle(x * CELL_SIZE + 16, y * CELL_SIZE + 16, 10, treeColor);     // extra leaves
                    DrawCircle(x * CELL_SIZE + 24, y * CELL_SIZE + 16, 10, treeColor);     // extra leaves
                }
                break;
            
            case CellType::GRAVE: {
                // Draw a more detailed tombstone-like shape (scaled up)
                Color graveColor = (cell.playerId >= 0) ? PLAYER_COLORS[cell.playerId % 8] : GRAY;
                DrawRectangle(x * CELL_SIZE + 12, y * CELL_SIZE + 8, 16, 24, graveColor);
                DrawRectangle(x * CELL_SIZE + 8, y * CELL_SIZE + 20, 24, 12, graveColor);
                // Add some detail
                DrawRectangle(x * CELL_SIZE + 14, y * CELL_SIZE + 12, 12, 2, DARKGRAY);
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
            
            // Flip sprite horizontally when moving left (lastDirectionX < 0)
            bool flipX = (player.lastDirectionX < 0);
            
            // Draw player sprite with color tint and flip if moving left
            DrawSprite(spriteIndex, player.x * CELL_SIZE, player.y * CELL_SIZE, player.color, flipX);
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

    void DrawBullet(const Bullet& bullet) {
        // Calculate current position
        float travelTime = gameTime - bullet.startTime;
        float distance = travelTime * 8.0f; // Same speed as UpdateBullets
        
        float currentX = bullet.x * CELL_SIZE + bullet.dirX * distance * CELL_SIZE;
        float currentY = bullet.y * CELL_SIZE + bullet.dirY * distance * CELL_SIZE;
        
        // Draw bullet as a small yellow circle
        DrawCircle(static_cast<int>(currentX + CELL_SIZE/2), static_cast<int>(currentY + CELL_SIZE/2), 3, YELLOW);
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
            DrawRectangle(animal.x * CELL_SIZE + 8, animal.y * CELL_SIZE + 8, CELL_SIZE - 16, CELL_SIZE - 16, animalColor);
            
            // Add some simple detail for animals
            if (animal.type == AnimalType::RABBIT) {
                // Rabbit ears
                DrawRectangle(animal.x * CELL_SIZE + 12, animal.y * CELL_SIZE + 4, 4, 8, WHITE);
                DrawRectangle(animal.x * CELL_SIZE + 20, animal.y * CELL_SIZE + 4, 4, 8, WHITE);
            } else {
                // Deer antlers
                DrawRectangle(animal.x * CELL_SIZE + 10, animal.y * CELL_SIZE + 4, 2, 6, BROWN);
                DrawRectangle(animal.x * CELL_SIZE + 24, animal.y * CELL_SIZE + 4, 2, 6, BROWN);
            }
        }
    }

public:
    RobbanPlanterar() : rng(std::chrono::steady_clock::now().time_since_epoch().count()) {
        InitializeGrid();
        SetupNetworking();
        LoadSprites();
        LoadSounds();
        
        // Add local player
        Player localPlayer;
        localPlayer.id = localPlayerId;
        localPlayer.color = PLAYER_COLORS[localPlayerId % 8];
        gameState.players[localPlayerId] = localPlayer;
        SpawnPlayer(localPlayerId);
    }
    
    ~RobbanPlanterar() {
        if (spritesLoaded && spriteSheet.id > 0) {
            UnloadTexture(spriteSheet);
            spritesLoaded = false;
        }
        
        if (soundsLoaded) {
            UnloadSound(shootSound);
            UnloadSound(axeSound);
            soundsLoaded = false;
        }
        
        CloseAudioDevice();
    }

    void Update() {
        gameTime = GetTime();

        // Resume audio context on first user interaction (required for web browsers)
        #ifdef PLATFORM_WEB
        if (!audioResumed && soundsLoaded) {
            // Check if any key is pressed
            if (GetKeyPressed() != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                // Resume audio context for web
                audioResumed = true;
                std::cout << "Audio context resumed after user interaction" << std::endl;
            }
        }
        #else
        audioResumed = true; // Native platforms don't need this
        #endif

        Player& localPlayer = gameState.players[localPlayerId];

        // Send periodic network updates (every 100ms)
        static float lastNetworkUpdate = 0.0f;
        if (gameTime - lastNetworkUpdate > 0.1f && isMultiplayer) {
            // Send local player update
            networkManager->SendPlayerUpdate(localPlayer);
            lastNetworkUpdate = gameTime;
        }
        
        // Network controls
        if (!isMultiplayer) {
            if (IsKeyPressed(KEY_H)) {
                // Host a game
                currentRoom = "RobbanRoom";
                 isMultiplayer = true;
                    isHost = true;
                if (!networkManager->CreateRoom(currentRoom)) {
                     isMultiplayer = false;
                    isHost = false;
                    std::cout << "[Game] Failed to create room!" << std::endl;
                } else {
                    std::cout << "[Game] Hosting room: " << networkManager->GetRoomId() << std::endl;
                }
            } else if (IsKeyPressed(KEY_J)) {
                // Join a game (simplified - in real version would show input dialog)
                currentRoom = "RobbanRoom_1234"; // Example room ID
                isMultiplayer = true;
                isHost = false;
                if (!networkManager->JoinRoom(currentRoom)) {
                     isMultiplayer = false;
                    isHost = false;
                    std::cout << "[Game] Failed to join room!" << std::endl;
                } else {
                    std::cout << "[Game] Joining room: " << currentRoom << std::endl;
                }
            }
        } else {
            // Debug: show multiplayer status
            static float lastStatusLog = 0.0f;
            if (gameTime - lastStatusLog > 2.0f) { // Log every 2 seconds
                std::cout << "[Game] Multiplayer active - Host: " << (isHost ? "yes" : "no")
                         << " Room: " << currentRoom << " Players: " << gameState.players.size() << std::endl;
                lastStatusLog = gameTime;
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
        UpdateBullets();
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
                DrawCell(x, y, gameState.grid[y][x]);
            }
        }
        
        // Draw animals
        for (const auto& animal : gameState.animals) {
            DrawAnimal(animal);
        }
        
        // Draw bullets
        for (const auto& bullet : gameState.bullets) {
            DrawBullet(bullet);
        }
        
        // Draw players
        for (const auto& [id, player] : gameState.players) {
            DrawPlayer(player);
        }
        
        // Draw UI
        DrawText(TextFormat("Score: %d", gameState.players[localPlayerId].score), 10, 10, 20, WHITE);
        
        const char* modeText = "Plant";
        if (gameState.players[localPlayerId].mode == PlayerMode::SHOOT) modeText = "Shoot";
        else if (gameState.players[localPlayerId].mode == PlayerMode::CHOP) modeText = "Chop";
        
        DrawText(TextFormat("Mode: %s (P to switch)", modeText), 10, 35, 20, WHITE);
        DrawText("WASD/Arrows: Move, SPACE: Action", 10, 60, 16, WHITE);
        
        // Show sprite loading status and debug info
        if (!spritesLoaded) {
            DrawText("Note: robban.png not found - using fallback graphics", 10, 80, 14, YELLOW);
        } else {
            DrawText(TextFormat("Using sprites: %dx%d", spriteSheet.width, spriteSheet.height), 10, 80, 14, GREEN);
        }
        
        // Show audio status
        #ifdef PLATFORM_WEB
        if (soundsLoaded && !audioResumed) {
            DrawText("Press any key to enable audio", 10, 100, 16, YELLOW);
        }
        #endif
        
        // Show shooting direction
        int uiOffset = spritesLoaded ? 80 : 100;
        if (gameState.players[localPlayerId].mode == PlayerMode::SHOOT) {
            const char* dirText = "No direction";
            if (gameState.players[localPlayerId].lastDirectionX > 0) dirText = "Shooting →";
            else if (gameState.players[localPlayerId].lastDirectionX < 0) dirText = "Shooting ←";
            else if (gameState.players[localPlayerId].lastDirectionY > 0) dirText = "Shooting ↓";
            else if (gameState.players[localPlayerId].lastDirectionY < 0) dirText = "Shooting ↑";
            
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
        if (gameState.players.find(playerId) == gameState.players.end()) {
            Player newPlayer;
            newPlayer.id = playerId;
            newPlayer.color = PLAYER_COLORS[playerId % 8];
            gameState.players[playerId] = newPlayer;
            SpawnPlayer(playerId);
        }
    }

    void RemovePlayer(int playerId) {
        gameState.players.erase(playerId);
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


#endif