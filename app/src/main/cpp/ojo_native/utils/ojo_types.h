#ifndef OJO_TYPES_H
#define OJO_TYPES_H

// Header conflict prevention for RockChip MPP on Android
#ifdef __ANDROID__
// Prevent MPP from redefining standard types that conflict with Android NDK
#define HAVE_STRUCT_TIMESPEC 1
#define _TIMESPEC_DEFINED 1
#define PTW32_STATIC_LIB 1
#define __CLEANUP_C 1
// Disable Windows-specific __declspec attributes
#ifdef __declspec
#undef __declspec
#endif
#define __declspec(x)
// Prevent pthread redefinitions
#define PTW32_LEVEL 1
#define PTW32_LEVEL_MAX 3
#endif

// Standard includes
#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>
#include <unordered_map>

namespace ojo {

// Forward declarations
struct VideoFrame;
class RTSPClient;
class MPPDecoder;
class NativeRenderer;

// Error codes
enum class OjoError {
    SUCCESS = 0,
    INVALID_PARAMETER,
    INVALID_STATE,
    INITIALIZATION_FAILED,
    CONNECTION_FAILED,
    DECODE_FAILED,
    RENDER_FAILED,
    OUT_OF_MEMORY,
    TIMEOUT,
    HARDWARE_NOT_AVAILABLE,
    HARDWARE_ERROR,
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
    I420,        // Same as YUV420P
    NV12,
    NV21,
    RGB24,
    BGR24,
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

// Performance statistics (non-atomic for return values)
struct StreamStatistics {
    uint64_t framesReceived{0};
    uint64_t framesDecoded{0};
    uint64_t framesRendered{0};
    uint64_t framesDropped{0};
    uint64_t bytesReceived{0};
    double currentFps{0.0};
    double averageFps{0.0};
    int64_t lastFrameTimestamp{0};
    uint32_t connectionRetries{0};
};

// Atomic stream statistics for internal use
struct AtomicStreamStatistics {
    std::atomic<uint64_t> framesReceived{0};
    std::atomic<uint64_t> framesDecoded{0};
    std::atomic<uint64_t> framesRendered{0};
    std::atomic<uint64_t> framesDropped{0};
    std::atomic<uint64_t> bytesReceived{0};
    std::atomic<double> currentFps{0.0};
    std::atomic<double> averageFps{0.0};
    std::atomic<int64_t> lastFrameTimestamp{0};
    std::atomic<uint32_t> connectionRetries{0};

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

// Decoder statistics (non-atomic for return values)
struct DecoderStatistics {
    uint64_t framesDecoded{0};
    uint64_t framesFailed{0};
    uint64_t bytesProcessed{0};
    double averageDecodeTime{0.0};
    int queuedFrames{0};
    bool hardwareAccelerated{false};
};

// Renderer statistics (non-atomic for return values)
struct RendererStatistics {
    uint64_t framesRendered{0};
    uint64_t framesDropped{0};
    double averageRenderTime{0.0};
    double currentFps{0.0};
};

// Callback function types
using FrameCallback = std::function<void(std::shared_ptr<ojo::VideoFrame>)>;
using StateCallback = std::function<void(ojo::StreamState, const std::string&)>;
using ErrorCallback = std::function<void(ojo::OjoError, const std::string&)>;
using StatisticsCallback = std::function<void(const ojo::StreamStatistics&)>;

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
inline const char* errorToString(ojo::OjoError error) {
    switch (error) {
        case ojo::OjoError::SUCCESS: return "Success";
        case ojo::OjoError::INVALID_PARAMETER: return "Invalid parameter";
        case ojo::OjoError::INVALID_STATE: return "Invalid state";
        case ojo::OjoError::INITIALIZATION_FAILED: return "Initialization failed";
        case ojo::OjoError::CONNECTION_FAILED: return "Connection failed";
        case ojo::OjoError::DECODE_FAILED: return "Decode failed";
        case ojo::OjoError::RENDER_FAILED: return "Render failed";
        case ojo::OjoError::OUT_OF_MEMORY: return "Out of memory";
        case ojo::OjoError::TIMEOUT: return "Timeout";
        case ojo::OjoError::UNKNOWN_ERROR: return "Unknown error";
        default: return "Unknown error";
    }
}

inline const char* stateToString(ojo::StreamState state) {
    switch (state) {
        case ojo::StreamState::IDLE: return "Idle";
        case ojo::StreamState::CONNECTING: return "Connecting";
        case ojo::StreamState::CONNECTED: return "Connected";
        case ojo::StreamState::PLAYING: return "Playing";
        case ojo::StreamState::PAUSED: return "Paused";
        case ojo::StreamState::STOPPED: return "Stopped";
        case ojo::StreamState::ERROR: return "Error";
        default: return "Unknown";
    }
}

// RGA (Rockchip Graphics Accelerator) types
#ifdef HAVE_RGA
struct RGAContext {
    bool initialized;
    bool available;
    int version;
    void* handle;

    RGAContext() : initialized(false), available(false), version(0), handle(nullptr) {}
};

enum class RGAFormat {
    UNKNOWN = 0,
    NV12 = 1,
    NV21 = 2,
    YUV420P = 3,
    RGB888 = 4,
    RGBA8888 = 5,
    BGRA8888 = 6
};

struct RGAOperation {
    int srcWidth;
    int srcHeight;
    RGAFormat srcFormat;
    void* srcBuffer;

    int dstWidth;
    int dstHeight;
    RGAFormat dstFormat;
    void* dstBuffer;

    bool enableScaling;
    bool enableColorConversion;
    bool enableRotation;
    int rotationDegrees;
};
#endif // HAVE_RGA

} // namespace ojo

#endif // OJO_TYPES_H
