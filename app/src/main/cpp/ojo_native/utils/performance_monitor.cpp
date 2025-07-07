#include "performance_monitor.h"
#include <android/log.h>
#include <fstream>
#include <sstream>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "OjoPerformanceMonitor"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace ojo {

PerformanceMonitor::PerformanceMonitor()
    : running_(false), monitoringInterval_(1000) {
    
    LOGD("Performance monitor created");
}

PerformanceMonitor::~PerformanceMonitor() {
    LOGD("Performance monitor destructor called");
    stop();
}

void PerformanceMonitor::start(int intervalMs) {
    if (running_) {
        LOGW("Performance monitor already running");
        return;
    }
    
    monitoringInterval_ = intervalMs;
    running_ = true;
    
    monitorThread_ = std::thread(&PerformanceMonitor::monitoringLoop, this);
    
    LOGI("Performance monitor started (interval: %dms)", intervalMs);
}

void PerformanceMonitor::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }
    
    LOGI("Performance monitor stopped");
}

bool PerformanceMonitor::isRunning() const {
    return running_;
}

void PerformanceMonitor::recordFrameReceived(int streamId) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    streamMetrics_[streamId].framesReceived++;
    streamMetrics_[streamId].lastFrameTimestamp = 
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    
    updateStreamFps(streamId);
}

void PerformanceMonitor::recordFrameDecoded(int streamId, double decodeTime) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    auto& metrics = streamMetrics_[streamId];
    metrics.framesDecoded++;
    
    // Update average decode time (exponential moving average)
    double currentAvg = metrics.averageDecodeTime.load();
    metrics.averageDecodeTime = (currentAvg * 0.9) + (decodeTime * 0.1);
}

void PerformanceMonitor::recordFrameRendered(int streamId, double renderTime) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    auto& metrics = streamMetrics_[streamId];
    metrics.framesRendered++;
    
    // Update average render time (exponential moving average)
    double currentAvg = metrics.averageRenderTime.load();
    metrics.averageRenderTime = (currentAvg * 0.9) + (renderTime * 0.1);
}

void PerformanceMonitor::recordFrameDropped(int streamId) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    streamMetrics_[streamId].framesDropped++;
}

void PerformanceMonitor::recordNetworkBytes(int streamId, size_t bytes) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    streamMetrics_[streamId].networkBytes += bytes;
}

PerformanceMonitor::StreamMetrics PerformanceMonitor::getStreamMetrics(int streamId) const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    auto it = streamMetrics_.find(streamId);
    if (it != streamMetrics_.end()) {
        return it->second;
    }
    return StreamMetrics{};
}

std::map<int, PerformanceMonitor::StreamMetrics> PerformanceMonitor::getAllStreamMetrics() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    return streamMetrics_;
}

PerformanceMonitor::SystemMetrics PerformanceMonitor::getSystemMetrics() const {
    SystemMetrics metrics;
    metrics.cpuUsage = getCpuUsage();
    metrics.memoryUsage = getMemoryUsage();
    metrics.networkBandwidth = getNetworkBandwidth();
    metrics.activeStreams = streamMetrics_.size();
    metrics.timestamp = std::chrono::steady_clock::now();
    
    return metrics;
}

void PerformanceMonitor::setMetricsCallback(MetricsCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    metricsCallback_ = callback;
}

void PerformanceMonitor::setAlertCallback(AlertCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    alertCallback_ = callback;
}

void PerformanceMonitor::setFrameDropThreshold(double percentage) {
    frameDropThreshold_ = percentage;
}

void PerformanceMonitor::setCpuUsageThreshold(double percentage) {
    cpuUsageThreshold_ = percentage;
}

void PerformanceMonitor::setMemoryUsageThreshold(double percentage) {
    memoryUsageThreshold_ = percentage;
}

void PerformanceMonitor::monitoringLoop() {
    LOGD("Performance monitoring loop started");
    
    while (running_) {
        try {
            auto streamMetrics = getAllStreamMetrics();
            auto systemMetrics = getSystemMetrics();
            
            // Check for alerts
            checkAlerts(streamMetrics, systemMetrics);
            
            // Call metrics callback
            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (metricsCallback_) {
                metricsCallback_(streamMetrics, systemMetrics);
            }
            
        } catch (const std::exception& e) {
            LOGE("Exception in monitoring loop: %s", e.what());
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(monitoringInterval_));
    }
    
    LOGD("Performance monitoring loop ended");
}

void PerformanceMonitor::updateStreamFps(int streamId) {
    // Calculate FPS based on frame timestamps
    // This is a simplified implementation
    auto& metrics = streamMetrics_[streamId];
    
    static std::map<int, std::chrono::steady_clock::time_point> lastFpsUpdate;
    static std::map<int, uint64_t> lastFrameCount;
    
    auto now = std::chrono::steady_clock::now();
    auto lastUpdate = lastFpsUpdate[streamId];
    auto lastCount = lastFrameCount[streamId];
    
    if (lastUpdate != std::chrono::steady_clock::time_point{}) {
        auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate);
        if (timeDiff.count() >= 1000) { // Update every second
            uint64_t currentCount = metrics.framesReceived.load();
            uint64_t frameDiff = currentCount - lastCount;
            double fps = frameDiff * 1000.0 / timeDiff.count();
            
            metrics.currentFps = fps;
            lastFpsUpdate[streamId] = now;
            lastFrameCount[streamId] = currentCount;
        }
    } else {
        lastFpsUpdate[streamId] = now;
        lastFrameCount[streamId] = metrics.framesReceived.load();
    }
}

void PerformanceMonitor::checkAlerts(const std::map<int, StreamMetrics>& streamMetrics, 
                                   const SystemMetrics& systemMetrics) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    
    if (!alertCallback_) {
        return;
    }
    
    // Check CPU usage
    if (systemMetrics.cpuUsage > cpuUsageThreshold_) {
        std::ostringstream oss;
        oss << "High CPU usage: " << systemMetrics.cpuUsage << "%";
        alertCallback_("CPU_USAGE", oss.str());
    }
    
    // Check memory usage
    if (systemMetrics.memoryUsage > memoryUsageThreshold_) {
        std::ostringstream oss;
        oss << "High memory usage: " << systemMetrics.memoryUsage << "%";
        alertCallback_("MEMORY_USAGE", oss.str());
    }
    
    // Check frame drop rates
    for (const auto& pair : streamMetrics) {
        int streamId = pair.first;
        const auto& metrics = pair.second;
        
        uint64_t totalFrames = metrics.framesReceived.load();
        uint64_t droppedFrames = metrics.framesDropped.load();
        
        if (totalFrames > 0) {
            double dropRate = (droppedFrames * 100.0) / totalFrames;
            if (dropRate > frameDropThreshold_) {
                std::ostringstream oss;
                oss << "High frame drop rate for stream " << streamId << ": " << dropRate << "%";
                alertCallback_("FRAME_DROP", oss.str());
            }
        }
    }
}

double PerformanceMonitor::getCpuUsage() const {
    // TODO: Implement actual CPU usage monitoring
    // This would read from /proc/stat or similar
    return 0.0;
}

double PerformanceMonitor::getMemoryUsage() const {
    // TODO: Implement actual memory usage monitoring
    // This would read from /proc/meminfo or similar
    return 0.0;
}

double PerformanceMonitor::getNetworkBandwidth() const {
    // TODO: Implement actual network bandwidth monitoring
    // This would read from /proc/net/dev or similar
    return 0.0;
}

void PerformanceMonitor::logDebug(const std::string& message) const {
    LOGD("%s", message.c_str());
}

void PerformanceMonitor::logError(const std::string& message) const {
    LOGE("%s", message.c_str());
}

void PerformanceMonitor::logInfo(const std::string& message) const {
    LOGI("%s", message.c_str());
}

} // namespace ojo
