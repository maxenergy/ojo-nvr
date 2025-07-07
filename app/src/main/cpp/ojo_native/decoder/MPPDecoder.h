#ifndef MPP_DECODER_H
#define MPP_DECODER_H

#include "../utils/ojo_types.h"
#include "../utils/mpp_wrapper.h"
#include <memory>
#include <thread>
#include <atomic>

// Rockchip MPP includes with conflict prevention
#ifdef HAVE_ROCKCHIP_MPP
// MPP types are now defined through mpp_wrapper.h
// No direct header includes to avoid conflicts
#endif

namespace ojo {

// MPP decoder frame callback type
typedef void (*MppDecoderFrameCallback)(void* userdata, int width_stride, int height_stride,
                                       int width, int height, int format, int fd, void* data);

// MPP decoder loop data structure for real hardware decoding
typedef struct {
    MppCtx          ctx;
    MppApi          *mpi;
    uint32_t        eos;
    char            *buf;
    MppBufferGroup  frm_grp;
    MppBufferGroup  pkt_grp;
    MppPacket       packet;
    size_t          packet_size;
    MppFrame        frame;
    int32_t         frame_count;
    int32_t         frame_num;
    size_t          max_usage;
} MpiDecLoopData;

/**
 * Rockchip MPP Hardware Decoder for Ojo surveillance system
 * Provides hardware-accelerated video decoding on RK3588 platform
 * Based on reference implementation from yolov5rtspthreadpool
 */
class MPPDecoder {
public:
    MPPDecoder();
    ~MPPDecoder();
    
    // Initialization and cleanup
    OjoError initialize(CodecType codecType);
    void stop();
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
    
    ::ojo::DecoderStatistics getStatistics() const;
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
    // MPP context and API - Real hardware decoder
    MppCtx mppCtx_;
    MppApi* mppApi_;
    MppBufferGroup frameGroup_;
    MppBufferGroup packetGroup_;

    // Real MPP decoder data structures
    MpiDecLoopData loopData_;
    MppPacket packet_;  // MPP packet handle (pointer type)
    MppFrame frame_;    // MPP frame handle (pointer type)
    MppCodingType mppType_;
    uint32_t needSplit_;
    size_t packetSize_;

    // Frame callback and timing
    MppDecoderFrameCallback frameCallback_;
    void* userdata_;
    int fps_;
    unsigned long lastFrameTimeMs_;
    
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
    
    // Internal methods - Real MPP hardware implementation
    void decoderLoop();
    OjoError initializeMPP();
    void cleanupMPP();
    MppCodingType codecTypeToMpp(CodecType codec) const;
    VideoFormat mppFormatToVideoFormat(int mppFormat) const;
    int videoFormatToMppFormat(VideoFormat format) const;

    // Real MPP decoder methods (based on reference implementation)
    int initMppDecoder(int videoType, int fps, void* userdata);
    int setMppCallback(MppDecoderFrameCallback callback);
    int decodeMppPacket(uint8_t* pktData, int pktSize, int pktEos);
    int resetMppDecoder();
    static unsigned long getCurrentTimeMS();
    
    // Frame processing - Real hardware decoding
    OjoError processInputPacket(const uint8_t* data, size_t size, int64_t timestamp);
    std::shared_ptr<VideoFrame> processOutputFrame();
    std::shared_ptr<VideoFrame> createVideoFrameFromMpp(MppFrame mppFrame);
    size_t calculateFrameSize(int width, int height, VideoFormat format) const;
    
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
    void generateTestPattern(VideoFrame* frame);
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
