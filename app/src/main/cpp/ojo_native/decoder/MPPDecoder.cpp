#include "MPPDecoder.h"
#include <android/log.h>
#include <chrono>

// Rockchip MPP includes (will be added when integrating the library)
// #include "rockchip/rk_mpi.h"
// #include "rockchip/mpp_frame.h"
// #include "rockchip/mpp_packet.h"
// #include "rockchip/mpp_buffer.h"

#define LOG_TAG "OjoMPPDecoder"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace ojo {

MPPDecoder::MPPDecoder()
    : mppCtx_(nullptr)
    , mppApi_(nullptr)
    , frameGroup_(nullptr)
    , packetGroup_(nullptr)
    , codecType_(CodecType::UNKNOWN)
    , outputFormat_(VideoFormat::NV12)
    , maxFrameBuffers_(8)
    , initialized_(false)
    , shouldStop_(false)
    , lastDecodeTime_(std::chrono::steady_clock::now()) {
    
    LOGD("MPPDecoder created");
    statistics_.reset();
}

MPPDecoder::~MPPDecoder() {
    LOGD("MPPDecoder destructor called");
    cleanup();
}

OjoError MPPDecoder::initialize(CodecType codecType) {
    LOGI("Initializing MPP decoder for codec: %d", static_cast<int>(codecType));
    
    if (initialized_) {
        LOGW("Decoder already initialized");
        return OjoError::SUCCESS;
    }
    
    codecType_ = codecType;
    
    // Check hardware support
    if (!checkHardwareSupport(codecType)) {
        LOGE("Hardware does not support codec type: %d", static_cast<int>(codecType));
        return OjoError::INITIALIZATION_FAILED;
    }
    
    // Initialize MPP
    OjoError result = initializeMPP();
    if (result != OjoError::SUCCESS) {
        LOGE("Failed to initialize MPP");
        return result;
    }
    
    // Allocate frame buffers
    result = allocateFrameBuffers();
    if (result != OjoError::SUCCESS) {
        LOGE("Failed to allocate frame buffers");
        cleanupMPP();
        return result;
    }
    
    // Start decoder thread
    shouldStop_ = false;
    decoderThread_ = std::thread(&MPPDecoder::decoderLoop, this);
    
    initialized_ = true;
    statistics_.hardwareAccelerated = true;
    
    LOGI("MPP decoder initialized successfully");
    return OjoError::SUCCESS;
}

void MPPDecoder::cleanup() {
    LOGI("Cleaning up MPP decoder");
    
    if (!initialized_) {
        return;
    }
    
    // Stop decoder thread
    shouldStop_ = true;
    if (decoderThread_.joinable()) {
        decoderThread_.join();
    }
    
    // Clear queues
    inputQueue_.clear();
    outputQueue_.clear();
    
    // Release frame buffers
    releaseFrameBuffers();
    
    // Cleanup MPP
    cleanupMPP();
    
    initialized_ = false;
    
    LOGI("MPP decoder cleanup completed");
}

OjoError MPPDecoder::decode(const uint8_t* data, size_t size, int64_t timestamp) {
    if (!initialized_) {
        LOGE("Decoder not initialized");
        return OjoError::INITIALIZATION_FAILED;
    }
    
    if (!data || size == 0) {
        LOGE("Invalid input data");
        return OjoError::INVALID_PARAMETER;
    }
    
    // Create video frame for input data
    auto frame = std::make_shared<VideoFrame>();
    frame->data = std::make_unique<uint8_t[]>(size);
    memcpy(frame->data.get(), data, size);
    frame->dataSize = size;
    frame->timestamp = timestamp;
    frame->codec = codecType_;
    
    // Add to input queue
    inputQueue_.push(frame);
    statistics_.queuedFrames = inputQueue_.size();
    
    return OjoError::SUCCESS;
}

std::shared_ptr<VideoFrame> MPPDecoder::getDecodedFrame() {
    std::shared_ptr<VideoFrame> frame;
    if (outputQueue_.pop(frame)) {
        statistics_.queuedFrames = inputQueue_.size();
        return frame;
    }
    return nullptr;
}

bool MPPDecoder::hasDecodedFrame() const {
    return !outputQueue_.empty();
}

MPPDecoder::DecoderStatistics MPPDecoder::getStatistics() const {
    return statistics_;
}

void MPPDecoder::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    errorCallback_ = callback;
}

void MPPDecoder::setFrameReadyCallback(FrameReadyCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    frameReadyCallback_ = callback;
}

// Private methods implementation

void MPPDecoder::decoderLoop() {
    LOGD("Decoder loop started");
    
    while (!shouldStop_) {
        try {
            std::shared_ptr<VideoFrame> inputFrame;
            
            // Wait for input frame with timeout
            if (inputQueue_.waitAndPop(inputFrame, 100)) {
                auto startTime = std::chrono::steady_clock::now();
                
                // Process input packet
                OjoError result = processInputPacket(
                    inputFrame->data.get(), 
                    inputFrame->dataSize, 
                    inputFrame->timestamp
                );
                
                if (result == OjoError::SUCCESS) {
                    // Try to get decoded frame
                    auto outputFrame = processOutputFrame();
                    if (outputFrame) {
                        // Add to output queue
                        outputQueue_.push(outputFrame);
                        
                        // Call frame ready callback
                        std::lock_guard<std::mutex> lock(callbackMutex_);
                        if (frameReadyCallback_) {
                            frameReadyCallback_(outputFrame);
                        }
                        
                        // Update statistics
                        auto endTime = std::chrono::steady_clock::now();
                        auto decodeTime = std::chrono::duration_cast<std::chrono::microseconds>(
                            endTime - startTime).count() / 1000.0;
                        updateStatistics(decodeTime);
                        
                        statistics_.framesDecoded++;
                    }
                } else {
                    statistics_.framesFailed++;
                    reportError(result, "Failed to process input packet");
                }
                
                statistics_.bytesProcessed += inputFrame->dataSize;
            }
            
        } catch (const std::exception& e) {
            LOGE("Exception in decoder loop: %s", e.what());
            statistics_.framesFailed++;
            reportError(OjoError::DECODE_FAILED, e.what());
        }
    }
    
    LOGD("Decoder loop ended");
}

OjoError MPPDecoder::initializeMPP() {
    LOGD("Initializing MPP context");
    
    try {
        // TODO: Initialize MPP context and API
        // MPP_RET ret = mpp_create(&mppCtx_, &mppApi_);
        // if (ret != MPP_OK) {
        //     LOGE("Failed to create MPP context: %s", mppRetToString(ret));
        //     return OjoError::INITIALIZATION_FAILED;
        // }
        
        // Configure decoder
        MppCodingType mppCodec = codecTypeToMpp(codecType_);
        // ret = mpp_init(mppCtx_, MPP_CTX_DEC, mppCodec);
        // if (ret != MPP_OK) {
        //     LOGE("Failed to initialize MPP decoder: %s", mppRetToString(ret));
        //     mpp_destroy(mppCtx_);
        //     return OjoError::INITIALIZATION_FAILED;
        // }
        
        LOGD("MPP context initialized successfully");
        return OjoError::SUCCESS;
        
    } catch (const std::exception& e) {
        LOGE("Exception during MPP initialization: %s", e.what());
        return OjoError::INITIALIZATION_FAILED;
    }
}

void MPPDecoder::cleanupMPP() {
    LOGD("Cleaning up MPP context");
    
    try {
        if (mppCtx_) {
            // TODO: Cleanup MPP context
            // mpp_destroy(mppCtx_);
            mppCtx_ = nullptr;
        }
        
        mppApi_ = nullptr;
        
    } catch (const std::exception& e) {
        LOGE("Exception during MPP cleanup: %s", e.what());
    }
}

MppCodingType MPPDecoder::codecTypeToMpp(CodecType codec) const {
    switch (codec) {
        case CodecType::H264:
            return MPP_VIDEO_CodingAVC;
        case CodecType::H265:
            return MPP_VIDEO_CodingHEVC;
        case CodecType::MJPEG:
            return MPP_VIDEO_CodingMJPEG;
        default:
            return MPP_VIDEO_CodingUnused;
    }
}

OjoError MPPDecoder::processInputPacket(const uint8_t* data, size_t size, int64_t timestamp) {
    // TODO: Implement MPP packet processing
    // This would involve:
    // 1. Creating MppPacket from input data
    // 2. Setting packet properties (timestamp, size, etc.)
    // 3. Sending packet to MPP decoder
    // 4. Handling any errors
    
    LOGD("Processing input packet: size=%zu, timestamp=%lld", size, timestamp);
    
    // Placeholder implementation
    return OjoError::SUCCESS;
}

std::shared_ptr<VideoFrame> MPPDecoder::processOutputFrame() {
    // TODO: Implement MPP frame retrieval
    // This would involve:
    // 1. Getting decoded frame from MPP
    // 2. Converting MPP frame to VideoFrame
    // 3. Handling format conversion if needed
    // 4. Managing frame lifecycle
    
    LOGD("Processing output frame");
    
    // Placeholder: create dummy frame
    auto frame = std::make_shared<VideoFrame>();
    frame->width = 1280;
    frame->height = 720;
    frame->format = outputFormat_;
    frame->timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    return frame;
}

OjoError MPPDecoder::allocateFrameBuffers() {
    LOGD("Allocating frame buffers: count=%d", maxFrameBuffers_);
    
    try {
        // TODO: Allocate MPP frame buffers
        // This would involve:
        // 1. Creating buffer group
        // 2. Allocating frame buffers
        // 3. Setting up buffer pool
        
        return OjoError::SUCCESS;
        
    } catch (const std::exception& e) {
        LOGE("Exception during buffer allocation: %s", e.what());
        return OjoError::OUT_OF_MEMORY;
    }
}

void MPPDecoder::releaseFrameBuffers() {
    LOGD("Releasing frame buffers");
    
    try {
        // TODO: Release MPP frame buffers
        if (frameGroup_) {
            // mpp_buffer_group_put(frameGroup_);
            frameGroup_ = nullptr;
        }
        
        if (packetGroup_) {
            // mpp_buffer_group_put(packetGroup_);
            packetGroup_ = nullptr;
        }
        
    } catch (const std::exception& e) {
        LOGE("Exception during buffer release: %s", e.what());
    }
}

bool MPPDecoder::checkHardwareSupport(CodecType codec) const {
    // TODO: Check actual hardware support
    // This would query the MPP system for codec support
    
    switch (codec) {
        case CodecType::H264:
        case CodecType::H265:
        case CodecType::MJPEG:
            return true;  // RK3588 supports these codecs
        default:
            return false;
    }
}

void MPPDecoder::reportError(OjoError error, const std::string& message) {
    LOGE("Decoder error: %s - %s", errorToString(error), message.c_str());
    
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (errorCallback_) {
        errorCallback_(error, message);
    }
}

void MPPDecoder::updateStatistics(double decodeTime) {
    // Update average decode time (exponential moving average)
    double currentAvg = statistics_.averageDecodeTime.load();
    statistics_.averageDecodeTime = (currentAvg * 0.9) + (decodeTime * 0.1);
}

const char* MPPDecoder::mppRetToString(MPP_RET ret) const {
    switch (ret) {
        case MPP_OK: return "Success";
        case MPP_NOK: return "Error";
        default: return "Unknown";
    }
}

} // namespace ojo
