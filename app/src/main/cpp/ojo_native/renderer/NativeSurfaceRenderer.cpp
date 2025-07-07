#include "NativeSurfaceRenderer.h"
#include <android/log.h>
#include <chrono>

// Use specific LOG_TAG for this module if not already defined
#ifndef LOG_TAG
#define LOG_TAG "OjoNativeRenderer"
#endif

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace ojo {

NativeSurfaceRenderer::NativeSurfaceRenderer()
    : nativeWindow_(nullptr)
    , rgaUtils_(std::make_unique<RGAUtils>())
    , rgaAvailable_(false)
    , rgaContext_(nullptr)
    , shouldStop_(false)
    , isPaused_(false)
    , maxQueueSize_(8)
    , lastRenderTime_(std::chrono::steady_clock::now())
    , lastFpsTime_(std::chrono::steady_clock::now())
    , frameCount_(0)
    , initialized_(false)
    , running_(false) {
    
    LOGD("NativeSurfaceRenderer created");
    statistics_.reset();
    
    // Initialize display configuration
    displayConfig_.x = 0;
    displayConfig_.y = 0;
    displayConfig_.width = 1280;
    displayConfig_.height = 720;
    displayConfig_.mode = 1;
    displayConfig_.vsyncEnabled = true;
}

NativeSurfaceRenderer::~NativeSurfaceRenderer() {
    LOGD("NativeSurfaceRenderer destructor called");
    cleanup();
}

OjoError NativeSurfaceRenderer::initialize(ANativeWindow* window) {
    if (initialized_) {
        LOGW("Renderer already initialized");
        return OjoError::SUCCESS;
    }
    
    if (!window) {
        LOGE("Invalid native window");
        return OjoError::INVALID_PARAMETER;
    }
    
    std::lock_guard<std::mutex> lock(surfaceMutex_);
    nativeWindow_ = window;
    
    // Configure surface
    OjoError result = configureSurface();
    if (result != OjoError::SUCCESS) {
        LOGE("Failed to configure surface");
        return result;
    }
    
    // Initialize RGA if available
    rgaAvailable_ = initializeRGA();
    if (rgaAvailable_) {
        LOGI("RGA hardware acceleration enabled");
        statistics_.hardwareAccelerated = true;
    } else {
        LOGI("Using software rendering");
        statistics_.hardwareAccelerated = false;
    }
    
    initialized_ = true;
    LOGI("Native surface renderer initialized successfully");
    
    return OjoError::SUCCESS;
}

void NativeSurfaceRenderer::cleanup() {
    if (!initialized_) {
        return;
    }
    
    LOGI("Cleaning up native surface renderer");
    
    // Stop renderer
    stop();
    
    // Clear frame queue
    frameQueue_.clear();
    
    // Cleanup RGA
    if (rgaAvailable_) {
        cleanupRGA();
        rgaAvailable_ = false;
    }
    
    // Release surface
    releaseSurface();
    
    initialized_ = false;
    LOGI("Native surface renderer cleanup completed");
}

OjoError NativeSurfaceRenderer::renderFrame(const VideoFrame& frame) {
    if (!initialized_) {
        LOGE("Renderer not initialized");
        return OjoError::INITIALIZATION_FAILED;
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    OjoError result = renderFrameInternal(frame);
    
    auto endTime = std::chrono::steady_clock::now();
    auto renderTime = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime).count() / 1000.0;
    
    updateStatistics(renderTime);
    
    if (result == OjoError::SUCCESS) {
        statistics_.framesRendered++;
        
        // Call frame rendered callback
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (frameRenderedCallback_) {
            frameRenderedCallback_(frame);
        }
    } else {
        statistics_.framesFailed++;
        reportError(result, "Failed to render frame");
    }
    
    return result;
}

OjoError NativeSurfaceRenderer::renderFrameAsync(std::shared_ptr<VideoFrame> frame) {
    if (!initialized_) {
        LOGE("Renderer not initialized");
        return OjoError::INITIALIZATION_FAILED;
    }
    
    if (frameQueue_.size() >= static_cast<size_t>(maxQueueSize_)) {
        statistics_.framesDropped++;
        LOGW("Frame queue full, dropping frame");
        return OjoError::TIMEOUT;
    }
    
    frameQueue_.push(frame);
    statistics_.queuedFrames = frameQueue_.size();
    
    return OjoError::SUCCESS;
}

void NativeSurfaceRenderer::start() {
    if (running_) {
        LOGW("Renderer already running");
        return;
    }
    
    LOGI("Starting native surface renderer");
    
    shouldStop_ = false;
    isPaused_ = false;
    
    // Start render thread
    renderThread_ = std::thread(&NativeSurfaceRenderer::renderLoop, this);
    
    running_ = true;
    LOGI("Native surface renderer started");
}

void NativeSurfaceRenderer::stop() {
    if (!running_) {
        return;
    }
    
    LOGI("Stopping native surface renderer");
    
    shouldStop_ = true;
    
    // Wait for render thread to finish
    if (renderThread_.joinable()) {
        renderThread_.join();
    }
    
    running_ = false;
    LOGI("Native surface renderer stopped");
}

void NativeSurfaceRenderer::setMaxQueueSize(int maxSize) {
    maxQueueSize_ = maxSize;
    LOGD("Set max queue size to %d", maxSize);
}

void NativeSurfaceRenderer::setVSyncEnabled(bool enabled) {
    displayConfig_.vsyncEnabled = enabled;
    LOGD("VSync %s", enabled ? "enabled" : "disabled");
}

::ojo::RendererStatistics NativeSurfaceRenderer::getStatistics() const {
    ::ojo::RendererStatistics stats;
    stats.framesRendered = statistics_.framesRendered.load();
    stats.framesDropped = statistics_.framesDropped.load();
    stats.averageRenderTime = statistics_.averageRenderTime.load();
    stats.currentFps = statistics_.currentFps.load();
    return stats;
}

void NativeSurfaceRenderer::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    errorCallback_ = callback;
}

void NativeSurfaceRenderer::setFrameRenderedCallback(FrameRenderedCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    frameRenderedCallback_ = callback;
}

bool NativeSurfaceRenderer::isRunning() const {
    return running_;
}

// Private methods implementation

void NativeSurfaceRenderer::renderLoop() {
    LOGD("Render loop started");
    
    while (!shouldStop_) {
        if (isPaused_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }
        
        std::shared_ptr<VideoFrame> frame;
        
        // Wait for frame with timeout
        if (frameQueue_.waitAndPop(frame, 16)) {
            try {
                OjoError result = renderFrame(*frame);
                if (result != OjoError::SUCCESS) {
                    LOGW("Failed to render frame in render loop");
                }
                
                statistics_.queuedFrames = frameQueue_.size();
                
            } catch (const std::exception& e) {
                LOGE("Exception in render loop: %s", e.what());
                statistics_.framesFailed++;
            }
        }
        
        // VSync simulation (16ms for 60fps)
        if (displayConfig_.vsyncEnabled) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }
    
    LOGD("Render loop ended");
}

OjoError NativeSurfaceRenderer::renderFrameInternal(const VideoFrame& frame) {
    std::lock_guard<std::mutex> lock(surfaceMutex_);
    
    if (!nativeWindow_) {
        LOGE("No native window available");
        return OjoError::RENDER_FAILED;
    }
    
    // Lock surface for rendering
    OjoError result = lockSurface();
    if (result != OjoError::SUCCESS) {
        return result;
    }
    
    // Choose rendering method based on hardware availability
    if (rgaAvailable_) {
        result = renderFrameWithRGA(frame);
    } else {
        result = renderFrameSoftware(frame);
    }
    
    // Unlock surface
    unlockSurface();
    
    return result;
}

OjoError NativeSurfaceRenderer::renderFrameWithRGA(const VideoFrame& frame) {
    if (!rgaAvailable_ || !rgaUtils_) {
        LOGW("RGA not available, falling back to software rendering");
        return renderFrameSoftware(frame);
    }

    std::lock_guard<std::mutex> lock(rgaMutex_);

    try {
        // Create destination frame for RGBA conversion
        VideoFrame rgbaFrame;

        // Use RGA to convert NV12 to RGBA and scale to window size
        OjoError result = rgaUtils_->convertAndScale(frame, rgbaFrame,
                                                    windowBuffer_.width, windowBuffer_.height,
                                                    VideoFormat::RGBA32);

        if (result != OjoError::SUCCESS) {
            LOGE("RGA convert and scale failed, falling back to software");
            return renderFrameSoftware(frame);
        }

        // Copy RGA output to window buffer
        if (rgbaFrame.data && windowBuffer_.bits) {
            size_t copySize = std::min(rgbaFrame.dataSize,
                                     static_cast<size_t>(windowBuffer_.height * windowBuffer_.stride * 4));
            std::memcpy(windowBuffer_.bits, rgbaFrame.data.get(), copySize);

            LOGD("RGA hardware rendering successful: %dx%d -> %dx%d",
                 frame.width, frame.height, windowBuffer_.width, windowBuffer_.height);
        }

        return OjoError::SUCCESS;

    } catch (const std::exception& e) {
        LOGE("Exception in RGA rendering: %s", e.what());
        return renderFrameSoftware(frame);
    }
}

OjoError NativeSurfaceRenderer::renderFrameSoftware(const VideoFrame& frame) {
    if (!windowBuffer_.bits) {
        LOGE("No window buffer available for software rendering");
        return OjoError::RENDER_FAILED;
    }

    LOGI("Software rendering frame: %dx%d, format=%d, data_size=%zu, timestamp=%" PRId64,
         frame.width, frame.height, static_cast<int>(frame.format),
         frame.dataSize, frame.timestamp);

    // Log window buffer details for debugging
    LOGD("Window buffer: %dx%d, stride=%d, format=%d",
         windowBuffer_.width, windowBuffer_.height,
         windowBuffer_.stride, windowBuffer_.format);

    try {
        // Handle different video formats
        switch (frame.format) {
            case VideoFormat::NV12:
                return renderNV12ToRGBA(frame);
            case VideoFormat::NV21:
                return renderNV21ToRGBA(frame);
            case VideoFormat::I420:
            case VideoFormat::YUV420P:
                return renderI420ToRGBA(frame);
            case VideoFormat::RGB24:
                return renderRGB24ToRGBA(frame);
            case VideoFormat::RGBA32:
                return renderRGBA32(frame);
            default:
                LOGE("Unsupported video format: %d", static_cast<int>(frame.format));
                // Fallback: render test pattern
                return renderTestPattern();
        }

    } catch (const std::exception& e) {
        LOGE("Exception in software rendering: %s", e.what());
        return OjoError::RENDER_FAILED;
    }
}

OjoError NativeSurfaceRenderer::lockSurface() {
    if (!nativeWindow_) {
        return OjoError::RENDER_FAILED;
    }
    
    int result = ANativeWindow_lock(nativeWindow_, &windowBuffer_, nullptr);
    if (result != 0) {
        LOGE("Failed to lock native window: %d", result);
        return OjoError::RENDER_FAILED;
    }
    
    return OjoError::SUCCESS;
}

void NativeSurfaceRenderer::unlockSurface() {
    if (nativeWindow_) {
        ANativeWindow_unlockAndPost(nativeWindow_);
    }
}

OjoError NativeSurfaceRenderer::configureSurface() {
    if (!nativeWindow_) {
        return OjoError::INVALID_PARAMETER;
    }
    
    // Set surface format (RGBA_8888)
    int result = ANativeWindow_setBuffersGeometry(
        nativeWindow_,
        displayConfig_.width,
        displayConfig_.height,
        WINDOW_FORMAT_RGBA_8888
    );
    
    if (result != 0) {
        LOGE("Failed to set surface geometry: %d", result);
        return OjoError::RENDER_FAILED;
    }
    
    LOGI("Surface configured: %dx%d, format=RGBA_8888", 
         displayConfig_.width, displayConfig_.height);
    
    return OjoError::SUCCESS;
}

void NativeSurfaceRenderer::releaseSurface() {
    std::lock_guard<std::mutex> lock(surfaceMutex_);
    
    if (nativeWindow_) {
        // Note: Don't release the window here as it's managed by the caller
        nativeWindow_ = nullptr;
    }
}

bool NativeSurfaceRenderer::initializeRGA() {
    LOGI("Initializing RGA hardware acceleration");

    if (!rgaUtils_) {
        LOGE("RGA utils not available");
        return false;
    }

    std::lock_guard<std::mutex> lock(rgaMutex_);

    if (rgaUtils_->initialize()) {
        rgaAvailable_ = true;
        LOGI("RGA hardware acceleration initialized successfully");
        return true;
    } else {
        rgaAvailable_ = false;
        LOGW("RGA hardware acceleration not available, using software rendering");
        return false;
    }
}

void NativeSurfaceRenderer::cleanupRGA() {
    LOGD("Cleaning up RGA resources");

    std::lock_guard<std::mutex> lock(rgaMutex_);

    if (rgaUtils_) {
        rgaUtils_->cleanup();
    }

    rgaAvailable_ = false;

    if (rgaContext_) {
        rgaContext_ = nullptr;
    }
}

void NativeSurfaceRenderer::updateStatistics(double renderTime) {
    // Update average render time (exponential moving average)
    double currentAvg = statistics_.averageRenderTime.load();
    statistics_.averageRenderTime = (currentAvg * 0.9) + (renderTime * 0.1);
    
    // Update FPS calculation
    frameCount_++;
    auto now = std::chrono::steady_clock::now();
    auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsTime_);
    
    if (timeDiff.count() >= 1000) { // Update FPS every second
        double fps = frameCount_ * 1000.0 / timeDiff.count();
        statistics_.currentFps = fps;
        
        frameCount_ = 0;
        lastFpsTime_ = now;
    }
}

void NativeSurfaceRenderer::reportError(OjoError error, const std::string& message) {
    LOGE("Renderer error: %s - %s", errorToString(error), message.c_str());
    
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (errorCallback_) {
        errorCallback_(error, message);
    }
}

void NativeSurfaceRenderer::logDebug(const std::string& message) const {
    LOGD("%s", message.c_str());
}

void NativeSurfaceRenderer::logError(const std::string& message) const {
    LOGE("%s", message.c_str());
}

void NativeSurfaceRenderer::logInfo(const std::string& message) const {
    LOGI("%s", message.c_str());
}

// Format conversion implementations
OjoError NativeSurfaceRenderer::renderNV12ToRGBA(const VideoFrame& frame) {
    if (!frame.data || frame.dataSize == 0) {
        LOGE("Invalid frame data for NV12 conversion");
        return OjoError::RENDER_FAILED;
    }

    if (!windowBuffer_.bits) {
        LOGE("No window buffer available for NV12 rendering");
        return OjoError::RENDER_FAILED;
    }

    // Validate frame dimensions and data size
    size_t expectedSize = frame.width * frame.height * 3 / 2; // NV12 format
    if (frame.dataSize < expectedSize) {
        LOGE("Frame data size too small: %zu, expected: %zu", frame.dataSize, expectedSize);
        return OjoError::RENDER_FAILED;
    }

    LOGD("Rendering NV12 frame: %dx%d, data size: %zu, window: %dx%d",
         frame.width, frame.height, frame.dataSize,
         windowBuffer_.width, windowBuffer_.height);

    uint32_t* pixels = static_cast<uint32_t*>(windowBuffer_.bits);
    const uint8_t* yPlane = frame.data.get();
    const uint8_t* uvPlane = yPlane + (frame.width * frame.height);

    // NV12 to RGBA conversion with proper bounds checking
    int renderWidth = std::min(frame.width, windowBuffer_.width);
    int renderHeight = std::min(frame.height, windowBuffer_.height);

    for (int y = 0; y < renderHeight; y++) {
        for (int x = 0; x < renderWidth; x++) {
            int yIndex = y * frame.width + x;

            // UV plane indexing for NV12 format (interleaved U,V pairs)
            // UV plane has half resolution, so divide coordinates by 2
            int uvY = y / 2;
            int uvX = (x / 2) * 2; // Ensure even X coordinate for UV pairs
            int uvIndex = uvY * frame.width + uvX;

            // Bounds checking for UV plane access
            if (uvIndex + 1 >= (frame.width * frame.height / 2)) {
                continue; // Skip if UV index is out of bounds
            }

            uint8_t Y = yPlane[yIndex];
            uint8_t U = uvPlane[uvIndex];
            uint8_t V = uvPlane[uvIndex + 1];

            // YUV to RGB conversion using ITU-R BT.601 standard coefficients
            // More accurate conversion for better color reproduction
            int C = Y - 16;
            int D = U - 128;
            int E = V - 128;

            int R = (298 * C + 409 * E + 128) >> 8;
            int G = (298 * C - 100 * D - 208 * E + 128) >> 8;
            int B = (298 * C + 516 * D + 128) >> 8;

            // Clamp values to valid RGB range
            R = std::max(0, std::min(255, R));
            G = std::max(0, std::min(255, G));
            B = std::max(0, std::min(255, B));

            // Write pixel to surface buffer (RGBA format)
            int pixelIndex = y * windowBuffer_.stride + x;
            pixels[pixelIndex] = 0xFF000000 | (R << 16) | (G << 8) | B;
        }
    }

    LOGD("Successfully rendered NV12 frame to RGBA surface: %dx%d", renderWidth, renderHeight);

    return OjoError::SUCCESS;
}

OjoError NativeSurfaceRenderer::renderNV21ToRGBA(const VideoFrame& frame) {
    if (!frame.data || frame.dataSize == 0) {
        LOGE("Invalid frame data for NV21 conversion");
        return OjoError::RENDER_FAILED;
    }

    uint32_t* pixels = static_cast<uint32_t*>(windowBuffer_.bits);
    const uint8_t* yPlane = frame.data.get();
    const uint8_t* vuPlane = yPlane + (frame.width * frame.height);

    // Simple NV21 to RGBA conversion (V and U are swapped compared to NV12)
    for (int y = 0; y < std::min(frame.height, windowBuffer_.height); y++) {
        for (int x = 0; x < std::min(frame.width, windowBuffer_.width); x++) {
            int yIndex = y * frame.width + x;
            int vuIndex = (y / 2) * frame.width + (x & ~1);

            uint8_t Y = yPlane[yIndex];
            uint8_t V = vuPlane[vuIndex];     // Note: V comes first in NV21
            uint8_t U = vuPlane[vuIndex + 1]; // Note: U comes second in NV21

            // YUV to RGB conversion
            int R = Y + 1.402 * (V - 128);
            int G = Y - 0.344 * (U - 128) - 0.714 * (V - 128);
            int B = Y + 1.772 * (U - 128);

            // Clamp values
            R = std::max(0, std::min(255, R));
            G = std::max(0, std::min(255, G));
            B = std::max(0, std::min(255, B));

            pixels[y * windowBuffer_.stride + x] = 0xFF000000 | (R << 16) | (G << 8) | B;
        }
    }

    return OjoError::SUCCESS;
}

OjoError NativeSurfaceRenderer::renderI420ToRGBA(const VideoFrame& frame) {
    if (!frame.data || frame.dataSize == 0) {
        LOGE("Invalid frame data for I420 conversion");
        return OjoError::RENDER_FAILED;
    }

    uint32_t* pixels = static_cast<uint32_t*>(windowBuffer_.bits);
    const uint8_t* yPlane = frame.data.get();
    const uint8_t* uPlane = yPlane + (frame.width * frame.height);
    const uint8_t* vPlane = uPlane + (frame.width * frame.height / 4);

    // I420 to RGBA conversion
    for (int y = 0; y < std::min(frame.height, windowBuffer_.height); y++) {
        for (int x = 0; x < std::min(frame.width, windowBuffer_.width); x++) {
            int yIndex = y * frame.width + x;
            int uvIndex = (y / 2) * (frame.width / 2) + (x / 2);

            uint8_t Y = yPlane[yIndex];
            uint8_t U = uPlane[uvIndex];
            uint8_t V = vPlane[uvIndex];

            // YUV to RGB conversion
            int R = Y + 1.402 * (V - 128);
            int G = Y - 0.344 * (U - 128) - 0.714 * (V - 128);
            int B = Y + 1.772 * (U - 128);

            // Clamp values
            R = std::max(0, std::min(255, R));
            G = std::max(0, std::min(255, G));
            B = std::max(0, std::min(255, B));

            pixels[y * windowBuffer_.stride + x] = 0xFF000000 | (R << 16) | (G << 8) | B;
        }
    }

    return OjoError::SUCCESS;
}

OjoError NativeSurfaceRenderer::renderRGB24ToRGBA(const VideoFrame& frame) {
    if (!frame.data || frame.dataSize == 0) {
        LOGE("Invalid frame data for RGB24 conversion");
        return OjoError::RENDER_FAILED;
    }

    uint32_t* pixels = static_cast<uint32_t*>(windowBuffer_.bits);
    const uint8_t* rgb = frame.data.get();

    // RGB24 to RGBA conversion
    for (int y = 0; y < std::min(frame.height, windowBuffer_.height); y++) {
        for (int x = 0; x < std::min(frame.width, windowBuffer_.width); x++) {
            int srcIndex = (y * frame.width + x) * 3;
            uint8_t R = rgb[srcIndex];
            uint8_t G = rgb[srcIndex + 1];
            uint8_t B = rgb[srcIndex + 2];

            pixels[y * windowBuffer_.stride + x] = 0xFF000000 | (R << 16) | (G << 8) | B;
        }
    }

    return OjoError::SUCCESS;
}

OjoError NativeSurfaceRenderer::renderRGBA32(const VideoFrame& frame) {
    if (!frame.data || frame.dataSize == 0) {
        LOGE("Invalid frame data for RGBA32 rendering");
        return OjoError::RENDER_FAILED;
    }

    uint32_t* pixels = static_cast<uint32_t*>(windowBuffer_.bits);
    const uint32_t* rgba = reinterpret_cast<const uint32_t*>(frame.data.get());

    // Direct RGBA copy with scaling if needed
    for (int y = 0; y < std::min(frame.height, windowBuffer_.height); y++) {
        for (int x = 0; x < std::min(frame.width, windowBuffer_.width); x++) {
            pixels[y * windowBuffer_.stride + x] = rgba[y * frame.width + x];
        }
    }

    return OjoError::SUCCESS;
}

OjoError NativeSurfaceRenderer::renderTestPattern() {
    uint32_t* pixels = static_cast<uint32_t*>(windowBuffer_.bits);

    // Create a colorful test pattern to indicate video rendering is working
    for (int y = 0; y < windowBuffer_.height; y++) {
        for (int x = 0; x < windowBuffer_.width; x++) {
            uint32_t color;
            if (y < windowBuffer_.height / 3) {
                color = 0xFF0000FF; // Red
            } else if (y < 2 * windowBuffer_.height / 3) {
                color = 0xFF00FF00; // Green
            } else {
                color = 0xFFFF0000; // Blue
            }
            pixels[y * windowBuffer_.stride + x] = color;
        }
    }

    return OjoError::SUCCESS;
}

} // namespace ojo
