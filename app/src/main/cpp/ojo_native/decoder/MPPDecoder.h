#ifndef MPP_DECODER_H
#define MPP_DECODER_H

#include "../utils/ojo_types.h"
#include <memory>
#include <thread>
#include <atomic>

// Forward declarations for Rockchip MPP
extern "C" {
    typedef void* MppCtx;
    typedef void* MppApi;
    typedef void* MppFrame;
    typedef void* MppPacket;
    typedef void* MppBuffer;
    typedef void* MppBufferGroup;
    
    typedef enum {
        MPP_VIDEO_CodingUnused,
        MPP_VIDEO_CodingAVC,        // H.264
        MPP_VIDEO_CodingHEVC,       // H.265
        MPP_VIDEO_CodingMJPEG,      // MJPEG
        MPP_VIDEO_CodingVP8,
        MPP_VIDEO_CodingVP9,
        MPP_VIDEO_CodingMax
    } MppCodingType;
    
    typedef enum {
        MPP_RET_SUCCESS = 0,
        MPP_RET_ERROR = -1,
        MPP_NOK = -1,
        MPP_OK = 0
    } MPP_RET;
}

namespace ojo {

/**
 * Rockchip MPP Hardware Decoder for Ojo surveillance system
 * Provides hardware-accelerated video decoding on RK3588 platform
 */
class MPPDecoder {
public:
    MPPDecoder();
    ~MPPDecoder();
    
    // Initialization and cleanup
    OjoError initialize(CodecType codecType);
    void cleanup();
    bool isInitialized() const;
    
    // Decoding operations
    OjoError decode(const uint8_t* data, size_t size, int64_t timestamp);
    std::shared_ptr<VideoFrame> getDecodedFrame();
    bool hasDecodedFrame() const;
    
    // Configuration
    void setOutputFormat(VideoFormat format);
    VideoFormat getOutputFormat() const;
    void setMaxFrameBuffers(int count);
    int getMaxFrameBuffers() const;
    
    // Statistics and monitoring
    struct DecoderStatistics {
        std::atomic<uint64_t> framesDecoded{0};
        std::atomic<uint64_t> framesFailed{0};
        std::atomic<uint64_t> bytesProcessed{0};
        std::atomic<double> averageDecodeTime{0.0};
        std::atomic<int> queuedFrames{0};
        std::atomic<bool> hardwareAccelerated{false};
        
        void reset() {
            framesDecoded = 0;
            framesFailed = 0;
            bytesProcessed = 0;
            averageDecodeTime = 0.0;
            queuedFrames = 0;
            hardwareAccelerated = false;
        }
    };
    
    DecoderStatistics getStatistics() const;
    void resetStatistics();
    
    // Error handling
    using ErrorCallback = std::function<void(OjoError, const std::string&)>;
    void setErrorCallback(ErrorCallback callback);
    
    // Frame callback for decoded frames
    using FrameReadyCallback = std::function<void(std::shared_ptr<VideoFrame>)>;
    void setFrameReadyCallback(FrameReadyCallback callback);
    
    // Control
    void flush();
    void reset();
    bool isHardwareAccelerated() const;
    
private:
    // MPP context and API
    MppCtx mppCtx_;
    MppApi* mppApi_;
    MppBufferGroup frameGroup_;
    MppBufferGroup packetGroup_;
    
    // Configuration
    CodecType codecType_;
    VideoFormat outputFormat_;
    int maxFrameBuffers_;
    bool initialized_;
    
    // Threading and synchronization
    std::thread decoderThread_;
    std::atomic<bool> shouldStop_;
    SafeQueue<std::shared_ptr<VideoFrame>> inputQueue_;
    SafeQueue<std::shared_ptr<VideoFrame>> outputQueue_;
    mutable std::mutex decoderMutex_;
    
    // Statistics
    DecoderStatistics statistics_;
    std::chrono::steady_clock::time_point lastDecodeTime_;
    
    // Callbacks
    ErrorCallback errorCallback_;
    FrameReadyCallback frameReadyCallback_;
    mutable std::mutex callbackMutex_;
    
    // Internal methods
    void decoderLoop();
    OjoError initializeMPP();
    void cleanupMPP();
    MppCodingType codecTypeToMpp(CodecType codec) const;
    VideoFormat mppFormatToVideoFormat(int mppFormat) const;
    int videoFormatToMppFormat(VideoFormat format) const;
    
    // Frame processing
    OjoError processInputPacket(const uint8_t* data, size_t size, int64_t timestamp);
    std::shared_ptr<VideoFrame> processOutputFrame();
    std::shared_ptr<VideoFrame> createVideoFrameFromMpp(MppFrame mppFrame);
    
    // Buffer management
    OjoError allocateFrameBuffers();
    void releaseFrameBuffers();
    MppBuffer allocateInputBuffer(size_t size);
    void releaseInputBuffer(MppBuffer buffer);
    
    // Error handling
    void reportError(OjoError error, const std::string& message);
    const char* mppRetToString(MPP_RET ret) const;
    
    // Utility methods
    void updateStatistics(double decodeTime);
    void logDebug(const std::string& message) const;
    void logError(const std::string& message) const;
    void logInfo(const std::string& message) const;
    
    // Hardware capability detection
    bool checkHardwareSupport(CodecType codec) const;
    std::string getDecoderInfo() const;
};

/**
 * Video Frame Processor - handles frame format conversion and optimization
 */
class VideoFrameProcessor {
public:
    VideoFrameProcessor();
    ~VideoFrameProcessor();
    
    // Format conversion
    OjoError convertFormat(const VideoFrame& input, VideoFrame& output, VideoFormat targetFormat);
    OjoError resizeFrame(const VideoFrame& input, VideoFrame& output, int targetWidth, int targetHeight);
    OjoError cropFrame(const VideoFrame& input, VideoFrame& output, int x, int y, int width, int height);
    
    // RGA hardware acceleration
    OjoError convertWithRGA(const VideoFrame& input, VideoFrame& output, VideoFormat targetFormat);
    OjoError resizeWithRGA(const VideoFrame& input, VideoFrame& output, int targetWidth, int targetHeight);
    
    // Utility
    bool isFormatSupported(VideoFormat format) const;
    size_t calculateFrameSize(int width, int height, VideoFormat format) const;
    
private:
    // RGA context (if available)
    void* rgaCtx_;
    bool rgaAvailable_;
    
    // Internal methods
    bool initializeRGA();
    void cleanupRGA();
    OjoError convertYUV420ToRGB(const VideoFrame& input, VideoFrame& output);
    OjoError convertNV12ToRGB(const VideoFrame& input, VideoFrame& output);
    
    void logDebug(const std::string& message) const;
    void logError(const std::string& message) const;
};

/**
 * Decoder Factory - creates appropriate decoder instances
 */
class DecoderFactory {
public:
    static std::unique_ptr<MPPDecoder> createDecoder(CodecType codecType);
    static bool isCodecSupported(CodecType codecType);
    static std::vector<CodecType> getSupportedCodecs();
    static std::string getDecoderCapabilities();
    
private:
    static bool checkMPPSupport();
    static bool checkCodecSupport(CodecType codecType);
};

} // namespace ojo

#endif // MPP_DECODER_H
