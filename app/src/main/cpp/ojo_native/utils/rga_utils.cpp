#include "rga_utils.h"
#include <android/log.h>
#include <cstring>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "OjoRGAUtils"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace ojo {

RGAUtils::RGAUtils() : context_(std::make_unique<RGAContext>()) {
    LOGD("RGAUtils created");
}

RGAUtils::~RGAUtils() {
    cleanup();
    LOGD("RGAUtils destroyed");
}

bool RGAUtils::initialize() {
    LOGI("Initializing RGA hardware acceleration");
    
    if (context_->initialized) {
        LOGW("RGA already initialized");
        return context_->available;
    }

#ifdef HAVE_RGA
    if (initializeRGA()) {
        context_->initialized = true;
        context_->available = true;
        LOGI("RGA hardware acceleration initialized successfully");
        return true;
    } else {
        context_->initialized = true;
        context_->available = false;
        LOGW("RGA hardware acceleration not available, falling back to software");
        return false;
    }
#else
    LOGW("RGA support not compiled in, using software rendering");
    context_->initialized = true;
    context_->available = false;
    return false;
#endif
}

void RGAUtils::cleanup() {
    if (context_->initialized) {
#ifdef HAVE_RGA
        cleanupRGA();
#endif
        context_->initialized = false;
        context_->available = false;
        LOGD("RGA cleanup completed");
    }
}

bool RGAUtils::isAvailable() const {
    return context_->initialized && context_->available;
}

OjoError RGAUtils::convertFormat(const VideoFrame& srcFrame, VideoFrame& dstFrame, VideoFormat dstFormat) {
    if (!isAvailable()) {
        LOGW("RGA not available for format conversion");
        return OjoError::HARDWARE_NOT_AVAILABLE;
    }

    if (!srcFrame.data || srcFrame.dataSize == 0) {
        LOGE("Invalid source frame for format conversion");
        return OjoError::INVALID_PARAMETER;
    }

#ifdef HAVE_RGA
    // Setup destination frame
    dstFrame.width = srcFrame.width;
    dstFrame.height = srcFrame.height;
    dstFrame.format = dstFormat;
    dstFrame.dataSize = calculateBufferSize(dstFrame.width, dstFrame.height, videoFormatToRGA(dstFormat));
    dstFrame.data = std::make_unique<uint8_t[]>(dstFrame.dataSize);
    dstFrame.timestamp = srcFrame.timestamp;
    dstFrame.pts = srcFrame.pts;
    dstFrame.dts = srcFrame.dts;

    // Perform RGA format conversion
    RGAFormat srcRGAFormat = videoFormatToRGA(srcFrame.format);
    RGAFormat dstRGAFormat = videoFormatToRGA(dstFormat);
    
    OjoError result = processBuffer(srcFrame.data.get(), srcFrame.width, srcFrame.height, srcRGAFormat,
                                   dstFrame.data.get(), dstFrame.width, dstFrame.height, dstRGAFormat);
    
    if (result == OjoError::SUCCESS) {
        LOGD("RGA format conversion successful: %dx%d, %d->%d", 
             srcFrame.width, srcFrame.height, static_cast<int>(srcFrame.format), static_cast<int>(dstFormat));
    } else {
        LOGE("RGA format conversion failed");
    }
    
    return result;
#else
    return OjoError::HARDWARE_NOT_AVAILABLE;
#endif
}

OjoError RGAUtils::scaleFrame(const VideoFrame& srcFrame, VideoFrame& dstFrame, int dstWidth, int dstHeight) {
    if (!isAvailable()) {
        LOGW("RGA not available for scaling");
        return OjoError::HARDWARE_NOT_AVAILABLE;
    }

#ifdef HAVE_RGA
    // Setup destination frame
    dstFrame.width = dstWidth;
    dstFrame.height = dstHeight;
    dstFrame.format = srcFrame.format; // Keep same format
    dstFrame.dataSize = calculateBufferSize(dstWidth, dstHeight, videoFormatToRGA(srcFrame.format));
    dstFrame.data = std::make_unique<uint8_t[]>(dstFrame.dataSize);
    dstFrame.timestamp = srcFrame.timestamp;
    dstFrame.pts = srcFrame.pts;
    dstFrame.dts = srcFrame.dts;

    // Perform RGA scaling
    RGAFormat rgaFormat = videoFormatToRGA(srcFrame.format);
    
    OjoError result = processBuffer(srcFrame.data.get(), srcFrame.width, srcFrame.height, rgaFormat,
                                   dstFrame.data.get(), dstWidth, dstHeight, rgaFormat);
    
    if (result == OjoError::SUCCESS) {
        LOGD("RGA scaling successful: %dx%d -> %dx%d", 
             srcFrame.width, srcFrame.height, dstWidth, dstHeight);
    } else {
        LOGE("RGA scaling failed");
    }
    
    return result;
#else
    return OjoError::HARDWARE_NOT_AVAILABLE;
#endif
}

OjoError RGAUtils::convertAndScale(const VideoFrame& srcFrame, VideoFrame& dstFrame,
                                  int dstWidth, int dstHeight, VideoFormat dstFormat) {
    if (!isAvailable()) {
        LOGW("RGA not available for convert and scale");
        return OjoError::HARDWARE_NOT_AVAILABLE;
    }

#ifdef HAVE_RGA
    // Setup destination frame
    dstFrame.width = dstWidth;
    dstFrame.height = dstHeight;
    dstFrame.format = dstFormat;
    dstFrame.dataSize = calculateBufferSize(dstWidth, dstHeight, videoFormatToRGA(dstFormat));
    dstFrame.data = std::make_unique<uint8_t[]>(dstFrame.dataSize);
    dstFrame.timestamp = srcFrame.timestamp;
    dstFrame.pts = srcFrame.pts;
    dstFrame.dts = srcFrame.dts;

    // Perform RGA convert and scale in one operation
    RGAFormat srcRGAFormat = videoFormatToRGA(srcFrame.format);
    RGAFormat dstRGAFormat = videoFormatToRGA(dstFormat);
    
    OjoError result = processBuffer(srcFrame.data.get(), srcFrame.width, srcFrame.height, srcRGAFormat,
                                   dstFrame.data.get(), dstWidth, dstHeight, dstRGAFormat);
    
    if (result == OjoError::SUCCESS) {
        LOGD("RGA convert and scale successful: %dx%d (%d) -> %dx%d (%d)", 
             srcFrame.width, srcFrame.height, static_cast<int>(srcFrame.format),
             dstWidth, dstHeight, static_cast<int>(dstFormat));
    } else {
        LOGE("RGA convert and scale failed");
    }
    
    return result;
#else
    return OjoError::HARDWARE_NOT_AVAILABLE;
#endif
}

OjoError RGAUtils::nv12ToRgb(const VideoFrame& srcFrame, VideoFrame& dstFrame) {
    return convertFormat(srcFrame, dstFrame, VideoFormat::RGB24);
}

OjoError RGAUtils::rotateFrame(const VideoFrame& srcFrame, VideoFrame& dstFrame, int rotationDegrees) {
    // Rotation implementation would go here
    // For now, just copy the frame
    LOGW("RGA rotation not implemented yet, copying frame");
    
    dstFrame.width = srcFrame.width;
    dstFrame.height = srcFrame.height;
    dstFrame.format = srcFrame.format;
    dstFrame.dataSize = srcFrame.dataSize;
    dstFrame.data = std::make_unique<uint8_t[]>(dstFrame.dataSize);
    dstFrame.timestamp = srcFrame.timestamp;
    dstFrame.pts = srcFrame.pts;
    dstFrame.dts = srcFrame.dts;
    
    std::memcpy(dstFrame.data.get(), srcFrame.data.get(), srcFrame.dataSize);
    
    return OjoError::SUCCESS;
}

// Static utility functions
RGAFormat RGAUtils::videoFormatToRGA(VideoFormat format) {
    switch (format) {
        case VideoFormat::NV12: return RGAFormat::NV12;
        case VideoFormat::NV21: return RGAFormat::NV21;
        case VideoFormat::YUV420P: return RGAFormat::YUV420P;
        case VideoFormat::RGB24: return RGAFormat::RGB888;
        case VideoFormat::RGBA32: return RGAFormat::RGBA8888;
        default: return RGAFormat::UNKNOWN;
    }
}

VideoFormat RGAUtils::rgaFormatToVideo(RGAFormat format) {
    switch (format) {
        case RGAFormat::NV12: return VideoFormat::NV12;
        case RGAFormat::NV21: return VideoFormat::NV21;
        case RGAFormat::YUV420P: return VideoFormat::YUV420P;
        case RGAFormat::RGB888: return VideoFormat::RGB24;
        case RGAFormat::RGBA8888: return VideoFormat::RGBA32;
        case RGAFormat::BGRA8888: return VideoFormat::RGBA32; // Map to RGBA32 since BGRA32 doesn't exist
        default: return VideoFormat::UNKNOWN;
    }
}

size_t RGAUtils::calculateBufferSize(int width, int height, RGAFormat format) {
    switch (format) {
        case RGAFormat::NV12:
        case RGAFormat::NV21:
        case RGAFormat::YUV420P:
            return width * height * 3 / 2;
        case RGAFormat::RGB888:
            return width * height * 3;
        case RGAFormat::RGBA8888:
        case RGAFormat::BGRA8888:
            return width * height * 4;
        default:
            return 0;
    }
}

OjoError RGAUtils::processBuffer(const void* srcBuffer, int srcWidth, int srcHeight, RGAFormat srcFormat,
                                void* dstBuffer, int dstWidth, int dstHeight, RGAFormat dstFormat) {
#ifdef HAVE_RGA
    if (!isAvailable()) {
        return OjoError::HARDWARE_NOT_AVAILABLE;
    }

    int ret = 0;
    rga_buffer_t srcImg, dstImg;
    rga_buffer_handle_t srcHandle, dstHandle;

    memset(&srcImg, 0, sizeof(srcImg));
    memset(&dstImg, 0, sizeof(dstImg));

    // Calculate buffer sizes
    size_t srcBufSize = calculateBufferSize(srcWidth, srcHeight, srcFormat);
    size_t dstBufSize = calculateBufferSize(dstWidth, dstHeight, dstFormat);

    // Import buffers
    srcHandle = importbuffer_virtualaddr(const_cast<void*>(srcBuffer), srcBufSize);
    dstHandle = importbuffer_virtualaddr(dstBuffer, dstBufSize);

    if (srcHandle == 0 || dstHandle == 0) {
        LOGE("RGA importbuffer failed");
        goto release_buffer;
    }

    // Setup source image
    srcImg = wrapbuffer_handle(srcHandle, srcWidth, srcHeight, mapVideoFormatToRGA(rgaFormatToVideo(srcFormat)));

    // Setup destination image
    dstImg = wrapbuffer_handle(dstHandle, dstWidth, dstHeight, mapVideoFormatToRGA(rgaFormatToVideo(dstFormat)));

    // Perform RGA operation
    ret = imcheck(srcImg, dstImg, {}, {});
    if (IM_STATUS_NOERROR != ret) {
        LOGE("RGA check failed: %s", imStrError((IM_STATUS)ret));
        goto release_buffer;
    }

    ret = imresize(srcImg, dstImg);
    if (ret != IM_STATUS_SUCCESS) {
        LOGE("RGA resize failed: %s", imStrError((IM_STATUS)ret));
        goto release_buffer;
    }

    LOGD("RGA operation successful: %dx%d -> %dx%d", srcWidth, srcHeight, dstWidth, dstHeight);

release_buffer:
    if (srcHandle) releasebuffer_handle(srcHandle);
    if (dstHandle) releasebuffer_handle(dstHandle);

    return (ret == IM_STATUS_SUCCESS) ? OjoError::SUCCESS : OjoError::HARDWARE_ERROR;
#else
    return OjoError::HARDWARE_NOT_AVAILABLE;
#endif
}

bool RGAUtils::initializeRGA() {
#ifdef HAVE_RGA
    // Check RGA availability
    const char* version = querystring(RGA_VERSION);
    if (version == nullptr) {
        LOGE("Failed to query RGA version");
        return false;
    }

    LOGI("RGA version: %s", version);
    context_->version = 1; // Simplified version tracking
    return true;
#else
    return false;
#endif
}

void RGAUtils::cleanupRGA() {
#ifdef HAVE_RGA
    // RGA cleanup if needed
    LOGD("RGA cleanup completed");
#endif
}

#ifdef HAVE_RGA
int RGAUtils::mapVideoFormatToRGA(VideoFormat format) {
    switch (format) {
        case VideoFormat::NV12: return RK_FORMAT_YCbCr_420_SP;
        case VideoFormat::NV21: return RK_FORMAT_YCrCb_420_SP;
        case VideoFormat::YUV420P: return RK_FORMAT_YCbCr_420_P;
        case VideoFormat::RGB24: return RK_FORMAT_RGB_888;
        case VideoFormat::RGBA32: return RK_FORMAT_RGBA_8888;
        default: return RK_FORMAT_UNKNOWN;
    }
}
#endif

} // namespace ojo
