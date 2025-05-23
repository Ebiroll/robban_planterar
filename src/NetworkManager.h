#pragma once
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <thread>
#include <mutex>
#include <queue>

// Simple message types for game networking
enum class MessageType {
    PLAYER_JOIN,
    PLAYER_LEAVE,
    PLAYER_MOVE,
    PLAYER_ACTION,
    PLAYER_MODE_CHANGE,
    GAME_STATE_UPDATE,
    ANIMAL_UPDATE,
    TREE_UPDATE
};

struct NetworkMessage {
    MessageType type;
    int playerId;
    std::string data;
    float timestamp;
};

struct PlayerUpdate {
    int id;
    int x, y;
    int mode;
    int score;
    bool alive;
};

struct ActionMessage {
    int playerId;
    int targetX, targetY;
    int actionType; // 0=plant, 1=shoot, 2=chop
};

class NetworkManager {
private:
    bool isHost = false;
    bool isConnected = false;
    std::string roomId;
    std::map<int, std::string> connectedPeers;
    
    std::queue<NetworkMessage> incomingMessages;
    std::queue<NetworkMessage> outgoingMessages;
    std::mutex messageMutex;
    
    std::thread networkThread;
    bool shouldStop = false;
    
    // Callbacks
    std::function<void(int)> onPlayerJoin;
    std::function<void(int)> onPlayerLeave;
    std::function<void(const PlayerUpdate&)> onPlayerUpdate;
    std::function<void(const ActionMessage&)> onPlayerAction;
    
    void NetworkLoop();
    void ProcessIncomingMessage(const NetworkMessage& msg);
    
public:
    NetworkManager();
    ~NetworkManager();
    
    // Connection management
    bool CreateRoom(const std::string& roomName);
    bool JoinRoom(const std::string& roomId);
    void Disconnect();
    
    // Message sending
    void SendPlayerUpdate(const PlayerUpdate& update);
    void SendPlayerAction(const ActionMessage& action);
    void SendPlayerModeChange(int playerId, int newMode);
    
    // Message processing
    void ProcessMessages();
    
    // Callbacks
    void SetPlayerJoinCallback(std::function<void(int)> callback) { onPlayerJoin = callback; }
    void SetPlayerLeaveCallback(std::function<void(int)> callback) { onPlayerLeave = callback; }
    void SetPlayerUpdateCallback(std::function<void(const PlayerUpdate&)> callback) { onPlayerUpdate = callback; }
    void SetPlayerActionCallback(std::function<void(const ActionMessage&)> callback) { onPlayerAction = callback; }
    
    // Status
    bool IsConnected() const { return isConnected; }
    bool IsHost() const { return isHost; }
    std::string GetRoomId() const { return roomId; }
    int GetPlayerCount() const { return connectedPeers.size() + (isConnected ? 1 : 0); }
};

// WebRTC wrapper class - simplified interface
class WebRTCConnection {
private:
    void* peerConnection; // Platform-specific WebRTC implementation
    std::function<void(const std::string&)> onMessage;
    std::function<void()> onConnect;
    std::function<void()> onDisconnect;
    
public:
    WebRTCConnection();
    ~WebRTCConnection();
    
    bool Initialize();
    bool CreateOffer(std::string& offer);
    bool CreateAnswer(const std::string& offer, std::string& answer);
    bool SetRemoteAnswer(const std::string& answer);
    bool SetRemoteOffer(const std::string& offer);
    
    void SendMessage(const std::string& message);
    
    void SetMessageCallback(std::function<void(const std::string&)> callback) { onMessage = callback; }
    void SetConnectCallback(std::function<void()> callback) { onConnect = callback; }
    void SetDisconnectCallback(std::function<void()> callback) { onDisconnect = callback; }
    
    bool IsConnected() const;
};