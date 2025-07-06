#include "ZLRTSPClient.h"
#include <android/log.h>
#include <regex>
#include <sstream>

// ZLMediaKit includes (will be added when integrating the library)
// #include "Player/PlayerProxy.h"
// #include "Common/MediaSource.h"

#define LOG_TAG "OjoRTSPClient"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace ojo {

ZLRTSPClient::ZLRTSPClient()
    : state_(StreamState::IDLE)
    , shouldStop_(false)
    , shouldReconnect_(false)
    , lastFrameTime_(std::chrono::steady_clock::now())
    , connectionStartTime_(std::chrono::steady_clock::now()) {
    
    LOGD("ZLRTSPClient created");
    
    // Initialize default configuration
    config_.connectionTimeoutMs = 10000;
    config_.readTimeoutMs = 5000;
    config_.maxRetryAttempts = 3;
    config_.enableHardwareDecoding = true;
    
    // Reset statistics
    statistics_.reset();
}

ZLRTSPClient::~ZLRTSPClient() {
    LOGD("ZLRTSPClient destructor called");
    stop();
    disconnect();
}

OjoError ZLRTSPClient::connect(const std::string& rtspUrl) {
    LOGI("Connecting to RTSP URL: %s", rtspUrl.c_str());
    
    if (!isValidRTSPUrl(rtspUrl)) {
        LOGE("Invalid RTSP URL: %s", rtspUrl.c_str());
        return OjoError::INVALID_PARAMETER;
    }
    
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.rtspUrl = rtspUrl;
    
    setState(StreamState::CONNECTING, "Attempting to connect to RTSP stream");
    
    // Initialize ZLMediaKit player (placeholder implementation)
    if (!initializePlayer()) {
        setState(StreamState::ERROR, "Failed to initialize RTSP player");
        return OjoError::INITIALIZATION_FAILED;
    }
    
    // Start connection attempt
    if (!attemptConnection()) {
        setState(StreamState::ERROR, "Failed to connect to RTSP stream");
        return OjoError::CONNECTION_FAILED;
    }
    
    setState(StreamState::CONNECTED, "Successfully connected to RTSP stream");
    return OjoError::SUCCESS;
}

void ZLRTSPClient::disconnect() {
    LOGI("Disconnecting RTSP client");
    
    shouldStop_ = true;
    shouldReconnect_ = false;
    
    // Stop worker threads
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    
    if (reconnectThread_.joinable()) {
        reconnectThread_.join();
    }
    
    if (frameProcessorThread_.joinable()) {
        frameProcessorThread_.join();
    }
    
    // Cleanup player
    cleanupPlayer();
    
    setState(StreamState::IDLE, "Disconnected from RTSP stream");
}

bool ZLRTSPClient::isConnected() const {
    StreamState currentState = state_.load();
    return currentState == StreamState::CONNECTED || currentState == StreamState::PLAYING;
}

void ZLRTSPClient::start() {
    LOGI("Starting RTSP client");
    
    if (state_ == StreamState::IDLE) {
        LOGW("Cannot start - not connected. Call connect() first.");
        return;
    }
    
    shouldStop_ = false;
    
    // Start worker thread
    if (!workerThread_.joinable()) {
        workerThread_ = std::thread(&ZLRTSPClient::workerLoop, this);
    }
    
    // Start frame processor thread
    if (!frameProcessorThread_.joinable()) {
        frameProcessorThread_ = std::thread(&ZLRTSPClient::frameProcessorLoop, this);
    }
    
    // Start reconnect thread
    if (!reconnectThread_.joinable()) {
        reconnectThread_ = std::thread(&ZLRTSPClient::reconnectLoop, this);
    }
    
    setState(StreamState::PLAYING, "RTSP stream started");
}

void ZLRTSPClient::stop() {
    LOGI("Stopping RTSP client");
    
    shouldStop_ = true;
    
    setState(StreamState::STOPPED, "RTSP stream stopped");
}

void ZLRTSPClient::setFrameCallback(FrameCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    frameCallback_ = callback;
}

void ZLRTSPClient::setStateCallback(StateCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    stateCallback_ = callback;
}

void ZLRTSPClient::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    errorCallback_ = callback;
}

StreamState ZLRTSPClient::getState() const {
    return state_.load();
}

StreamStatistics ZLRTSPClient::getStatistics() const {
    return statistics_;
}

// Private methods implementation

void ZLRTSPClient::workerLoop() {
    LOGD("Worker loop started");
    
    while (!shouldStop_) {
        try {
            // Update statistics
            updateStatistics();
            
            // Check connection health
            if (isConnected()) {
                // Process any pending data
                // This would interface with ZLMediaKit's data callbacks
            }
            
            // Sleep for a short interval
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
        } catch (const std::exception& e) {
            LOGE("Exception in worker loop: %s", e.what());
            reportError(OjoError::UNKNOWN_ERROR, e.what());
        }
    }
    
    LOGD("Worker loop ended");
}

void ZLRTSPClient::frameProcessorLoop() {
    LOGD("Frame processor loop started");
    
    while (!shouldStop_) {
        std::shared_ptr<VideoFrame> frame;
        
        // Wait for frame with timeout
        if (frameQueue_.waitAndPop(frame, 100)) {
            try {
                // Update statistics
                statistics_.framesDecoded++;
                
                // Call frame callback if set
                std::lock_guard<std::mutex> lock(callbackMutex_);
                if (frameCallback_) {
                    frameCallback_(frame);
                }
                
            } catch (const std::exception& e) {
                LOGE("Exception in frame processor: %s", e.what());
                statistics_.framesDropped++;
            }
        }
    }
    
    LOGD("Frame processor loop ended");
}

void ZLRTSPClient::reconnectLoop() {
    LOGD("Reconnect loop started");
    
    while (!shouldStop_) {
        if (shouldReconnect_ && isReconnectNeeded()) {
            LOGI("Attempting automatic reconnection");
            
            // Attempt reconnection
            if (attemptConnection()) {
                shouldReconnect_ = false;
                setState(StreamState::CONNECTED, "Reconnected to RTSP stream");
                statistics_.connectionRetries++;
            } else {
                // Wait before next attempt
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    
    LOGD("Reconnect loop ended");
}

bool ZLRTSPClient::initializePlayer() {
    LOGD("Initializing ZLMediaKit player");
    
    try {
        // TODO: Initialize ZLMediaKit PlayerProxy
        // player_ = std::make_unique<mediakit::PlayerProxy>("ojo_rtsp", true);
        
        // Set up callbacks
        // player_->setOnPlay([this]() { onPlayerPlay(); });
        // player_->setOnPause([this]() { onPlayerPause(); });
        // player_->setOnTeardown([this]() { onPlayerTeardown(); });
        
        LOGD("ZLMediaKit player initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        LOGE("Failed to initialize player: %s", e.what());
        return false;
    }
}

void ZLRTSPClient::cleanupPlayer() {
    LOGD("Cleaning up ZLMediaKit player");
    
    try {
        if (player_) {
            // TODO: Cleanup ZLMediaKit player
            // player_->teardown();
            player_.reset();
        }
        
        mediaSource_.reset();
        
    } catch (const std::exception& e) {
        LOGE("Exception during player cleanup: %s", e.what());
    }
}

bool ZLRTSPClient::attemptConnection() {
    LOGD("Attempting RTSP connection");
    
    try {
        connectionStartTime_ = std::chrono::steady_clock::now();
        
        // TODO: Start ZLMediaKit player
        // if (player_) {
        //     player_->play(config_.rtspUrl);
        //     return true;
        // }
        
        // Placeholder: simulate successful connection
        return true;
        
    } catch (const std::exception& e) {
        LOGE("Connection attempt failed: %s", e.what());
        return false;
    }
}

void ZLRTSPClient::setState(StreamState newState, const std::string& message) {
    StreamState oldState = state_.exchange(newState);
    
    if (oldState != newState) {
        LOGI("State changed: %s -> %s (%s)", 
             stateToString(oldState), stateToString(newState), message.c_str());
        
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (stateCallback_) {
            stateCallback_(newState, message);
        }
    }
}

void ZLRTSPClient::reportError(OjoError error, const std::string& message) {
    LOGE("Error reported: %s - %s", errorToString(error), message.c_str());
    
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (errorCallback_) {
        errorCallback_(error, message);
    }
}

bool ZLRTSPClient::isValidRTSPUrl(const std::string& url) const {
    // Simple RTSP URL validation
    std::regex rtspRegex(R"(^rtsp://[a-zA-Z0-9\-\.]+:[0-9]+/.*)");
    return std::regex_match(url, rtspRegex);
}

void ZLRTSPClient::updateStatistics() {
    auto now = std::chrono::steady_clock::now();
    auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameTime_);
    
    if (timeDiff.count() > 0) {
        double fps = 1000.0 / timeDiff.count();
        statistics_.currentFps = fps;
        
        // Update average FPS (simple moving average)
        double currentAvg = statistics_.averageFps.load();
        statistics_.averageFps = (currentAvg * 0.9) + (fps * 0.1);
    }
    
    lastFrameTime_ = now;
}

} // namespace ojo
