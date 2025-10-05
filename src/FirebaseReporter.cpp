#include "FirebaseReporter.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstdlib>

#ifdef PLATFORM_WEB
#include <emscripten.h>
#include <emscripten/fetch.h>
#else
#include <curl/curl.h>
#include <string>
#endif

// Callback for writing curl response
#ifdef PLATFORM_WEB
#else
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}
#endif

FirebaseReporter::FirebaseReporter(const std::string& serverId,
                                 const std::string& serverName,
                                 const std::string& firebaseUrl)
    : serverId(serverId), serverName(serverName), firebaseUrl(firebaseUrl), isRunning(false) {
#ifndef PLATFORM_WEB
    lastReportTime = std::chrono::steady_clock::now();
#else
    timeSinceLastReport = 0.0f;
#endif
}

FirebaseReporter::~FirebaseReporter() {
    Stop();
}

void FirebaseReporter::Start() {
    if (isRunning) return;
    
    isRunning = true;
    
#ifdef PLATFORM_WEB
    std::cout << "[Firebase] Reporter started (web mode)" << std::endl;
    std::cout << "[Firebase] Report interval: " << reportInterval << " seconds" << std::endl;
#else
    reporterThread = std::thread(&FirebaseReporter::ReporterLoop, this);
    std::cout << "[Firebase] Reporter started (native mode)" << std::endl;
#endif
}

void FirebaseReporter::Stop() {
    if (!isRunning) return;
    
    isRunning = false;
    
#ifndef PLATFORM_WEB
    if (reporterThread.joinable()) {
        reporterThread.join();
    }
#endif
    
    std::cout << "[Firebase] Reporter stopped" << std::endl;
}

void FirebaseReporter::UpdateGameState(const GameState& gameState) {
#ifndef PLATFORM_WEB
    std::lock_guard<std::mutex> lock(gameStateMutex);
#endif
    lastReportedState = gameState;
}

void FirebaseReporter::UpdateRoomId(const std::string& roomId) {
    this->roomId = roomId;
    std::cout << "[Firebase] Room ID updated: " << roomId << std::endl;
}

void FirebaseReporter::ReportNow() {
#ifndef PLATFORM_WEB
    std::lock_guard<std::mutex> lock(gameStateMutex);
#endif
    std::string jsonData = CreateServerStatusJson(lastReportedState);
    SendServerStatus(jsonData);
#ifndef PLATFORM_WEB
    lastReportTime = std::chrono::steady_clock::now();
#else
    timeSinceLastReport = 0.0f;
#endif
}

#ifdef PLATFORM_WEB
void FirebaseReporter::Update() {
    if (!isRunning) return;
    
    timeSinceLastReport += GetFrameTime();
    
    // Report every minute (60 seconds)
    if (timeSinceLastReport >= reportInterval) {
        std::string jsonData = CreateServerStatusJson(lastReportedState);
        std::cout << "[Firebase] Attempting to report server status (web mode)" << std::endl;
        std::cout << "[Firebase] Time since last report: " << timeSinceLastReport << " seconds" << std::endl;
        
        if (SendServerStatus(jsonData)) {
            timeSinceLastReport = 0.0f;
            std::cout << "[Firebase] Server status reported successfully" << std::endl;
        } else {
            std::cerr << "[Firebase] Failed to report server status" << std::endl;
        }
    }
}
#endif

#ifndef PLATFORM_WEB
void FirebaseReporter::ReporterLoop() {
    while (isRunning) {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastReportTime);
        
        // Report every minute (60 seconds)
        if (elapsed.count() >= 60) {
            std::lock_guard<std::mutex> lock(gameStateMutex);
            std::string jsonData = CreateServerStatusJson(lastReportedState);
            
            if (SendServerStatus(jsonData)) {
                lastReportTime = currentTime;
                std::cout << "[Firebase] Server status reported successfully" << std::endl;
            } else {
                std::cerr << "[Firebase] Failed to report server status" << std::endl;
            }
        }
        
        // Sleep for 5 seconds before checking again
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}
#else
// Web implementation - called from the main game loop
void FirebaseReporter::WebTimerCallback(void* userData) {
    FirebaseReporter* reporter = static_cast<FirebaseReporter*>(userData);
    if (reporter && reporter->isRunning) {
        reporter->timeSinceLastReport += GetFrameTime();
        
        // Report every minute (60 seconds)
        if (reporter->timeSinceLastReport >= reporter->reportInterval) {
            std::string jsonData = reporter->CreateServerStatusJson(reporter->lastReportedState);
            
            if (reporter->SendServerStatus(jsonData)) {
                reporter->timeSinceLastReport = 0.0f;
                std::cout << "[Firebase] Server status reported successfully" << std::endl;
            } else {
                std::cerr << "[Firebase] Failed to report server status" << std::endl;
            }
        }
    }
}
#endif

std::string FirebaseReporter::CreateServerStatusJson(const GameState& gameState) {
    std::ostringstream json;
    
    json << "{\n";
    json << "  \"serverId\": \"" << serverId << "\",\n";
    json << "  \"serverName\": \"" << serverName << "\",\n";
    
    // Include room ID if available
    if (!roomId.empty()) {
        json << "  \"roomId\": \"" << roomId << "\",\n";
    }
    
    json << "  \"players\": [\n";
    
    bool firstPlayer = true;
    for (const auto& [id, player] : gameState.players) {
        if (!firstPlayer) {
            json << ",\n";
        }
        
        json << "    { \"userId\": \"player_" << player.id << "\", \"score\": " << player.score << " }";
        firstPlayer = false;
    }
    
    json << "\n  ],\n";
    json << "  \"status\": \"Online\"\n";
    json << "}";
    
    return json.str();
}

bool FirebaseReporter::SendServerStatus(const std::string& jsonData) {
#ifdef PLATFORM_WEB
    // Web implementation using emscripten_fetch
    std::cout << "[Firebase] Sending data to: " << firebaseUrl << std::endl;
    std::cout << "[Firebase] Data: " << jsonData << std::endl;
    
    char* url = const_cast<char*>(firebaseUrl.c_str());
    char* data = const_cast<char*>(jsonData.c_str());
    
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.requestData = data;
    attr.requestDataSize = strlen(data);
    attr.onsuccess = [](emscripten_fetch_t* fetch) {
        printf("[Firebase] HTTP request succeeded: %s\n", fetch->statusText);
        emscripten_fetch_close(fetch);
    };
    attr.onerror = [](emscripten_fetch_t* fetch) {
        printf("[Firebase] HTTP request failed: %s\n", fetch->statusText);
        emscripten_fetch_close(fetch);
    };
    
    emscripten_fetch(&attr, url);
    return true; // Assume success for web (async)
#else
    // Native implementation using libcurl
    CURL *curl;
    CURLcode res;
    std::string readBuffer;
    bool success = false;
    
    curl = curl_easy_init();
    if (curl) {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        curl_easy_setopt(curl, CURLOPT_URL, firebaseUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            
            if (response_code >= 200 && response_code < 300) {
                success = true;
                std::cout << "[Firebase] Response: " << readBuffer << std::endl;
            } else {
                std::cerr << "[Firebase] HTTP error: " << response_code << std::endl;
            }
        } else {
            std::cerr << "[Firebase] CURL error: " << curl_easy_strerror(res) << std::endl;
        }
        
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    
    return success;
#endif
}