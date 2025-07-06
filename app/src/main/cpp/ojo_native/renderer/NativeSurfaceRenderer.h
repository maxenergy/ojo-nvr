#ifndef NATIVE_SURFACE_RENDERER_H
#define NATIVE_SURFACE_RENDERER_H

#include "../utils/ojo_types.h"
#include <android/native_window.h>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

// Forward declarations for RGA
extern "C" {
    typedef struct {
        int fd;
        void* virAddr;
        void* phyAddr;
        int size;
    } rga_buffer_t;
    
    typedef struct {
        int x, y;
        int w, h;
        int wstride, hstride;
        int format;
    } rga_rect_t;
    
    typedef struct {
        rga_buffer_t src;
        rga_buffer_t dst;
        rga_rect_t src_rect;
        rga_rect_t dst_rect;
        int rotation;
        int blend;
    } rga_info_t;
}

namespace ojo {

/**
 * Native Surface Renderer for Ojo surveillance system
 * Provides direct hardware-accelerated rendering to Android Surface
 */
class NativeSurfaceRenderer {
public:
    NativeSurfaceRenderer();
    ~NativeSurfaceRenderer();
    
    // Initialization and cleanup
    OjoError initialize(ANativeWindow* window);
    void cleanup();
    bool isInitialized() const;
    
    // Surface management
    OjoError setSurface(ANativeWindow* window);
    void clearSurface();
    ANativeWindow* getSurface() const;
    
    // Display configuration
    OjoError setDisplayRect(int x, int y, int width, int height);
    void getDisplayRect(int& x, int& y, int& width, int& height) const;
    OjoError setDisplayMode(int mode); // 1x1, 2x2, 3x3, 4x4
    int getDisplayMode() const;
    
    // Rendering operations
    OjoError renderFrame(const VideoFrame& frame);
    OjoError renderFrameAsync(std::shared_ptr<VideoFrame> frame);
    void clearScreen();
    void clearScreen(uint32_t color);
    
    // Performance and statistics
    struct RendererStatistics {
        std::atomic<uint64_t> framesRendered{0};
        std::atomic<uint64_t> framesFailed{0};
        std::atomic<uint64_t> framesDropped{0};
        std::atomic<double> averageRenderTime{0.0};
        std::atomic<double> currentFps{0.0};
        std::atomic<int> queuedFrames{0};
        std::atomic<bool> hardwareAccelerated{false};
        
        void reset() {
            framesRendered = 0;
            framesFailed = 0;
            framesDropped = 0;
            averageRenderTime = 0.0;
            currentFps = 0.0;
            queuedFrames = 0;
            hardwareAccelerated = false;
        }
    };
    
    RendererStatistics getStatistics() const;
    void resetStatistics();
    
    // Configuration
    void setMaxQueueSize(int maxSize);
    int getMaxQueueSize() const;
    void setVSyncEnabled(bool enabled);
    bool isVSyncEnabled() const;
    
    // Error handling
    using ErrorCallback = std::function<void(OjoError, const std::string&)>;
    void setErrorCallback(ErrorCallback callback);
    
    // Frame callback for rendered frames
    using FrameRenderedCallback = std::function<void(const VideoFrame&)>;
    void setFrameRenderedCallback(FrameRenderedCallback callback);
    
    // Control
    void start();
    void stop();
    void pause();
    void resume();
    bool isRunning() const;
    
private:
    // Surface and window management
    ANativeWindow* nativeWindow_;
    ANativeWindow_Buffer windowBuffer_;
    std::mutex surfaceMutex_;
    
    // Display configuration
    struct DisplayConfig {
        int x, y;
        int width, height;
        int mode;
        bool vsyncEnabled;
        
        DisplayConfig() : x(0), y(0), width(1280), height(720), mode(1), vsyncEnabled(true) {}
    } displayConfig_;
    
    // RGA hardware acceleration
    bool rgaAvailable_;
    void* rgaContext_;
    std::mutex rgaMutex_;
    
    // Threading and frame queue
    std::thread renderThread_;
    std::atomic<bool> shouldStop_;
    std::atomic<bool> isPaused_;
    SafeQueue<std::shared_ptr<VideoFrame>> frameQueue_;
    int maxQueueSize_;
    
    // Statistics and monitoring
    RendererStatistics statistics_;
    std::chrono::steady_clock::time_point lastRenderTime_;
    std::chrono::steady_clock::time_point lastFpsTime_;
    int frameCount_;
    
    // Callbacks
    ErrorCallback errorCallback_;
    FrameRenderedCallback frameRenderedCallback_;
    std::mutex callbackMutex_;
    
    // State management
    std::atomic<bool> initialized_;
    std::atomic<bool> running_;
    
    // Internal methods
    void renderLoop();
    OjoError renderFrameInternal(const VideoFrame& frame);
    OjoError renderFrameWithRGA(const VideoFrame& frame);
    OjoError renderFrameSoftware(const VideoFrame& frame);
    
    // Surface operations
    OjoError lockSurface();
    void unlockSurface();
    OjoError configureSurface();
    void releaseSurface();
    
    // RGA operations
    bool initializeRGA();
    void cleanupRGA();
    OjoError convertFrameWithRGA(const VideoFrame& input, ANativeWindow_Buffer& output);
    OjoError scaleFrameWithRGA(const VideoFrame& input, ANativeWindow_Buffer& output);
    
    // Format conversion
    OjoError convertYUV420ToRGBA(const VideoFrame& input, ANativeWindow_Buffer& output);
    OjoError convertNV12ToRGBA(const VideoFrame& input, ANativeWindow_Buffer& output);
    OjoError convertRGB24ToRGBA(const VideoFrame& input, ANativeWindow_Buffer& output);
    
    // Utility methods
    bool isFormatSupported(VideoFormat format) const;
    int getAndroidFormat(VideoFormat format) const;
    void calculateDisplayRect(int& x, int& y, int& width, int& height) const;
    void updateStatistics(double renderTime);
    void reportError(OjoError error, const std::string& message);
    
    // Logging
    void logDebug(const std::string& message) const;
    void logError(const std::string& message) const;
    void logInfo(const std::string& message) const;
};

/**
 * Multi-Stream Renderer - manages rendering for multiple video streams
 */
class MultiStreamRenderer {
public:
    MultiStreamRenderer();
    ~MultiStreamRenderer();
    
    // Stream management
    OjoError addStream(int streamId, ANativeWindow* window);
    OjoError removeStream(int streamId);
    OjoError renderFrame(int streamId, const VideoFrame& frame);
    
    // Layout management
    enum class LayoutMode {
        SINGLE = 1,
        QUAD = 4,
        NINE = 9,
        SIXTEEN = 16
    };
    
    OjoError setLayoutMode(LayoutMode mode);
    LayoutMode getLayoutMode() const;
    OjoError setStreamPosition(int streamId, int position);
    
    // Global configuration
    void setGlobalDisplayRect(int x, int y, int width, int height);
    void setVSyncEnabled(bool enabled);
    
    // Statistics
    struct MultiStreamStatistics {
        std::map<int, NativeSurfaceRenderer::RendererStatistics> streamStats;
        std::atomic<int> activeStreams{0};
        std::atomic<double> totalFps{0.0};
        
        void reset() {
            streamStats.clear();
            activeStreams = 0;
            totalFps = 0.0;
        }
    };
    
    MultiStreamStatistics getStatistics() const;
    
    // Control
    void startAll();
    void stopAll();
    void pauseAll();
    void resumeAll();
    
private:
    // Stream renderers
    std::map<int, std::unique_ptr<NativeSurfaceRenderer>> renderers_;
    std::mutex renderersMutex_;
    
    // Layout configuration
    LayoutMode layoutMode_;
    std::map<int, int> streamPositions_;
    struct {
        int x, y, width, height;
    } globalDisplayRect_;
    
    // Internal methods
    void calculateStreamRect(int position, int& x, int& y, int& width, int& height) const;
    void updateStreamLayouts();
    
    // Utility
    void logDebug(const std::string& message) const;
    void logError(const std::string& message) const;
};

} // namespace ojo

#endif // NATIVE_SURFACE_RENDERER_H
