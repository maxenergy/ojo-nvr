#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include "ojo_types.h"
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <string>

namespace ojo {

/**
 * Performance monitor for RTSP streaming system
 */
class PerformanceMonitor {
public:
    PerformanceMonitor();
    ~PerformanceMonitor();
    
    // Monitoring control
    void start(int intervalMs = 1000);
    void stop();
    bool isRunning() const;
    
    // Metrics collection
    void recordFrameReceived(int streamId);
    void recordFrameDecoded(int streamId, double decodeTime);
    void recordFrameRendered(int streamId, double renderTime);
    void recordFrameDropped(int streamId);
    void recordNetworkBytes(int streamId, size_t bytes);
    
    // Statistics
    struct StreamMetrics {
        std::atomic<uint64_t> framesReceived{0};
        std::atomic<uint64_t> framesDecoded{0};
        std::atomic<uint64_t> framesRendered{0};
        std::atomic<uint64_t> framesDropped{0};
        std::atomic<uint64_t> networkBytes{0};
        std::atomic<double> averageDecodeTime{0.0};
        std::atomic<double> averageRenderTime{0.0};
        std::atomic<double> currentFps{0.0};
        std::atomic<int64_t> lastFrameTimestamp{0};

        // Copy constructor for atomic values
        StreamMetrics() = default;
        StreamMetrics(const StreamMetrics& other) {
            framesReceived.store(other.framesReceived.load());
            framesDecoded.store(other.framesDecoded.load());
            framesRendered.store(other.framesRendered.load());
            framesDropped.store(other.framesDropped.load());
            networkBytes.store(other.networkBytes.load());
            averageDecodeTime.store(other.averageDecodeTime.load());
            averageRenderTime.store(other.averageRenderTime.load());
            currentFps.store(other.currentFps.load());
            lastFrameTimestamp.store(other.lastFrameTimestamp.load());
        }

        StreamMetrics& operator=(const StreamMetrics& other) {
            if (this != &other) {
                framesReceived.store(other.framesReceived.load());
                framesDecoded.store(other.framesDecoded.load());
                framesRendered.store(other.framesRendered.load());
                framesDropped.store(other.framesDropped.load());
                networkBytes.store(other.networkBytes.load());
                averageDecodeTime.store(other.averageDecodeTime.load());
                averageRenderTime.store(other.averageRenderTime.load());
                currentFps.store(other.currentFps.load());
                lastFrameTimestamp.store(other.lastFrameTimestamp.load());
            }
            return *this;
        }

        void reset() {
            framesReceived = 0;
            framesDecoded = 0;
            framesRendered = 0;
            framesDropped = 0;
            networkBytes = 0;
            averageDecodeTime = 0.0;
            averageRenderTime = 0.0;
            currentFps = 0.0;
            lastFrameTimestamp = 0;
        }
    };
    
    StreamMetrics getStreamMetrics(int streamId) const;
    std::map<int, StreamMetrics> getAllStreamMetrics() const;
    
    // System metrics
    struct SystemMetrics {
        double cpuUsage;
        double memoryUsage;
        double networkBandwidth;
        int activeStreams;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    SystemMetrics getSystemMetrics() const;
    
    // Callbacks
    using MetricsCallback = std::function<void(const std::map<int, StreamMetrics>&, const SystemMetrics&)>;
    void setMetricsCallback(MetricsCallback callback);
    
    // Alert thresholds
    void setFrameDropThreshold(double percentage);
    void setCpuUsageThreshold(double percentage);
    void setMemoryUsageThreshold(double percentage);
    
    // Alert callback
    using AlertCallback = std::function<void(const std::string&, const std::string&)>;
    void setAlertCallback(AlertCallback callback);
    
private:
    // Stream metrics storage
    mutable std::mutex metricsMutex_;
    std::map<int, StreamMetrics> streamMetrics_;
    
    // Monitoring thread
    std::thread monitorThread_;
    std::atomic<bool> running_;
    int monitoringInterval_;
    
    // Callbacks
    MetricsCallback metricsCallback_;
    AlertCallback alertCallback_;
    mutable std::mutex callbackMutex_;
    
    // Alert thresholds
    std::atomic<double> frameDropThreshold_{10.0};
    std::atomic<double> cpuUsageThreshold_{80.0};
    std::atomic<double> memoryUsageThreshold_{85.0};
    
    // Internal methods
    void monitoringLoop();
    void updateStreamFps(int streamId);
    void checkAlerts(const std::map<int, StreamMetrics>& streamMetrics, const SystemMetrics& systemMetrics);
    
    // System monitoring
    double getCpuUsage() const;
    double getMemoryUsage() const;
    double getNetworkBandwidth() const;
    
    // Utility
    void logDebug(const std::string& message) const;
    void logError(const std::string& message) const;
    void logInfo(const std::string& message) const;
};

/**
 * Performance profiler for detailed timing analysis
 */
class PerformanceProfiler {
public:
    PerformanceProfiler();
    ~PerformanceProfiler();
    
    // Profiling sessions
    void startSession(const std::string& name);
    void endSession(const std::string& name);
    
    // Timing measurements
    void startTiming(const std::string& operation);
    void endTiming(const std::string& operation);
    
    // Scoped timing helper
    class ScopedTimer {
    public:
        ScopedTimer(PerformanceProfiler* profiler, const std::string& operation)
            : profiler_(profiler), operation_(operation) {
            if (profiler_) {
                profiler_->startTiming(operation_);
            }
        }
        
        ~ScopedTimer() {
            if (profiler_) {
                profiler_->endTiming(operation_);
            }
        }
        
    private:
        PerformanceProfiler* profiler_;
        std::string operation_;
    };
    
    // Results
    struct TimingResult {
        std::string operation;
        double averageTime;
        double minTime;
        double maxTime;
        uint64_t callCount;
    };
    
    std::vector<TimingResult> getResults() const;
    void clearResults();
    
    // Report generation
    void generateReport(const std::string& filename) const;
    void logReport() const;
    
private:
    struct TimingData {
        std::chrono::steady_clock::time_point startTime;
        double totalTime;
        double minTime;
        double maxTime;
        uint64_t callCount;
        bool active;
        
        TimingData() : totalTime(0.0), minTime(std::numeric_limits<double>::max()),
                      maxTime(0.0), callCount(0), active(false) {}
    };
    
    mutable std::mutex dataMutex_;
    std::map<std::string, TimingData> timingData_;
    std::string currentSession_;
    
    void updateTiming(const std::string& operation, double time);
};

// Macro for easy scoped timing
#define PROFILE_SCOPE(profiler, operation) \
    ojo::PerformanceProfiler::ScopedTimer timer(profiler, operation)

} // namespace ojo

#endif // PERFORMANCE_MONITOR_H
