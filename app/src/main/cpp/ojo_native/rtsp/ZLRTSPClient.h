#ifndef ZL_RTSP_CLIENT_H
#define ZL_RTSP_CLIENT_H

#include "../utils/ojo_types.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <map>

// Forward declarations for ZLMediaKit
namespace mediakit {
    class PlayerProxy;
    class MediaSource;
}

namespace ojo {

/**
 * ZLMediaKit-based RTSP client for Ojo surveillance system
 * Provides robust RTSP streaming with automatic reconnection and error handling
 */
class ZLRTSPClient {
public:
    ZLRTSPClient();
    ~ZLRTSPClient();
    
    // Connection management
    OjoError connect(const std::string& rtspUrl);
    void disconnect();
    bool isConnected() const;
    void reconnect();
    
    // Configuration
    void setConfig(const StreamConfig& config);
    StreamConfig getConfig() const;
    
    // Callbacks
    void setFrameCallback(FrameCallback callback);
    void setStateCallback(StateCallback callback);
    void setErrorCallback(ErrorCallback callback);
    void setStatisticsCallback(StatisticsCallback callback);
    
    // Statistics
    StreamStatistics getStatistics() const;
    StreamState getState() const;
    
    // Control
    void start();
    void stop();
    void pause();
    void resume();
    
private:
    // Internal state
    std::atomic<StreamState> state_;
    StreamConfig config_;
    mutable std::mutex configMutex_;
    
    // ZLMediaKit components
    std::unique_ptr<mediakit::PlayerProxy> player_;
    std::shared_ptr<mediakit::MediaSource> mediaSource_;
    
    // Threading
    std::thread workerThread_;
    std::thread reconnectThread_;
    std::atomic<bool> shouldStop_;
    std::atomic<bool> shouldReconnect_;
    
    // Statistics and monitoring
    StreamStatistics statistics_;
    std::chrono::steady_clock::time_point lastFrameTime_;
    std::chrono::steady_clock::time_point connectionStartTime_;
    
    // Callbacks
    FrameCallback frameCallback_;
    StateCallback stateCallback_;
    ErrorCallback errorCallback_;
    StatisticsCallback statisticsCallback_;
    mutable std::mutex callbackMutex_;
    
    // Frame processing
    SafeQueue<std::shared_ptr<VideoFrame>> frameQueue_;
    std::thread frameProcessorThread_;
    
    // Internal methods
    void workerLoop();
    void reconnectLoop();
    void frameProcessorLoop();
    void initializePlayer();
    void cleanupPlayer();
    void setState(StreamState newState, const std::string& message = "");
    void reportError(OjoError error, const std::string& message);
    void updateStatistics();
    void processReceivedData(const uint8_t* data, size_t size, int64_t timestamp);
    std::shared_ptr<VideoFrame> createVideoFrame(const uint8_t* data, size_t size, int64_t timestamp);
    
    // ZLMediaKit callbacks
    void onPlayerPlay();
    void onPlayerPause();
    void onPlayerTeardown();
    void onPlayerData(const uint8_t* data, size_t size, int64_t timestamp);
    void onPlayerError(const std::string& error);
    
    // Connection management
    bool attemptConnection();
    void scheduleReconnect();
    bool isReconnectNeeded() const;
    
    // Utility methods
    CodecType detectCodecType(const uint8_t* data, size_t size);
    bool isValidRTSPUrl(const std::string& url) const;
    void logDebug(const std::string& message) const;
    void logError(const std::string& message) const;
    void logInfo(const std::string& message) const;
};

/**
 * RTSP Stream Manager - manages multiple RTSP clients
 */
class RTSPStreamManager {
public:
    RTSPStreamManager();
    ~RTSPStreamManager();
    
    // Stream management
    OjoError addStream(int streamId, const StreamConfig& config);
    OjoError removeStream(int streamId);
    OjoError startStream(int streamId);
    OjoError stopStream(int streamId);
    OjoError pauseStream(int streamId);
    OjoError resumeStream(int streamId);
    
    // Configuration
    OjoError updateStreamConfig(int streamId, const StreamConfig& config);
    StreamConfig getStreamConfig(int streamId) const;
    
    // Status and statistics
    StreamState getStreamState(int streamId) const;
    StreamStatistics getStreamStatistics(int streamId) const;
    std::vector<int> getActiveStreamIds() const;
    
    // Global callbacks
    void setGlobalFrameCallback(std::function<void(int, std::shared_ptr<VideoFrame>)> callback);
    void setGlobalStateCallback(std::function<void(int, StreamState, const std::string&)> callback);
    void setGlobalErrorCallback(std::function<void(int, OjoError, const std::string&)> callback);
    
    // Utility
    void startAll();
    void stopAll();
    void pauseAll();
    void resumeAll();
    size_t getStreamCount() const;
    
private:
    // Stream storage
    std::map<int, std::unique_ptr<ZLRTSPClient>> streams_;
    mutable std::mutex streamsMutex_;
    
    // Global callbacks
    std::function<void(int, std::shared_ptr<VideoFrame>)> globalFrameCallback_;
    std::function<void(int, StreamState, const std::string&)> globalStateCallback_;
    std::function<void(int, OjoError, const std::string&)> globalErrorCallback_;
    mutable std::mutex callbackMutex_;
    
    // Internal methods
    ZLRTSPClient* getStream(int streamId) const;
    void setupStreamCallbacks(int streamId, ZLRTSPClient* client);
    void onStreamFrame(int streamId, std::shared_ptr<VideoFrame> frame);
    void onStreamState(int streamId, StreamState state, const std::string& message);
    void onStreamError(int streamId, OjoError error, const std::string& message);
    
    // Utility
    void logDebug(const std::string& message) const;
    void logError(const std::string& message) const;
    void logInfo(const std::string& message) const;
};

} // namespace ojo

#endif // ZL_RTSP_CLIENT_H
