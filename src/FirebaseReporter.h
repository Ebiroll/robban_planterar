#pragma once

#include "GameState.h"
#include <string>

#ifdef PLATFORM_WEB
// Web implementation doesn't use threads
#include <emscripten.h>
#else
// Native implementation uses threads
#include <thread>
#include <chrono>
#include <mutex>
#endif

class FirebaseReporter {
private:
    std::string serverId;
    std::string serverName;
    std::string firebaseUrl;
    bool isRunning;
    
#ifdef PLATFORM_WEB
    // Web implementation uses timers instead of threads
    static void WebTimerCallback(void* userData);
    float reportInterval = 60.0f; // seconds
    float timeSinceLastReport = 0.0f;
#else
    // Native implementation uses threads
    std::thread reporterThread;
    std::mutex gameStateMutex;
    std::chrono::steady_clock::time_point lastReportTime;
    void ReporterLoop();
#endif

    GameState lastReportedState;
    std::string CreateServerStatusJson(const GameState& gameState);
    bool SendServerStatus(const std::string& jsonData);
    
public:
    FirebaseReporter(const std::string& serverId = "forest-server-001",
                    const std::string& serverName = "Robban's Scored Lobby",
                    const std::string& firebaseUrl = "https://studio--studio-4023979787-cd3b9.us-central1.hosted.app/");
    
    ~FirebaseReporter();
    
    void Start();
    void Stop();
    void UpdateGameState(const GameState& gameState);
    void ReportNow();
    bool IsRunning() const { return isRunning; }
    
#ifdef PLATFORM_WEB
    // Web implementation requires manual update from main loop
    void Update();
#endif
};