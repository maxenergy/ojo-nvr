#ifndef OJO_TYPES_H
#define OJO_TYPES_H

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace ojo {

// Forward declarations
class VideoFrame;
class RTSPClient;
class MPPDecoder;
class NativeRenderer;

// Error codes
enum class OjoError {
    SUCCESS = 0,
    INVALID_PARAMETER,
    INITIALIZATION_FAILED,
    CONNECTION_FAILED,
    DECODE_FAILED,
    RENDER_FAILED,
    OUT_OF_MEMORY,
    TIMEOUT,
    UNKNOWN_ERROR
};

// Stream states
enum class StreamState {
    IDLE = 0,
    CONNECTING,
    CONNECTED,
    PLAYING,
    PAUSED,
    STOPPED,
    ERROR
};

// Video formats
enum class VideoFormat {
    UNKNOWN = 0,
    YUV420P,
    NV12,
    NV21,
    RGB24,
    RGBA32
};

// Codec types
enum class CodecType {
    UNKNOWN = 0,
    H264,
    H265,
    MJPEG
};

// Video frame data structure
struct VideoFrame {
    // Frame data
    std::unique_ptr<uint8_t[]> data;
    size_t dataSize;
    
    // Frame properties
    int width;
    int height;
    int stride;
    VideoFormat format;
    CodecType codec;
    
    // Timing information
    int64_t timestamp;
    int64_t pts;
    int64_t dts;
    
    // Frame metadata
    int frameId;
    bool isKeyFrame;
    
    // Constructor
    VideoFrame() 
        : data(nullptr), dataSize(0), width(0), height(0), stride(0)
        , format(VideoFormat::UNKNOWN), codec(CodecType::UNKNOWN)
        , timestamp(0), pts(0), dts(0), frameId(0), isKeyFrame(false) {}
    
    // Move constructor
    VideoFrame(VideoFrame&& other) noexcept
        : data(std::move(other.data)), dataSize(other.dataSize)
        , width(other.width), height(other.height), stride(other.stride)
        , format(other.format), codec(other.codec)
        , timestamp(other.timestamp), pts(other.pts), dts(other.dts)
        , frameId(other.frameId), isKeyFrame(other.isKeyFrame) {
        other.dataSize = 0;
        other.width = other.height = other.stride = 0;
        other.timestamp = other.pts = other.dts = 0;
        other.frameId = 0;
        other.isKeyFrame = false;
    }
    
    // Move assignment
    VideoFrame& operator=(VideoFrame&& other) noexcept {
        if (this != &other) {
            data = std::move(other.data);
            dataSize = other.dataSize;
            width = other.width;
            height = other.height;
            stride = other.stride;
            format = other.format;
            codec = other.codec;
            timestamp = other.timestamp;
            pts = other.pts;
            dts = other.dts;
            frameId = other.frameId;
            isKeyFrame = other.isKeyFrame;
            
            other.dataSize = 0;
            other.width = other.height = other.stride = 0;
            other.timestamp = other.pts = other.dts = 0;
            other.frameId = 0;
            other.isKeyFrame = false;
        }
        return *this;
    }
    
    // Disable copy constructor and assignment
    VideoFrame(const VideoFrame&) = delete;
    VideoFrame& operator=(const VideoFrame&) = delete;
};

// Stream configuration
struct StreamConfig {
    std::string rtspUrl;
    int streamId;
    int displayX, displayY;
    int displayWidth, displayHeight;
    bool enableHardwareDecoding;
    int connectionTimeoutMs;
    int readTimeoutMs;
    int maxRetryAttempts;
    
    StreamConfig()
        : streamId(0), displayX(0), displayY(0)
        , displayWidth(1280), displayHeight(720)
        , enableHardwareDecoding(true)
        , connectionTimeoutMs(10000), readTimeoutMs(5000)
        , maxRetryAttempts(3) {}
};

// Performance statistics
struct StreamStatistics {
    std::atomic<uint64_t> framesReceived{0};
    std::atomic<uint64_t> framesDecoded{0};
    std::atomic<uint64_t> framesRendered{0};
    std::atomic<uint64_t> framesDropped{0};
    std::atomic<uint64_t> bytesReceived{0};
    std::atomic<double> currentFps{0.0};
    std::atomic<double> averageFps{0.0};
    std::atomic<int64_t> lastFrameTimestamp{0};
    std::atomic<int> connectionRetries{0};
    
    void reset() {
        framesReceived = 0;
        framesDecoded = 0;
        framesRendered = 0;
        framesDropped = 0;
        bytesReceived = 0;
        currentFps = 0.0;
        averageFps = 0.0;
        lastFrameTimestamp = 0;
        connectionRetries = 0;
    }
};

// Callback function types
using FrameCallback = std::function<void(std::shared_ptr<VideoFrame>)>;
using StateCallback = std::function<void(StreamState, const std::string&)>;
using ErrorCallback = std::function<void(OjoError, const std::string&)>;
using StatisticsCallback = std::function<void(const StreamStatistics&)>;

// Thread-safe queue template
template<typename T>
class SafeQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    
public:
    void push(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
        condition_.notify_one();
    }
    
    bool pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        item = queue_.front();
        queue_.pop();
        return true;
    }
    
    bool waitAndPop(T& item, int timeoutMs = -1) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (timeoutMs < 0) {
            condition_.wait(lock, [this] { return !queue_.empty(); });
        } else {
            if (!condition_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                                   [this] { return !queue_.empty(); })) {
                return false;
            }
        }
        item = queue_.front();
        queue_.pop();
        return true;
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<T> empty;
        queue_.swap(empty);
    }
};

// Utility functions
inline const char* errorToString(OjoError error) {
    switch (error) {
        case OjoError::SUCCESS: return "Success";
        case OjoError::INVALID_PARAMETER: return "Invalid parameter";
        case OjoError::INITIALIZATION_FAILED: return "Initialization failed";
        case OjoError::CONNECTION_FAILED: return "Connection failed";
        case OjoError::DECODE_FAILED: return "Decode failed";
        case OjoError::RENDER_FAILED: return "Render failed";
        case OjoError::OUT_OF_MEMORY: return "Out of memory";
        case OjoError::TIMEOUT: return "Timeout";
        default: return "Unknown error";
    }
}

inline const char* stateToString(StreamState state) {
    switch (state) {
        case StreamState::IDLE: return "Idle";
        case StreamState::CONNECTING: return "Connecting";
        case StreamState::CONNECTED: return "Connected";
        case StreamState::PLAYING: return "Playing";
        case StreamState::PAUSED: return "Paused";
        case StreamState::STOPPED: return "Stopped";
        case StreamState::ERROR: return "Error";
        default: return "Unknown";
    }
}

} // namespace ojo

#endif // OJO_TYPES_H
