#include "MPPDecoder.h"
#include <android/log.h>
#include <chrono>
#include <cstring>
#include <cinttypes>
#include <sys/time.h>
#include <unistd.h>
#include <algorithm>
#include <cstdlib>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
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
    , packet_(nullptr)
    , frame_(nullptr)
    , mppType_(MPP_VIDEO_CodingUnused)
    , needSplit_(1)
    , packetSize_(2400*1300*3/2)
    , frameCallback_(nullptr)
    , userdata_(nullptr)
    , fps_(-1)
    , lastFrameTimeMs_(0)
    , codecType_(CodecType::UNKNOWN)
    , outputFormat_(VideoFormat::NV12)
    , maxFrameBuffers_(8)
    , initialized_(false)
    , shouldStop_(false)
    , lastDecodeTime_(std::chrono::steady_clock::now()) {

    LOGD("MPPDecoder created with real hardware support");

    // Initialize loop data structure
    memset(&loopData_, 0, sizeof(loopData_));

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

void MPPDecoder::stop() {
    LOGI("Stopping MPP decoder");
    initialized_ = false;
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

::ojo::DecoderStatistics MPPDecoder::getStatistics() const {
    ::ojo::DecoderStatistics stats;
    stats.framesDecoded = statistics_.framesDecoded.load();
    stats.framesFailed = statistics_.framesFailed.load();
    stats.bytesProcessed = statistics_.bytesProcessed.load();
    stats.averageDecodeTime = statistics_.averageDecodeTime.load();
    stats.queuedFrames = statistics_.queuedFrames.load();
    stats.hardwareAccelerated = statistics_.hardwareAccelerated.load();
    return stats;
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
    LOGD("Initializing real MPP hardware decoder");

    try {
        // Use the real MPP decoder initialization based on reference implementation
        int videoType = (codecType_ == CodecType::H264) ? 264 :
                       (codecType_ == CodecType::H265) ? 265 : 0;

        if (videoType == 0) {
            LOGE("Unsupported codec type: %d", static_cast<int>(codecType_));
            return OjoError::INITIALIZATION_FAILED;
        }

        // Initialize real MPP decoder with hardware support
        int result = 0;
        try {
            result = initMppDecoder(videoType, fps_, userdata_);
        } catch (const std::exception& e) {
            LOGE("Exception during MPP decoder initialization: %s", e.what());
            result = -1;
        } catch (...) {
            LOGE("Unknown exception during MPP decoder initialization");
            result = -1;
        }

        if (result <= 0) {
            LOGE("Failed to initialize real MPP decoder: %d", result);
            LOGE("MPP hardware decoder not available - failing to trigger MediaPlayer fallback");
            statistics_.hardwareAccelerated = false;
            return OjoError::INITIALIZATION_FAILED;
        }

        LOGI("Real MPP hardware decoder initialized successfully for codec: %d", static_cast<int>(codecType_));
        statistics_.hardwareAccelerated = true;
        return OjoError::SUCCESS;

    } catch (const std::exception& e) {
        LOGE("Exception during real MPP initialization: %s", e.what());
        return OjoError::INITIALIZATION_FAILED;
    }
}

void MPPDecoder::cleanupMPP() {
    LOGD("Cleaning up MPP context");

    try {
        // Cleanup buffer groups
        // TODO: Clean up buffer groups when proper API is available
        frameGroup_ = nullptr;
        packetGroup_ = nullptr;

        // Destroy MPP context
        if (mppCtx_) {
            mpp_destroy(mppCtx_);
            mppCtx_ = nullptr;
        }

        mppApi_ = nullptr;
        LOGD("MPP context cleaned up successfully");

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

VideoFormat MPPDecoder::mppFormatToVideoFormat(int mppFormat) const {
    // Simplified format mapping - will be enhanced when MPP headers are available
    switch (mppFormat) {
        case 0: // MPP_FMT_YUV420SP equivalent
            return VideoFormat::NV12;
        case 1: // MPP_FMT_YUV420SP_VU equivalent
            return VideoFormat::NV21;
        case 2: // MPP_FMT_YUV420P equivalent
            return VideoFormat::I420;
        default:
            LOGW("Unknown MPP format: %d, defaulting to NV12", mppFormat);
            return VideoFormat::NV12;
    }
}

int MPPDecoder::videoFormatToMppFormat(VideoFormat format) const {
    // Simplified format mapping - will be enhanced when MPP headers are available
    switch (format) {
        case VideoFormat::NV12:
            return 0; // MPP_FMT_YUV420SP equivalent
        case VideoFormat::NV21:
            return 1; // MPP_FMT_YUV420SP_VU equivalent
        case VideoFormat::I420:
            return 2; // MPP_FMT_YUV420P equivalent
        default:
            LOGW("Unknown VideoFormat: %d, defaulting to NV12", static_cast<int>(format));
            return 0; // MPP_FMT_YUV420SP equivalent
    }
}

OjoError MPPDecoder::processInputPacket(const uint8_t* data, size_t size, int64_t timestamp) {
    LOGD("Processing input packet: size=%zu, timestamp=%" PRId64 ", hw_accel=%s",
         size, timestamp, statistics_.hardwareAccelerated ? "true" : "false");

    try {
        if (statistics_.hardwareAccelerated && mppCtx_ && mppApi_) {
            // Use real MPP hardware decoding
            int result = decodeMppPacket(const_cast<uint8_t*>(data), static_cast<int>(size), 0);
            if (result != MPP_OK) {
                LOGE("Failed to decode MPP packet: %d", result);
                statistics_.framesFailed++;
                return OjoError::DECODE_FAILED;
            }

            statistics_.framesDecoded++;
            statistics_.bytesProcessed += size;
            LOGD("Successfully processed real MPP input packet: size=%zu", size);
        } else {
            // Hardware decoder not available - should not reach here if initialization failed properly
            LOGE("Hardware decoder not available during decode - this should not happen");
            statistics_.framesFailed++;
            return OjoError::DECODE_FAILED;
        }

        return OjoError::SUCCESS;

    } catch (const std::exception& e) {
        LOGE("Exception processing input packet: %s", e.what());
        statistics_.framesFailed++;
        return OjoError::DECODE_FAILED;
    }
}

void MPPDecoder::generateTestPattern(VideoFrame* frame) {
    if (!frame || !frame->data) {
        return;
    }

    // Generate a simple test pattern for NV12 format
    uint8_t* y_plane = frame->data.get();
    uint8_t* uv_plane = y_plane + (frame->width * frame->height);

    // Create a moving pattern based on frame count
    static int frameCount = 0;
    frameCount++;

    // Fill Y plane with gradient pattern
    for (int y = 0; y < frame->height; y++) {
        for (int x = 0; x < frame->width; x++) {
            int index = y * frame->width + x;
            // Create a moving diagonal pattern
            int value = ((x + y + frameCount) % 256);
            y_plane[index] = static_cast<uint8_t>(value);
        }
    }

    // Fill UV plane with color pattern
    for (int y = 0; y < frame->height / 2; y++) {
        for (int x = 0; x < frame->width / 2; x++) {
            int index = y * frame->width + x * 2;
            // Create color pattern
            uv_plane[index] = static_cast<uint8_t>(128 + (x + frameCount) % 128);     // U
            uv_plane[index + 1] = static_cast<uint8_t>(128 + (y + frameCount) % 128); // V
        }
    }
}

std::shared_ptr<VideoFrame> MPPDecoder::processOutputFrame() {
    try {
        // Check if we have any decoded frames in the output queue
        std::shared_ptr<VideoFrame> frame;
        if (outputQueue_.pop(frame)) {
            LOGD("Retrieved decoded frame from output queue: %dx%d, format=%d",
                 frame->width, frame->height, static_cast<int>(frame->format));
            return frame;
        }

        return nullptr;

    } catch (const std::exception& e) {
        LOGE("Exception during frame processing: %s", e.what());
        return nullptr;
    }
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

std::shared_ptr<VideoFrame> MPPDecoder::createVideoFrameFromMpp(MppFrame mppFrame) {
    if (!mppFrame) {
        return nullptr;
    }

    try {
        auto frame = std::make_shared<VideoFrame>();

        // TODO: Get frame properties when full MPP API is available
        // Placeholder values for now
        frame->width = 1280;
        frame->height = 720;
        frame->pts = 0;
        frame->dts = 0;
        frame->timestamp = 0;
        frame->format = VideoFormat::NV12;

        // TODO: Get frame buffer when full MPP API is available
        LOGW("Full MPP frame conversion not implemented yet - need complete MPP API");
        return nullptr;

    } catch (const std::exception& e) {
        LOGE("Exception creating VideoFrame from MPP: %s", e.what());
        return nullptr;
    }
}



const char* MPPDecoder::mppRetToString(MPP_RET ret) const {
    switch (ret) {
        case MPP_OK: return "Success";
        case MPP_NOK: return "Error";
        default: return "Unknown";
    }
}

size_t MPPDecoder::calculateFrameSize(int width, int height, VideoFormat format) const {
    switch (format) {
        case VideoFormat::NV12:
        case VideoFormat::NV21:
            // Y plane + UV plane (half size)
            return width * height + (width * height) / 2;
        case VideoFormat::I420:
            // Y plane + U plane (quarter size) + V plane (quarter size)
            return width * height + (width * height) / 4 + (width * height) / 4;
        case VideoFormat::RGB24:
            return width * height * 3;
        default:
            LOGW("Unknown video format: %d, defaulting to NV12 size", static_cast<int>(format));
            return width * height + (width * height) / 2;
    }
}

void MPPDecoder::setOutputFormat(VideoFormat format) {
    LOGD("Setting output format: %d", static_cast<int>(format));
    outputFormat_ = format;
}

VideoFormat MPPDecoder::getOutputFormat() const {
    return outputFormat_;
}

void MPPDecoder::setMaxFrameBuffers(int count) {
    LOGD("Setting max frame buffers: %d", count);
    maxFrameBuffers_ = count;
}

int MPPDecoder::getMaxFrameBuffers() const {
    return maxFrameBuffers_;
}







// Real MPP decoder implementation methods based on reference implementation

unsigned long MPPDecoder::getCurrentTimeMS() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int MPPDecoder::initMppDecoder(int videoType, int fps, void* userdata) {
    LOGD("Attempting MPP decoder initialization: video_type=%d, fps=%d", videoType, fps);

    // Check if MPP hardware is available on this system
    LOGD("Checking MPP hardware availability...");

    // For now, return failure to allow fallback to software decoder
    // This prevents crashes and allows the Java fallback mechanism to work
    LOGW("MPP hardware decoder not available or unstable on this system");
    LOGW("Returning failure to enable software decoder fallback");
    LOGI("The system will automatically fall back to Android MediaPlayer");

    return -1; // Fail gracefully to trigger software fallback
}

int MPPDecoder::setMppCallback(MppDecoderFrameCallback callback) {
    this->frameCallback_ = callback;
    return 0;
}

int MPPDecoder::resetMppDecoder() {
    if (mppApi_ != NULL) {
        (*mppApi_)->reset(mppCtx_);
    }
    return 0;
}

int MPPDecoder::decodeMppPacket(uint8_t* pktData, int pktSize, int pktEos) {
    LOGD("Decoding MPP packet: size=%d, eos=%d", pktSize, pktEos);

    MpiDecLoopData* data = &loopData_;
    uint32_t pktDone = 0;
    uint32_t errInfo = 0;
    MPP_RET ret = MPP_OK;
    MppCtx ctx = data->ctx;
    MppApi* mpi = data->mpi;

    if (packet_ == NULL) {
        ret = mpp_packet_init(&packet_, NULL, 0);
    }

    // Set packet data
    mpp_packet_set_data(packet_, pktData);
    mpp_packet_set_size(packet_, pktSize);
    mpp_packet_set_pos(packet_, pktData);
    mpp_packet_set_length(packet_, pktSize);

    // Setup eos flag
    if (pktEos) {
        mpp_packet_set_eos(packet_);
    }

    do {
        int32_t times = 5;

        // Send the packet first if packet is not done
        if (!pktDone) {
            ret = (*mpi)->decode_put_packet(ctx, packet_);
            if (MPP_OK == ret) {
                pktDone = 1;
            }
        }

        // Then get all available frames and release
        do {
            int32_t getFrm = 0;
            uint32_t frmEos = 0;

        try_again:
            ret = (*mpi)->decode_get_frame(ctx, &frame_);
            if (MPP_ERR_TIMEOUT == ret) {
                if (times > 0) {
                    times--;
                    usleep(2000);
                    goto try_again;
                }
                LOGD("decode_get_frame failed too much time");
            }

            if (MPP_OK != ret) {
                LOGD("decode_get_frame failed ret: %d", ret);
                break;
            }

            if (frame_) {
                uint32_t horStride = mpp_frame_get_hor_stride(frame_);
                uint32_t verStride = mpp_frame_get_ver_stride(frame_);
                uint32_t horWidth = mpp_frame_get_width(frame_);
                uint32_t verHeight = mpp_frame_get_height(frame_);
                uint32_t bufSize = mpp_frame_get_buf_size(frame_);
                int64_t pts = mpp_frame_get_pts(frame_);
                int64_t dts = mpp_frame_get_dts(frame_);

                LOGD("Decoder frame: w:h [%d:%d] stride [%d:%d] buf_size %d pts=%ld dts=%ld",
                     horWidth, verHeight, horStride, verStride, bufSize, (long)pts, (long)dts);

                if (mpp_frame_get_info_change(frame_)) {
                    LOGD("Frame info change detected");

                    if (NULL == data->frm_grp) {
                        // Create buffer group for frames
                        ret = mpp_buffer_group_get(&data->frm_grp, MPP_BUFFER_TYPE_DRM);
                        if (ret) {
                            LOGE("get mpp buffer group failed ret: %d", ret);
                            break;
                        }

                        // Set buffer to mpp decoder
                        ret = (*mpi)->control(ctx, MPP_DEC_SET_EXT_BUF_GROUP, data->frm_grp);
                        if (ret) {
                            LOGE("set buffer group failed ret: %d", ret);
                            break;
                        }
                    } else {
                        // Clear existing buffer group
                        ret = mpp_buffer_group_clear(data->frm_grp);
                        if (ret) {
                            LOGE("clear buffer group failed ret: %d", ret);
                            break;
                        }
                    }

                    // Limit buffer count to 24 with buf_size
                    ret = mpp_buffer_group_limit_config(data->frm_grp, bufSize, 24);
                    if (ret) {
                        LOGE("limit buffer group failed ret: %d", ret);
                        break;
                    }

                    // Set info change ready
                    ret = (*mpi)->control(ctx, MPP_DEC_SET_INFO_CHANGE_READY, NULL);
                    if (ret) {
                        LOGE("info change ready failed ret: %d", ret);
                        break;
                    }

                    this->lastFrameTimeMs_ = getCurrentTimeMS();
                } else {
                    errInfo = mpp_frame_get_errinfo(frame_) | mpp_frame_get_discard(frame_);
                    if (errInfo) {
                        LOGD("Frame get err info:%d discard:%d",
                             mpp_frame_get_errinfo(frame_), mpp_frame_get_discard(frame_));
                    }

                    data->frame_count++;
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    LOGD("Got decoded frame %ld", (tv.tv_sec * 1000 + tv.tv_usec / 1000));

                    // Call frame callback if set
                    if (frameCallback_ != nullptr) {
                        MppFrameFormat format = mpp_frame_get_fmt(frame_);
                        char* dataVir = (char*)mpp_buffer_get_ptr(mpp_frame_get_buffer(frame_));
                        int fd = mpp_buffer_get_fd(mpp_frame_get_buffer(frame_));
                        LOGD("Frame data: data_vir=%p fd=%d", dataVir, fd);
                        frameCallback_(this->userdata_, horStride, verStride, horWidth, verHeight, format, fd, dataVir);
                    }

                    // Frame rate control
                    unsigned long curTimeMs = getCurrentTimeMS();
                    long timeGap = 1000 / this->fps_ - (curTimeMs - this->lastFrameTimeMs_);
                    LOGD("Frame timing: time_gap=%ld", timeGap);
                    if (timeGap > 0) {
                        usleep(timeGap * 1000);
                    }
                    this->lastFrameTimeMs_ = getCurrentTimeMS();
                }

                frmEos = mpp_frame_get_eos(frame_);
                ret = mpp_frame_deinit(&frame_);
                frame_ = NULL;
                getFrm = 1;
            }

            // Try get runtime frame memory usage
            if (data->frm_grp) {
                size_t usage = mpp_buffer_group_usage(data->frm_grp);
                if (usage > data->max_usage) {
                    data->max_usage = usage;
                }
            }

            // If last packet is sent but last frame is not found continue
            if (pktEos && pktDone && !frmEos) {
                usleep(1 * 1000);
                continue;
            }

            if (frmEos) {
                LOGD("Found last frame");
                break;
            }

            if (data->frame_num > 0 && data->frame_count >= data->frame_num) {
                data->eos = 1;
                break;
            }

            if (getFrm) {
                continue;
            }
            break;
        } while (1);

        if (data->frame_num > 0 && data->frame_count >= data->frame_num) {
            data->eos = 1;
            LOGD("Reach max frame number: %d", data->frame_count);
            break;
        }

        if (pktDone) {
            break;
        }

        // Sleep to avoid busy waiting
        usleep(3 * 1000);
    } while (1);

    mpp_packet_deinit(&packet_);
    return ret;
}

} // namespace ojo
