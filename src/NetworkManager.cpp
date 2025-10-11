#include "NetworkManager.h"
#include "GameState.h"
#include <iostream>
#include <sstream>
#include <random>
#include <chrono>
#include <algorithm>

// Callback function pointer for peer ready event
typedef void (*PeerReadyCallback)(const char* peerId);
static PeerReadyCallback g_peerReadyCallback = nullptr;

// Function to set the callback from outside
extern "C" void SetPeerReadyCallback(PeerReadyCallback callback) {
    g_peerReadyCallback = callback;
}

#ifdef PLATFORM_WEB
#include <emscripten.h>

// JavaScript bridge functions
extern "C" {
    int JS_InitPeerNetwork();
    int JS_CreateRoom();
    int JS_JoinRoom(const char* roomId);
    void JS_BroadcastMessage(const char* message);
    void JS_SendMessageTo(const char* peerId, const char* message);
    int JS_GetRoomId(char* buffer, int bufferSize);
    int JS_GetConnectionCount();
    void JS_DisconnectPeer();
}

// Global network manager pointer for callbacks
static NetworkManager* g_networkManager = nullptr;

// Callbacks from JavaScript to C++
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void OnPeerReady(const char* peerId) {
        std::cout << "[C++] Peer ready with ID: " << peerId << std::endl;
        
        // Call the registered callback if available
        if (g_peerReadyCallback) {
            g_peerReadyCallback(peerId);
        }
    }
    
    EMSCRIPTEN_KEEPALIVE
    void OnPlayerJoined(const char* peerId) {
        std::cout << "[C++] Player joined: " << peerId << std::endl;
        if (g_networkManager) {
            g_networkManager->HandlePlayerJoined(peerId);
        }
    }
    
    EMSCRIPTEN_KEEPALIVE
    void OnPlayerLeft(const char* peerId) {
        std::cout << "[C++] Player left: " << peerId << std::endl;
    }
    
    EMSCRIPTEN_KEEPALIVE
    void OnNetworkMessage(const char* message) {
        // Parse message type first to filter logging
        std::string msgStr(message);
        std::string type;
        {
            std::string search = "\"type\":";
            size_t pos = msgStr.find(search);
            if (pos != std::string::npos) {
                pos += search.length();
                while (pos < msgStr.length() && (msgStr[pos] == ' ' || msgStr[pos] == '\t' || msgStr[pos] == '"')) pos++;
                size_t endPos = msgStr.find('"', pos);
                if (endPos != std::string::npos) {
                    type = msgStr.substr(pos, endPos - pos);
                }
            }
        }
        
        // Only log non-FULL_GAME_STATE messages to reduce spam
        if (type != "FULL_GAME_STATE") {
            std::cout << "[C++] Network message received: " << type << std::endl;
        }

        // Host only rebroadcasts certain message types, not player moves
        if (g_networkManager && g_networkManager->IsHost()) {
            std::string msgStr(message);
            auto extractType = [&]() -> std::string {
                std::string search = "\"type\":";
                size_t pos = msgStr.find(search);
                if (pos == std::string::npos) return "";
                pos += search.length();
                while (pos < msgStr.length() && (msgStr[pos] == ' ' || msgStr[pos] == '\t' || msgStr[pos] == '"')) pos++;
                size_t endPos = msgStr.find('"', pos);
                if (endPos == std::string::npos) return "";
                return msgStr.substr(pos, endPos - pos);
            };
            
            std::string type = extractType();
            // Only rebroadcast actions, not player moves (clients already see their own moves)
            if (type == "PLAYER_ACTION") {
                std::cout << "[C++] Host rebroadcasting PLAYER_ACTION" << std::endl;
                JS_BroadcastMessage(message);
            }
        }

        // Parse JSON message and update game state
        // (msgStr already defined above for type checking)

        // Simple JSON parsing for key-value pairs
        auto extractValue = [&](const std::string& key) -> std::string {
            std::string search = "\"" + key + "\":";
            size_t pos = msgStr.find(search);
            if (pos == std::string::npos) return "";

            pos += search.length();
            // Skip whitespace
            while (pos < msgStr.length() && (msgStr[pos] == ' ' || msgStr[pos] == '\t')) pos++;

            if (pos >= msgStr.length()) return "";

            if (msgStr[pos] == '"') {
                // String value
                pos++; // Skip opening quote
                size_t endPos = msgStr.find('"', pos);
                if (endPos == std::string::npos) return "";
                return msgStr.substr(pos, endPos - pos);
            } else if (msgStr[pos] == '[') {
                // Array value - find matching closing bracket
                size_t endPos = pos + 1;
                int bracketCount = 1;
                while (endPos < msgStr.length() && bracketCount > 0) {
                    if (msgStr[endPos] == '[') bracketCount++;
                    else if (msgStr[endPos] == ']') bracketCount--;
                    endPos++;
                }
                return msgStr.substr(pos, endPos - pos);
            } else {
                // Numeric or boolean value
                size_t endPos = pos;
                while (endPos < msgStr.length() && msgStr[endPos] != ',' && msgStr[endPos] != '}') endPos++;
                return msgStr.substr(pos, endPos - pos);
            }
        };

        try {
            std::string type = extractValue("type");
            // Don't log FULL_GAME_STATE messages to reduce spam
            if (type != "FULL_GAME_STATE") {
                std::cout << "[C++] Message type: " << type << std::endl;
            }

            if (type == "PLAYER_MOVE") {
                Player update = {};

                std::string idStr = extractValue("playerId");
                std::string xStr = extractValue("x");
                std::string yStr = extractValue("y");
                std::string modeStr = extractValue("mode");
                std::string scoreStr = extractValue("score");
                std::string aliveStr = extractValue("alive");
                std::string dirXStr = extractValue("dirX");
                std::string dirYStr = extractValue("dirY");

                if (!idStr.empty()) update.id = std::stoi(idStr);
                if (!xStr.empty()) update.x = std::stoi(xStr);
                if (!yStr.empty()) update.y = std::stoi(yStr);
                if (!modeStr.empty()) update.mode = static_cast<PlayerMode>(std::stoi(modeStr));
                if (!scoreStr.empty()) update.score = std::stoi(scoreStr);
                update.alive = (aliveStr == "true");
                if (!dirXStr.empty()) update.lastDirectionX = std::stoi(dirXStr);
                if (!dirYStr.empty()) update.lastDirectionY = std::stoi(dirYStr);

                // Update player if we have a valid network manager
                if (g_networkManager) {
                    g_networkManager->OnPlayerUpdate(update);
                }

            } else if (type == "PLAYER_ACTION") {
                ActionMessage action = {};

                std::string idStr = extractValue("playerId");
                std::string xStr = extractValue("targetX");
                std::string yStr = extractValue("targetY");
                std::string actionStr = extractValue("actionType");

                if (!idStr.empty()) action.playerId = std::stoi(idStr);
                if (!xStr.empty()) action.targetX = std::stoi(xStr);
                if (!yStr.empty()) action.targetY = std::stoi(yStr);
                if (!actionStr.empty()) action.actionType = std::stoi(actionStr);

                std::cout << "[C++] Player action: ID=" << action.playerId << " type=" << action.actionType << std::endl;

                // Handle player action if we have a valid network manager
                if (g_networkManager) {
                    g_networkManager->OnPlayerAction(action);
                }

            } else if (type == "PLAYER_MODE_CHANGE") {
                std::string idStr = extractValue("playerId");
                std::string modeStr = extractValue("mode");

                int playerId = idStr.empty() ? -1 : std::stoi(idStr);
                int newMode = modeStr.empty() ? 0 : std::stoi(modeStr);

                std::cout << "[C++] Player " << playerId << " changed mode to " << newMode << std::endl;
            } else if (type == "FULL_GAME_STATE") {
                 if (g_networkManager) {
                    GameState state;
                    std::string grid_str = extractValue("grid");
                    std::string players_str = extractValue("players");

                    // Parse grid
                    std::stringstream grid_ss(grid_str);
                    std::string row_token;
                    int y = 0;
                    while(std::getline(grid_ss, row_token, '|')) {
                        state.grid.resize(y + 1);
                        std::stringstream cell_row_ss(row_token);
                        std::string cell_token;
                        int x = 0;
                        while(std::getline(cell_row_ss, cell_token, ';')) {
                           state.grid[y].resize(x + 1);
                           std::stringstream cell_props_ss(cell_token);
                           std::string prop;
                           std::getline(cell_props_ss, prop, ',');
                           state.grid[y][x].type = static_cast<CellType>(std::stoi(prop));
                           std::getline(cell_props_ss, prop, ',');
                           state.grid[y][x].playerId = std::stoi(prop);
                           std::getline(cell_props_ss, prop, ',');
                           state.grid[y][x].growth = std::stof(prop);
                           x++;
                        }
                        y++;
                    }

                     size_t current_pos = 0;
                     while(current_pos < players_str.length()) {
                        size_t start_obj = players_str.find('{', current_pos);
                        if (start_obj == std::string::npos) break;
                        size_t end_obj = players_str.find('}', start_obj);
                        if (end_obj == std::string::npos) break;

                        std::string player_obj_str = players_str.substr(start_obj, end_obj - start_obj + 1);
                        
                         auto extractPlayerValue = [&](const std::string& key) -> std::string {
                             std::string search = "\"" + key + "\":";
                             size_t pos = player_obj_str.find(search);
                             if (pos == std::string::npos) return "";
                             pos += search.length();
                             if (player_obj_str[pos] == '"') {
                                 pos++;
                                 size_t endPos = player_obj_str.find('"', pos);
                                 return player_obj_str.substr(pos, endPos - pos);
                             } else {
                                 size_t endPos = pos;
                                 while (endPos < player_obj_str.length() && player_obj_str[endPos] != ',' && player_obj_str[endPos] != '}') endPos++;
                                 return player_obj_str.substr(pos, endPos - pos);
                             }
                         };

                        Player p;
                        p.id = std::stoi(extractPlayerValue("id"));
                        p.x = std::stoi(extractPlayerValue("x"));
                        p.y = std::stoi(extractPlayerValue("y"));
                        p.mode = static_cast<PlayerMode>(std::stoi(extractPlayerValue("mode")));
                        p.score = std::stoi(extractPlayerValue("score"));
                        p.alive = extractPlayerValue("alive") == "true";
                        
                        std::string dirXStr = extractPlayerValue("dirX");
                        std::string dirYStr = extractPlayerValue("dirY");
                        if (!dirXStr.empty()) p.lastDirectionX = std::stoi(dirXStr);
                        if (!dirYStr.empty()) p.lastDirectionY = std::stoi(dirYStr);
                        
                        // Extract username
                        std::string usernameStr = extractPlayerValue("username");
                        if (!usernameStr.empty()) {
                            p.username = usernameStr;
                        }

                        // Set player color based on ID (same as in AddPlayer)
                        const Color PLAYER_COLORS[] = {
                            BLUE, RED, GREEN, YELLOW, PURPLE, ORANGE, PINK, BROWN
                        };
                        p.color = PLAYER_COLORS[p.id % 8];

                        state.players[p.id] = p;

                        current_pos = end_obj + 1;
                     }
                     
                     // Parse animals
                     std::string animals_str = extractValue("animals");
                     current_pos = 0;
                     while(current_pos < animals_str.length()) {
                        size_t start_obj = animals_str.find('{', current_pos);
                        if (start_obj == std::string::npos) break;
                        size_t end_obj = animals_str.find('}', start_obj);
                        if (end_obj == std::string::npos) break;

                        std::string animal_obj_str = animals_str.substr(start_obj, end_obj - start_obj + 1);
                        
                        auto extractAnimalValue = [&](const std::string& key) -> std::string {
                            std::string search = "\"" + key + "\":";
                            size_t pos = animal_obj_str.find(search);
                            if (pos == std::string::npos) return "";
                            pos += search.length();
                            size_t endPos = pos;
                            while (endPos < animal_obj_str.length() && animal_obj_str[endPos] != ',' && animal_obj_str[endPos] != '}') endPos++;
                            return animal_obj_str.substr(pos, endPos - pos);
                        };

                        Animal a;
                        a.id = std::stoi(extractAnimalValue("id"));
                        a.type = static_cast<AnimalType>(std::stoi(extractAnimalValue("type")));
                        a.x = std::stoi(extractAnimalValue("x"));
                        a.y = std::stoi(extractAnimalValue("y"));

                        state.animals.push_back(a);
                        current_pos = end_obj + 1;
                     }
                     
                     // Note: Bullets are NOT parsed from game state
                     // They are created via PLAYER_ACTION messages which are already synced
                     
                     g_networkManager->OnFullGameState(state);
                }
            } else if (type == "ASSIGN_PLAYER_ID") {
                if (g_networkManager && g_networkManager->onPlayerIdAssigned) {
                    g_networkManager->onPlayerIdAssigned(std::stoi(extractValue("playerId")));
                }
            }
             else {
                std::cout << "[C++] Unknown message type: " << type << std::endl;
            }

        } catch (const std::exception& e) {
            std::cerr << "[C++] Error parsing network message: " << e.what() << " (message: " << message << ")" << std::endl;
        }
    }
    
    // UI button callbacks
    EMSCRIPTEN_KEEPALIVE
    void OnHostGameClicked() {
        std::cout << "[C++] Host game button clicked" << std::endl;
        if (g_networkManager) {
            g_networkManager->CreateRoom("RobbanRoom");
            // Host automatically gets player ID 0
            if (g_networkManager->onPlayerIdAssigned) {
                g_networkManager->onPlayerIdAssigned(0);
            }
        }
    }
    
    EMSCRIPTEN_KEEPALIVE
    void OnJoinGameClicked(const char* roomId) {
        std::cout << "[C++] Join game button clicked with room: " << roomId << std::endl;
        if (g_networkManager) {
            g_networkManager->JoinRoom(roomId);
        }
    }
    
    EMSCRIPTEN_KEEPALIVE
    void OnDisconnectClicked() {
        std::cout << "[C++] Disconnect button clicked" << std::endl;
        if (g_networkManager) {
            g_networkManager->Disconnect();
        }
    }
}
#endif

// JSON-like serialization helpers
std::string SerializeAction(const ActionMessage& action) {
    std::ostringstream oss;
    oss << action.playerId << "," << action.targetX << "," << action.targetY << "," << action.actionType;
    return oss.str();
}

ActionMessage DeserializeAction(const std::string& data) {
    ActionMessage action = {};
    std::istringstream iss(data);
    std::string token;
    
    if (std::getline(iss, token, ',')) action.playerId = std::stoi(token);
    if (std::getline(iss, token, ',')) action.targetX = std::stoi(token);
    if (std::getline(iss, token, ',')) action.targetY = std::stoi(token);
    if (std::getline(iss, token, ',')) action.actionType = std::stoi(token);
    
    return action;
}

std::string SerializeGameState(const GameState& state) {
    std::ostringstream oss;
    oss << "{\"type\":\"FULL_GAME_STATE\",";

    // Serialize grid
    oss << "\"grid\":\"";
    for(size_t y = 0; y < state.grid.size(); ++y) {
        for(size_t x = 0; x < state.grid[y].size(); ++x) {
            const auto& cell = state.grid[y][x];
            oss << static_cast<int>(cell.type) << "," << cell.playerId << "," << cell.growth;
            if (x < state.grid[y].size() - 1) {
                oss << ";";
            }
        }
        if (y < state.grid.size() - 1) {
            oss << "|";
        }
    }
    oss << "\",";

    oss << "\"players\":[";
    bool first = true;
    for (const auto& [id, player] : state.players) {
        if (!first) {
            oss << ",";
        }
        oss << "{\"id\":" << player.id
            << ",\"x\":" << player.x
            << ",\"y\":" << player.y
            << ",\"mode\":" << static_cast<int>(player.mode)
            << ",\"score\":" << player.score
            << ",\"alive\":" << (player.alive ? "true" : "false")
            << ",\"dirX\":" << player.lastDirectionX
            << ",\"dirY\":" << player.lastDirectionY
            << ",\"username\":\"" << player.username << "\""
            << "}";
        first = false;
    }
    oss << "],";
    
    // Serialize animals
    oss << "\"animals\":[";
    first = true;
    for (const auto& animal : state.animals) {
        if (!first) {
            oss << ",";
        }
        oss << "{\"id\":" << animal.id
            << ",\"type\":" << static_cast<int>(animal.type)
            << ",\"x\":" << animal.x
            << ",\"y\":" << animal.y
            << "}";
        first = false;
    }
    oss << "]}";
    
    // Note: Bullets are NOT serialized in game state
    // They are created via PLAYER_ACTION messages which are already synced
    
    return oss.str();
}

NetworkManager::NetworkManager() {
    // Initialize random room ID generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    #ifdef PLATFORM_WEB
    // Set global pointer for callbacks
    g_networkManager = this;
    
    // Initialize PeerJS networking on web
    if (JS_InitPeerNetwork()) {
        std::cout << "PeerJS networking initialized" << std::endl;
    } else {
        std::cerr << "Failed to initialize PeerJS networking" << std::endl;
    }
    #endif
}

NetworkManager::~NetworkManager() {
    Disconnect();
}
void NetworkManager::HandlePlayerJoined(const std::string& peerId) {
    int newPlayerId = connectedPeers.size() + 1; // Simple ID assignment for now
    connectedPeers[newPlayerId] = peerId;
    if (onPlayerJoin) {
        onPlayerJoin(newPlayerId);
    }
}
bool NetworkManager::CreateRoom(const std::string& roomName) {
#ifdef PLATFORM_WEB
    // On web, PeerJS auto-generates room ID
    if (JS_CreateRoom()) {
        isHost = true;
        isConnected = true;
        
        // Get the generated room ID from JS
        char buffer[256];
        if (JS_GetRoomId(buffer, sizeof(buffer))) {
            roomId = std::string(buffer);
            std::cout << "Created room with ID: " << roomId << std::endl;
            std::cout << "Share this ID with others to join!" << std::endl;
            return true;
        }
    }
    return false;
    #else
    // Native build - use threads
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    roomId = roomName + "_" + std::to_string(dis(gen));
    
    isHost = true;
    isConnected = true;
    
    shouldStop = false;
    networkThread = std::thread(&NetworkManager::NetworkLoop, this);
    
    std::cout << "Created room: " << roomId << std::endl;
    return true;
    #endif
}

bool NetworkManager::JoinRoom(const std::string& targetRoomId) {
    #ifdef PLATFORM_WEB
    // On web, use PeerJS to connect
    if (JS_JoinRoom(targetRoomId.c_str())) {
        roomId = targetRoomId;
        isHost = false;
        isConnected = true;
        std::cout << "Joining room: " << roomId << std::endl;
        return true;
    }
    return false;
    #else
    // Native build
    roomId = targetRoomId;
    isHost = false;
    isConnected = true;
    
    shouldStop = false;
    networkThread = std::thread(&NetworkManager::NetworkLoop, this);
    
    std::cout << "Joined room: " << roomId << std::endl;
    return true;
    #endif
}

void NetworkManager::Disconnect() {
    if (isConnected) {
        #ifdef PLATFORM_WEB
        JS_DisconnectPeer();
        #else
        shouldStop = true;
        
        if (networkThread.joinable()) {
            networkThread.join();
        }
        #endif
        
        isConnected = false;
        isHost = false;
        connectedPeers.clear();
        roomId.clear();
        
        // Clear message queues
        std::lock_guard<std::mutex> lock(messageMutex);
        while (!incomingMessages.empty()) incomingMessages.pop();
        while (!outgoingMessages.empty()) outgoingMessages.pop();
    }
}

void NetworkManager::SendPlayerUpdate(const Player& update) {
    if (!isConnected) return;
    
    #ifdef PLATFORM_WEB
    // Build JSON message for JavaScript
    std::ostringstream json;
    json << "{\"type\":\"PLAYER_MOVE\",\"playerId\":" << update.id
         << ",\"x\":" << update.x << ",\"y\":" << update.y
         << ",\"mode\":" << static_cast<int>(update.mode) << ",\"score\":" << update.score
         << ",\"alive\":" << (update.alive ? "true" : "false")
         << ",\"dirX\":" << update.lastDirectionX
         << ",\"dirY\":" << update.lastDirectionY << "}";
    
    JS_BroadcastMessage(json.str().c_str());
    #else
    NetworkMessage msg;
    msg.type = MessageType::PLAYER_MOVE;
    msg.playerId = update.id;
    std::ostringstream oss;
    oss << update.id << "," << update.x << "," << update.y << ","
        << static_cast<int>(update.mode) << "," << update.score << "," << (update.alive ? 1 : 0)
        << "," << update.lastDirectionX << "," << update.lastDirectionY;
    msg.data = oss.str();
    msg.timestamp = std::chrono::duration<float>(std::chrono::steady_clock::now().time_since_epoch()).count();
    
    std::lock_guard<std::mutex> lock(messageMutex);
    outgoingMessages.push(msg);
    #endif
}

void NetworkManager::SendPlayerAction(const ActionMessage& action) {
    if (!isConnected) return;
    
    #ifdef PLATFORM_WEB
    // Build JSON message for JavaScript
    std::ostringstream json;
    json << "{\"type\":\"PLAYER_ACTION\",\"playerId\":" << action.playerId
         << ",\"targetX\":" << action.targetX << ",\"targetY\":" << action.targetY
         << ",\"actionType\":" << action.actionType << "}";
    
    std::cout << "[C++] Sending player action from player " << action.playerId << " type " << action.actionType << std::endl;
    JS_BroadcastMessage(json.str().c_str());
    #else
    NetworkMessage msg;
    msg.type = MessageType::PLAYER_ACTION;
    msg.playerId = action.playerId;
    msg.data = SerializeAction(action);
    msg.timestamp = std::chrono::duration<float>(std::chrono::steady_clock::now().time_since_epoch()).count();
    
    std::lock_guard<std::mutex> lock(messageMutex);
    outgoingMessages.push(msg);
    #endif
}

void NetworkManager::SendPlayerModeChange(int playerId, int newMode) {
    if (!isConnected) return;
    
    #ifdef PLATFORM_WEB
    // Build JSON message for JavaScript
    std::ostringstream json;
    json << "{\"type\":\"PLAYER_MODE_CHANGE\",\"playerId\":" << playerId
         << ",\"mode\":" << newMode << "}";
    
    JS_BroadcastMessage(json.str().c_str());
    #else
    NetworkMessage msg;
    msg.type = MessageType::PLAYER_MODE_CHANGE;
    msg.playerId = playerId;
    msg.data = std::to_string(newMode);
    msg.timestamp = std::chrono::duration<float>(std::chrono::steady_clock::now().time_since_epoch()).count();
    
    std::lock_guard<std::mutex> lock(messageMutex);
    outgoingMessages.push(msg);
    #endif
}

void NetworkManager::SendGameState(const GameState& state) {
    if (!isConnected) return;

#ifdef PLATFORM_WEB
    std::string stateStr = SerializeGameState(state);
    JS_BroadcastMessage(stateStr.c_str());
#else
    // Native implementation would go here
#endif
}

void NetworkManager::AssignPlayerId(int playerId) {
    if (!isConnected || !isHost) return;

#ifdef PLATFORM_WEB
    std::ostringstream json;
    json << "{\"type\":\"ASSIGN_PLAYER_ID\",\"playerId\":" << playerId << "}";
    
    // Find the peerId from the connectedPeers map
    for (const auto& [pId, peerId] : connectedPeers) {
        if (pId == playerId) {
            JS_SendMessageTo(peerId.c_str(), json.str().c_str());
            break;
        }
    }
#endif
}

void NetworkManager::ProcessMessages() {
    std::lock_guard<std::mutex> lock(messageMutex);
    
    while (!incomingMessages.empty()) {
        NetworkMessage msg = incomingMessages.front();
        incomingMessages.pop();
        
        ProcessIncomingMessage(msg);
    }
}

void NetworkManager::ProcessIncomingMessage(const NetworkMessage& msg) {
    switch (msg.type) {
        case MessageType::PLAYER_JOIN:
            if (onPlayerJoin) {
                onPlayerJoin(msg.playerId);
            }
            break;
            
        case MessageType::PLAYER_LEAVE:
            if (onPlayerLeave) {
                onPlayerLeave(msg.playerId);
            }
            break;
            
        case MessageType::PLAYER_MOVE: {
            Player update = {};
            std::istringstream iss(msg.data);
            std::string token;
            
            if (std::getline(iss, token, ',')) update.id = std::stoi(token);
            if (std::getline(iss, token, ',')) update.x = std::stoi(token);
            if (std::getline(iss, token, ',')) update.y = std::stoi(token);
            if (std::getline(iss, token, ',')) update.mode = static_cast<PlayerMode>(std::stoi(token));
            if (std::getline(iss, token, ',')) update.score = std::stoi(token);
            if (std::getline(iss, token, ',')) update.alive = (std::stoi(token) == 1);
            if (std::getline(iss, token, ',')) update.lastDirectionX = std::stoi(token);
            if (std::getline(iss, token, ',')) update.lastDirectionY = std::stoi(token);

            if (onPlayerUpdate) {
                onPlayerUpdate(update);
            }
            break;
        }
        
        case MessageType::PLAYER_ACTION: {
            ActionMessage action = DeserializeAction(msg.data);
            if (onPlayerAction) {
                onPlayerAction(action);
            }
            break;
        }
        
        case MessageType::PLAYER_MODE_CHANGE:
            // Handle mode change
            break;
            
        default:
            break;
    }
}

void NetworkManager::NetworkLoop() {
    while (!shouldStop) {
        // Process outgoing messages
        {
            std::lock_guard<std::mutex> lock(messageMutex);
            while (!outgoingMessages.empty()) {
                NetworkMessage msg = outgoingMessages.front();
                outgoingMessages.pop();
                
                // In a real implementation, this would send the message
                // via WebRTC data channels to all connected peers
                std::cout << "Sending message type " << (int)msg.type 
                         << " from player " << msg.playerId << std::endl;
            }
        }
        
        // Simulate receiving messages (in real implementation, this would
        // be triggered by WebRTC data channel events)
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
}

// WebRTC Connection Implementation (Simplified)
WebRTCConnection::WebRTCConnection() : peerConnection(nullptr) {
}

WebRTCConnection::~WebRTCConnection() {
    // Cleanup WebRTC resources
}

bool WebRTCConnection::Initialize() {
    // Initialize WebRTC peer connection
    // In a real implementation, this would:
    // 1. Create RTCPeerConnection with ICE servers
    // 2. Set up data channels for game communication
    // 3. Configure ICE candidate gathering
    
    try {
        // Simulate WebRTC initialization
        // Real implementation would use libwebrtc or similar
        
        // Configuration for ICE servers (STUN/TURN)
        // RTCConfiguration config;
        // config.servers.push_back(RTCIceServer{"stun:stun.l.google.com:19302"});
        
        // Create peer connection
        // peerConnection = CreatePeerConnection(config);
        
        // Set up data channel for game messages
        // auto dataChannel = peerConnection->CreateDataChannel("game", {});
        // dataChannel->OnMessage([this](const std::string& message) {
        //     if (onMessage) onMessage(message);
        // });
        
        std::cout << "WebRTC connection initialized" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize WebRTC: " << e.what() << std::endl;
        return false;
    }
}

bool WebRTCConnection::CreateOffer(std::string& offer) {
    if (!peerConnection) return false;
    
    // In real implementation:
    // auto offerSdp = peerConnection->CreateOffer();
    // offer = offerSdp->ToString();
    
    // Simulate SDP offer
    offer = "v=0\r\no=- 123456789 2 IN IP4 127.0.0.1\r\n"
           "s=-\r\nt=0 0\r\na=group:BUNDLE 0\r\n"
           "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\n"
           "c=IN IP4 0.0.0.0\r\na=ice-ufrag:simulated\r\n"
           "a=ice-pwd:simulatedpassword\r\na=setup:actpass\r\n"
           "a=mid:0\r\na=sctp-port:5000\r\n";
    
    std::cout << "Created WebRTC offer" << std::endl;
    return true;
}

bool WebRTCConnection::CreateAnswer(const std::string& /*offer*/, std::string& answer) {
    if (!peerConnection) return false;
    
    // In real implementation:
    // auto offerSdp = RTCSessionDescription::FromString(offer);
    // peerConnection->SetRemoteDescription(offerSdp);
    // auto answerSdp = peerConnection->CreateAnswer();
    // answer = answerSdp->ToString();
    
    // Simulate SDP answer
    answer = "v=0\r\not=- 987654321 2 IN IP4 127.0.0.1\r\n"
            "s=-\r\nt=0 0\r\na=group:BUNDLE 0\r\n"
            "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\n"
            "c=IN IP4 0.0.0.0\r\na=ice-ufrag:simulated\r\n"
            "a=ice-pwd:simulatedpassword\r\na=setup:active\r\n"
            "a=mid:0\r\na=sctp-port:5000\r\n";
    
    std::cout << "Created WebRTC answer" << std::endl;
    return true;
}

bool WebRTCConnection::SetRemoteAnswer(const std::string& /*answer*/) {
    if (!peerConnection) return false;
    
    // In real implementation:
    // auto answerSdp = RTCSessionDescription::FromString(answer);
    // peerConnection->SetRemoteDescription(answerSdp);
    
    std::cout << "Set remote answer" << std::endl;
    
    // Simulate connection establishment
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (onConnect) onConnect();
    
    return true;
}

bool WebRTCConnection::SetRemoteOffer(const std::string& /*offer*/) {
    if (!peerConnection) return false;
    
    // In real implementation:
    // auto offerSdp = RTCSessionDescription::FromString(offer);
    // peerConnection->SetRemoteDescription(offerSdp);
    
    std::cout << "Set remote offer" << std::endl;
    return true;
}

void WebRTCConnection::SendMessage(const std::string& message) {
    if (!IsConnected()) return;
    
    // In real implementation:
    // dataChannel->Send(message);
    
    std::cout << "Sending WebRTC message: " << message.substr(0, 50) << "..." << std::endl;
}

bool WebRTCConnection::IsConnected() const {
    // In real implementation:
    // return peerConnection && peerConnection->GetConnectionState() == RTCPeerConnectionState::Connected;
    
    // Simulate connection status
    return peerConnection != nullptr;
}