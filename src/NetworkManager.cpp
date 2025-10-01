#include "NetworkManager.h"
#include <iostream>
#include <sstream>
#include <random>
#include <chrono>
#include <algorithm>

#ifdef PLATFORM_WEB
#include <emscripten.h>

// JavaScript bridge functions
extern "C" {
    int JS_InitPeerNetwork();
    int JS_CreateRoom();
    int JS_JoinRoom(const char* roomId);
    void JS_BroadcastMessage(const char* message);
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
    }
    
    EMSCRIPTEN_KEEPALIVE
    void OnPlayerJoined(const char* peerId) {
        std::cout << "[C++] Player joined: " << peerId << std::endl;
    }
    
    EMSCRIPTEN_KEEPALIVE
    void OnPlayerLeft(const char* peerId) {
        std::cout << "[C++] Player left: " << peerId << std::endl;
    }
    
    EMSCRIPTEN_KEEPALIVE
    void OnNetworkMessage(const char* message) {
        std::cout << "[C++] Network message: " << message << std::endl;
    }
    
    // UI button callbacks
    EMSCRIPTEN_KEEPALIVE
    void OnHostGameClicked() {
        std::cout << "[C++] Host game button clicked" << std::endl;
        if (g_networkManager) {
            g_networkManager->CreateRoom("RobbanRoom");
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
std::string SerializePlayerUpdate(const PlayerUpdate& update) {
    std::ostringstream oss;
    oss << update.id << "," << update.x << "," << update.y << "," 
        << update.mode << "," << update.score << "," << (update.alive ? 1 : 0);
    return oss.str();
}

PlayerUpdate DeserializePlayerUpdate(const std::string& data) {
    PlayerUpdate update = {};
    std::istringstream iss(data);
    std::string token;
    
    if (std::getline(iss, token, ',')) update.id = std::stoi(token);
    if (std::getline(iss, token, ',')) update.x = std::stoi(token);
    if (std::getline(iss, token, ',')) update.y = std::stoi(token);
    if (std::getline(iss, token, ',')) update.mode = std::stoi(token);
    if (std::getline(iss, token, ',')) update.score = std::stoi(token);
    if (std::getline(iss, token, ',')) update.alive = (std::stoi(token) == 1);
    
    return update;
}

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

void NetworkManager::SendPlayerUpdate(const PlayerUpdate& update) {
    if (!isConnected) return;
    
    #ifdef PLATFORM_WEB
    // Build JSON message for JavaScript
    std::ostringstream json;
    json << "{\"type\":\"PLAYER_MOVE\",\"playerId\":" << update.id
         << ",\"x\":" << update.x << ",\"y\":" << update.y
         << ",\"mode\":" << update.mode << ",\"score\":" << update.score
         << ",\"alive\":" << (update.alive ? "true" : "false") << "}";
    
    JS_BroadcastMessage(json.str().c_str());
    #else
    NetworkMessage msg;
    msg.type = MessageType::PLAYER_MOVE;
    msg.playerId = update.id;
    msg.data = SerializePlayerUpdate(update);
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
            PlayerUpdate update = DeserializePlayerUpdate(msg.data);
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
    answer = "v=0\r\no=- 987654321 2 IN IP4 127.0.0.1\r\n"
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