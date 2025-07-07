#include "thread_pool.h"
#include <android/log.h>
#include <algorithm>
#include <cinttypes>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "OjoThreadPool"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace ojo {

// ThreadPool implementation
ThreadPool::ThreadPool(size_t numThreads)
    : shutdown_(false), activeThreads_(0) {
    
    LOGI("Creating thread pool with %zu threads", numThreads);
    
    statistics_.reset();
    
    // Create worker threads
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back(&ThreadPool::workerLoop, this);
    }
    
    LOGI("Thread pool created successfully");
}

ThreadPool::~ThreadPool() {
    LOGI("Destroying thread pool");
    shutdown();
}

void ThreadPool::shutdown() {
    if (shutdown_) {
        return;
    }
    
    LOGI("Shutting down thread pool");
    
    {
        std::unique_lock<std::mutex> lock(tasksMutex_);
        shutdown_ = true;
    }
    
    condition_.notify_all();
    
    // Wait for all threads to finish
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    workers_.clear();
    
    LOGI("Thread pool shutdown completed");
}

void ThreadPool::waitForAll() {
    std::unique_lock<std::mutex> lock(tasksMutex_);
    condition_.wait(lock, [this] {
        return tasks_.empty() && activeThreads_ == 0;
    });
}

size_t ThreadPool::getThreadCount() const {
    return workers_.size();
}

size_t ThreadPool::getQueueSize() const {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    return tasks_.size();
}

size_t ThreadPool::getActiveThreads() const {
    return activeThreads_;
}

bool ThreadPool::isShutdown() const {
    return shutdown_;
}

ThreadPool::ThreadPoolStatistics ThreadPool::getStatistics() const {
    return statistics_;
}

void ThreadPool::resetStatistics() {
    statistics_.reset();
}

void ThreadPool::workerLoop() {
    LOGD("Worker thread started");
    
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(tasksMutex_);
            
            condition_.wait(lock, [this] {
                return shutdown_ || !tasks_.empty();
            });
            
            if (shutdown_ && tasks_.empty()) {
                break;
            }
            
            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
                activeThreads_++;
            }
        }
        
        if (task) {
            auto startTime = std::chrono::steady_clock::now();
            
            try {
                task();
                statistics_.tasksCompleted++;
            } catch (const std::exception& e) {
                LOGE("Exception in worker thread: %s", e.what());
                statistics_.tasksFailed++;
            }
            
            auto endTime = std::chrono::steady_clock::now();
            auto executionTime = std::chrono::duration_cast<std::chrono::microseconds>(
                endTime - startTime).count() / 1000.0;
            
            updateStatistics(executionTime);
            
            activeThreads_--;
        }
    }
    
    LOGD("Worker thread ended");
}

void ThreadPool::updateStatistics(double executionTime) {
    // Update average execution time (exponential moving average)
    double currentAvg = statistics_.averageExecutionTime.load();
    statistics_.averageExecutionTime = (currentAvg * 0.9) + (executionTime * 0.1);
}

// RTSPStreamThreadPool implementation
RTSPStreamThreadPool::RTSPStreamThreadPool(size_t numThreads)
    : maxQueueSize_(1000)
    , threadAffinityEnabled_(false)
    , shutdown_(false) {
    
    LOGI("Creating RTSP stream thread pool with %zu threads", numThreads);
    
    // Create specialized thread pools
    receivePool_ = std::make_unique<ThreadPool>(std::max(size_t(1), numThreads / 4));
    decodePool_ = std::make_unique<ThreadPool>(std::max(size_t(1), numThreads / 2));
    renderPool_ = std::make_unique<ThreadPool>(std::max(size_t(1), numThreads / 4));
    utilityPool_ = std::make_unique<ThreadPool>(1);
    
    rtspStatistics_.reset();
    
    // Start priority dispatcher
    priorityDispatcher_ = std::thread(&RTSPStreamThreadPool::priorityDispatcherLoop, this);
    
    LOGI("RTSP stream thread pool created successfully");
}

RTSPStreamThreadPool::~RTSPStreamThreadPool() {
    LOGI("Destroying RTSP stream thread pool");
    
    shutdown_ = true;
    priorityCondition_.notify_all();
    
    if (priorityDispatcher_.joinable()) {
        priorityDispatcher_.join();
    }
    
    // Shutdown thread pools
    receivePool_.reset();
    decodePool_.reset();
    renderPool_.reset();
    utilityPool_.reset();
    
    LOGI("RTSP stream thread pool destroyed");
}

void RTSPStreamThreadPool::setStreamPriority(int streamId, int priority) {
    std::lock_guard<std::mutex> lock(streamMutex_);
    streamPriorities_[streamId] = priority;
    LOGD("Set stream %d priority to %d", streamId, priority);
}

void RTSPStreamThreadPool::pauseStream(int streamId) {
    std::lock_guard<std::mutex> lock(streamMutex_);
    pausedStreams_.insert(streamId);
    LOGD("Paused stream %d", streamId);
}

void RTSPStreamThreadPool::resumeStream(int streamId) {
    std::lock_guard<std::mutex> lock(streamMutex_);
    pausedStreams_.erase(streamId);
    LOGD("Resumed stream %d", streamId);
}

void RTSPStreamThreadPool::removeStream(int streamId) {
    std::lock_guard<std::mutex> lock(streamMutex_);
    streamPriorities_.erase(streamId);
    pausedStreams_.erase(streamId);
    LOGD("Removed stream %d", streamId);
}

void RTSPStreamThreadPool::setMaxQueueSize(size_t maxSize) {
    maxQueueSize_ = maxSize;
    LOGD("Set max queue size to %zu", maxSize);
}

void RTSPStreamThreadPool::setThreadAffinity(bool enable) {
    threadAffinityEnabled_ = enable;
    LOGD("Thread affinity %s", enable ? "enabled" : "disabled");
}

RTSPStreamThreadPool::RTSPThreadPoolStatistics RTSPStreamThreadPool::getRTSPStatistics() const {
    return rtspStatistics_;
}

void RTSPStreamThreadPool::priorityDispatcherLoop() {
    LOGD("Priority dispatcher loop started");
    
    while (!shutdown_) {
        PriorityTask task;
        
        {
            std::unique_lock<std::mutex> lock(priorityMutex_);
            
            priorityCondition_.wait(lock, [this] {
                return shutdown_ || !priorityTasks_.empty();
            });
            
            if (shutdown_) {
                break;
            }
            
            if (!priorityTasks_.empty()) {
                task = priorityTasks_.top();
                priorityTasks_.pop();
            } else {
                continue;
            }
        }
        
        // Check if stream is paused
        {
            std::lock_guard<std::mutex> streamLock(streamMutex_);
            if (pausedStreams_.find(task.streamId) != pausedStreams_.end()) {
                continue; // Skip paused stream tasks
            }
        }
        
        // Dispatch to appropriate thread pool
        ThreadPool* pool = getPoolForTaskType(task.type);
        if (pool && !pool->isShutdown()) {
            try {
                auto future = pool->submit(task.task);
                
                // Update statistics
                auto now = std::chrono::steady_clock::now();
                auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                    now - task.submitTime).count() / 1000.0;
                updateRTSPStatistics(task.type, task.streamId, latency);
                
            } catch (const std::exception& e) {
                LOGE("Failed to submit task: %s", e.what());
                rtspStatistics_.droppedTasks++;
            }
        }
    }
    
    LOGD("Priority dispatcher loop ended");
}

ThreadPool* RTSPStreamThreadPool::getPoolForTaskType(TaskType type) {
    switch (type) {
        case TaskType::RTSP_RECEIVE:
            return receivePool_.get();
        case TaskType::VIDEO_DECODE:
            return decodePool_.get();
        case TaskType::FRAME_RENDER:
            return renderPool_.get();
        case TaskType::STATISTICS_UPDATE:
            return utilityPool_.get();
        default:
            return utilityPool_.get();
    }
}

void RTSPStreamThreadPool::updateRTSPStatistics(TaskType type, int streamId, double executionTime) {
    rtspStatistics_.taskCounts[type]++;
    rtspStatistics_.streamTaskCounts[streamId]++;
    
    // Update average latency (exponential moving average)
    double currentAvg = rtspStatistics_.averageLatency.load();
    rtspStatistics_.averageLatency = (currentAvg * 0.9) + (executionTime * 0.1);
}

// ThreadPoolMonitor implementation
ThreadPoolMonitor::ThreadPoolMonitor()
    : monitoring_(false), monitoringInterval_(5000) {
    
    LOGD("Thread pool monitor created");
}

ThreadPoolMonitor::~ThreadPoolMonitor() {
    LOGD("Thread pool monitor destructor called");
    stopMonitoring();
}

void ThreadPoolMonitor::addThreadPool(const std::string& name, ThreadPool* pool) {
    std::lock_guard<std::mutex> lock(poolsMutex_);
    threadPools_[name] = pool;
    LOGD("Added thread pool: %s", name.c_str());
}

void ThreadPoolMonitor::addRTSPThreadPool(const std::string& name, RTSPStreamThreadPool* pool) {
    std::lock_guard<std::mutex> lock(poolsMutex_);
    rtspThreadPools_[name] = pool;
    LOGD("Added RTSP thread pool: %s", name.c_str());
}

void ThreadPoolMonitor::removeThreadPool(const std::string& name) {
    std::lock_guard<std::mutex> lock(poolsMutex_);
    threadPools_.erase(name);
    rtspThreadPools_.erase(name);
    LOGD("Removed thread pool: %s", name.c_str());
}

ThreadPoolMonitor::PerformanceReport ThreadPoolMonitor::generateReport() const {
    PerformanceReport report;
    report.timestamp = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> lock(poolsMutex_);
    
    // Collect thread pool statistics
    for (const auto& pair : threadPools_) {
        if (pair.second) {
            report.threadPoolStats[pair.first] = pair.second->getStatistics();
        }
    }
    
    // Collect RTSP thread pool statistics
    for (const auto& pair : rtspThreadPools_) {
        if (pair.second) {
            report.rtspPoolStats[pair.first] = pair.second->getRTSPStatistics();
        }
    }
    
    // Get system statistics (placeholder)
    report.systemCpuUsage = getSystemCpuUsage();
    report.systemMemoryUsage = getSystemMemoryUsage();
    
    return report;
}

void ThreadPoolMonitor::logReport(const PerformanceReport& report) const {
    LOGI("=== Thread Pool Performance Report ===");
    
    for (const auto& pair : report.threadPoolStats) {
        const auto& stats = pair.second;
        LOGI("Pool %s: submitted=%" PRIu64 ", completed=%" PRIu64 ", failed=%" PRIu64 ", avgTime=%.2fms",
             pair.first.c_str(),
             stats.tasksSubmitted.load(),
             stats.tasksCompleted.load(),
             stats.tasksFailed.load(),
             stats.averageExecutionTime.load());
    }
    
    for (const auto& pair : report.rtspPoolStats) {
        const auto& stats = pair.second;
        LOGI("RTSP Pool %s: avgLatency=%.2fms, dropped=%" PRIu64,
             pair.first.c_str(),
             stats.averageLatency.load(),
             stats.droppedTasks.load());
    }
    
    LOGI("System: CPU=%.1f%%, Memory=%.1f%%",
         report.systemCpuUsage, report.systemMemoryUsage);
    LOGI("=====================================");
}

void ThreadPoolMonitor::startMonitoring(int intervalMs) {
    if (monitoring_) {
        LOGW("Monitoring already started");
        return;
    }
    
    monitoringInterval_ = intervalMs;
    monitoring_ = true;
    
    monitorThread_ = std::thread(&ThreadPoolMonitor::monitoringLoop, this);
    
    LOGI("Started thread pool monitoring (interval: %dms)", intervalMs);
}

void ThreadPoolMonitor::stopMonitoring() {
    if (!monitoring_) {
        return;
    }
    
    monitoring_ = false;
    
    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }
    
    LOGI("Stopped thread pool monitoring");
}

void ThreadPoolMonitor::setAlertCallback(AlertCallback callback) {
    std::lock_guard<std::mutex> lock(alertMutex_);
    alertCallback_ = callback;
}

void ThreadPoolMonitor::monitoringLoop() {
    LOGD("Monitoring loop started");
    
    while (monitoring_) {
        try {
            PerformanceReport report = generateReport();
            logReport(report);
            checkAlerts(report);
            
        } catch (const std::exception& e) {
            LOGE("Exception in monitoring loop: %s", e.what());
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(monitoringInterval_));
    }
    
    LOGD("Monitoring loop ended");
}

void ThreadPoolMonitor::checkAlerts(const PerformanceReport& report) {
    std::lock_guard<std::mutex> lock(alertMutex_);
    
    if (!alertCallback_) {
        return;
    }
    
    // Check for high CPU usage
    if (report.systemCpuUsage > 80.0) {
        alertCallback_("High CPU Usage", "System CPU usage is above 80%");
    }
    
    // Check for high memory usage
    if (report.systemMemoryUsage > 85.0) {
        alertCallback_("High Memory Usage", "System memory usage is above 85%");
    }
    
    // Check for high task failure rates
    for (const auto& pair : report.threadPoolStats) {
        const auto& stats = pair.second;
        uint64_t total = stats.tasksSubmitted.load();
        uint64_t failed = stats.tasksFailed.load();
        
        if (total > 0 && (failed * 100 / total) > 10) {
            alertCallback_("High Task Failure Rate", 
                          "Thread pool " + pair.first + " has >10% task failure rate");
        }
    }
}

double ThreadPoolMonitor::getSystemCpuUsage() const {
    // TODO: Implement actual CPU usage monitoring
    // This would read from /proc/stat or similar
    return 0.0;
}

double ThreadPoolMonitor::getSystemMemoryUsage() const {
    // TODO: Implement actual memory usage monitoring
    // This would read from /proc/meminfo or similar
    return 0.0;
}

} // namespace ojo
