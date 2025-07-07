#ifndef ZL_RTSP_CLIENT_H
#define ZL_RTSP_CLIENT_H

#include "../utils/ojo_types.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <map>
#include <set>
#include <vector>

// ZLMediaKit C API includes
#ifdef HAVE_ZLMEDIAKIT
extern "C" {
    #include "mk_common.h"
    #include "mk_player.h"
    #include "mk_frame.h"
    #include "mk_track.h"
}
#else
// Forward declarations when ZLMediaKit is not available
typedef void* mk_player;
typedef void* mk_frame;
typedef void* mk_track;
#endif

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

    // Network components
    int rtspSocket_;
    int rtpSocket_;
    int rtcpSocket_;
    std::string rtspHost_;
    int rtspPort_;
    std::string rtspPath_;
    std::string rtspUsername_;
    std::string rtspPassword_;

    // RTSP protocol state
    int cseq_;
    std::string sessionId_;
    std::string lastResponse_;
    int rtpPort_;

    // Media information
    bool hasVideo_;
    int videoPayloadType_;
    CodecType videoCodec_;
    std::string controlUrl_;

    // Threading
    std::thread workerThread_;
    std::thread rtpReceiverThread_;
    std::atomic<bool> shouldStop_;
    std::atomic<bool> isConnected_;

    // Statistics and monitoring
    AtomicStreamStatistics statistics_;

    // Callbacks
    FrameCallback frameCallback_;
    StateCallback stateCallback_;
    ErrorCallback errorCallback_;
    StatisticsCallback statisticsCallback_;
    mutable std::mutex callbackMutex_;
    
    // Internal methods
    void workerLoop();
    void rtpReceiverLoop();
    void setState(StreamState newState, const std::string& message = "");
    void reportError(OjoError error, const std::string& message);
    void updateStatistics();
    std::shared_ptr<VideoFrame> createVideoFrame(const uint8_t* data, size_t size, uint32_t timestamp, bool isMarker);

    // RTSP protocol methods
    bool parseRTSPUrl(const std::string& url);
    bool connectToServer();
    bool performRTSPHandshake();
    bool sendOptions();
    bool sendDescribe();
    bool sendSetup();
    bool sendPlay();
    bool sendPause();
    bool sendTeardown();
    bool sendKeepAlive();
    bool sendRTSPRequest(const std::string& request);

    // RTP processing
    bool createRTPSockets();
    void processRTPPacket(const uint8_t* data, size_t size);
    void processRTPPayload(const uint8_t* payload, size_t payloadSize,
                          uint32_t timestamp, uint16_t sequenceNumber,
                          bool marker, uint32_t ssrc);

    // SDP parsing
    bool parseSDP(const std::string& response);
    void parseRTPMap(const std::string& line);
    bool parseSessionId(const std::string& response);

    // Frame assembly for RTP packets
    struct FrameAssembly {
        uint32_t timestamp;
        std::vector<uint8_t> data;
        std::set<uint16_t> receivedSequences;
        uint16_t expectedSequences;
        bool isComplete;
        std::chrono::steady_clock::time_point lastUpdate;

        FrameAssembly() : timestamp(0), expectedSequences(0), isComplete(false) {}
    };
    std::map<uint32_t, FrameAssembly> frameAssemblyMap_;
    mutable std::mutex frameAssemblyMutex_;

    // Utility methods
    bool isValidRTSPUrl(const std::string& url) const;
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
