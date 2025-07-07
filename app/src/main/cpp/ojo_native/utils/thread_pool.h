#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "ojo_types.h"
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <future>
#include <atomic>
#include <condition_variable>
#include <set>
#include <map>
#include <mutex>

namespace ojo {

/**
 * High-performance thread pool for concurrent RTSP stream processing
 * Based on yolov5rtspthreadpool reference implementation
 */
class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    // Task submission
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>;
    
    // Pool management
    void resize(size_t numThreads);
    void shutdown();
    void waitForAll();
    
    // Status
    size_t getThreadCount() const;
    size_t getQueueSize() const;
    size_t getActiveThreads() const;
    bool isShutdown() const;
    
    // Statistics
    struct ThreadPoolStatistics {
        std::atomic<uint64_t> tasksSubmitted{0};
        std::atomic<uint64_t> tasksCompleted{0};
        std::atomic<uint64_t> tasksFailed{0};
        std::atomic<double> averageExecutionTime{0.0};
        std::atomic<size_t> peakQueueSize{0};
        std::atomic<size_t> activeThreads{0};

        // Copy constructor for atomic values
        ThreadPoolStatistics() = default;
        ThreadPoolStatistics(const ThreadPoolStatistics& other) {
            tasksSubmitted.store(other.tasksSubmitted.load());
            tasksCompleted.store(other.tasksCompleted.load());
            tasksFailed.store(other.tasksFailed.load());
            averageExecutionTime.store(other.averageExecutionTime.load());
            peakQueueSize.store(other.peakQueueSize.load());
            activeThreads.store(other.activeThreads.load());
        }

        ThreadPoolStatistics& operator=(const ThreadPoolStatistics& other) {
            if (this != &other) {
                tasksSubmitted.store(other.tasksSubmitted.load());
                tasksCompleted.store(other.tasksCompleted.load());
                tasksFailed.store(other.tasksFailed.load());
                averageExecutionTime.store(other.averageExecutionTime.load());
                peakQueueSize.store(other.peakQueueSize.load());
                activeThreads.store(other.activeThreads.load());
            }
            return *this;
        }

        void reset() {
            tasksSubmitted = 0;
            tasksCompleted = 0;
            tasksFailed = 0;
            averageExecutionTime = 0.0;
            peakQueueSize = 0;
            activeThreads = 0;
        }
    };
    
    ThreadPoolStatistics getStatistics() const;
    void resetStatistics();
    
private:
    // Worker threads
    std::vector<std::thread> workers_;
    
    // Task queue
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex tasksMutex_;
    std::condition_variable condition_;
    
    // State management
    std::atomic<bool> shutdown_;
    std::atomic<size_t> activeThreads_;
    
    // Statistics
    mutable ThreadPoolStatistics statistics_;
    
    // Internal methods
    void workerLoop();
    void updateStatistics(double executionTime);
};

/**
 * RTSP Stream Thread Pool - specialized for RTSP stream processing
 */
class RTSPStreamThreadPool {
public:
    explicit RTSPStreamThreadPool(size_t numThreads = 4);
    ~RTSPStreamThreadPool();
    
    // Stream task types
    enum class TaskType {
        RTSP_RECEIVE,
        VIDEO_DECODE,
        FRAME_RENDER,
        STATISTICS_UPDATE
    };
    
    // Task submission with priority
    template<typename F, typename... Args>
    auto submitRTSPTask(TaskType type, int priority, F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type>;
    
    // Stream-specific operations
    void setStreamPriority(int streamId, int priority);
    void pauseStream(int streamId);
    void resumeStream(int streamId);
    void removeStream(int streamId);
    
    // Configuration
    void setMaxQueueSize(size_t maxSize);
    void setThreadAffinity(bool enable);
    
    // Statistics
    struct RTSPThreadPoolStatistics {
        std::map<TaskType, uint64_t> taskCounts;
        std::map<int, uint64_t> streamTaskCounts;
        std::atomic<double> averageLatency{0.0};
        std::atomic<size_t> droppedTasks{0};

        // Copy constructor for atomic values
        RTSPThreadPoolStatistics() = default;
        RTSPThreadPoolStatistics(const RTSPThreadPoolStatistics& other)
            : taskCounts(other.taskCounts), streamTaskCounts(other.streamTaskCounts) {
            averageLatency.store(other.averageLatency.load());
            droppedTasks.store(other.droppedTasks.load());
        }

        RTSPThreadPoolStatistics& operator=(const RTSPThreadPoolStatistics& other) {
            if (this != &other) {
                taskCounts = other.taskCounts;
                streamTaskCounts = other.streamTaskCounts;
                averageLatency.store(other.averageLatency.load());
                droppedTasks.store(other.droppedTasks.load());
            }
            return *this;
        }

        void reset() {
            taskCounts.clear();
            streamTaskCounts.clear();
            averageLatency = 0.0;
            droppedTasks = 0;
        }
    };
    
    RTSPThreadPoolStatistics getRTSPStatistics() const;
    
private:
    // Priority task structure
    struct PriorityTask {
        std::function<void()> task;
        TaskType type;
        int priority;
        int streamId;
        std::chrono::steady_clock::time_point submitTime;
        
        bool operator<(const PriorityTask& other) const {
            return priority < other.priority; // Higher priority = lower number
        }
    };
    
    // Thread pool for different task types
    std::unique_ptr<ThreadPool> receivePool_;
    std::unique_ptr<ThreadPool> decodePool_;
    std::unique_ptr<ThreadPool> renderPool_;
    std::unique_ptr<ThreadPool> utilityPool_;
    
    // Priority queue for tasks
    std::priority_queue<PriorityTask> priorityTasks_;
    mutable std::mutex priorityMutex_;
    std::condition_variable priorityCondition_;
    
    // Stream management
    std::map<int, int> streamPriorities_;
    std::set<int> pausedStreams_;
    mutable std::mutex streamMutex_;
    
    // Configuration
    size_t maxQueueSize_;
    bool threadAffinityEnabled_;
    std::atomic<bool> shutdown_;
    
    // Statistics
    mutable RTSPThreadPoolStatistics rtspStatistics_;
    
    // Internal methods
    void priorityDispatcherLoop();
    ThreadPool* getPoolForTaskType(TaskType type);
    void setThreadAffinity(std::thread& thread, int cpu);
    void updateRTSPStatistics(TaskType type, int streamId, double executionTime);
    
    std::thread priorityDispatcher_;
};

/**
 * Performance Monitor for thread pools
 */
class ThreadPoolMonitor {
public:
    ThreadPoolMonitor();
    ~ThreadPoolMonitor();
    
    // Monitoring
    void addThreadPool(const std::string& name, ThreadPool* pool);
    void addRTSPThreadPool(const std::string& name, RTSPStreamThreadPool* pool);
    void removeThreadPool(const std::string& name);
    
    // Reporting
    struct PerformanceReport {
        std::map<std::string, ThreadPool::ThreadPoolStatistics> threadPoolStats;
        std::map<std::string, RTSPStreamThreadPool::RTSPThreadPoolStatistics> rtspPoolStats;
        std::chrono::steady_clock::time_point timestamp;
        double systemCpuUsage;
        double systemMemoryUsage;
    };
    
    PerformanceReport generateReport() const;
    void logReport(const PerformanceReport& report) const;
    
    // Automatic monitoring
    void startMonitoring(int intervalMs = 5000);
    void stopMonitoring();
    
    // Callbacks
    using AlertCallback = std::function<void(const std::string&, const std::string&)>;
    void setAlertCallback(AlertCallback callback);
    
private:
    // Monitored pools
    std::map<std::string, ThreadPool*> threadPools_;
    std::map<std::string, RTSPStreamThreadPool*> rtspThreadPools_;
    mutable std::mutex poolsMutex_;
    
    // Monitoring thread
    std::thread monitorThread_;
    std::atomic<bool> monitoring_;
    int monitoringInterval_;
    
    // Alert system
    AlertCallback alertCallback_;
    mutable std::mutex alertMutex_;
    
    // Internal methods
    void monitoringLoop();
    void checkAlerts(const PerformanceReport& report);
    double getSystemCpuUsage() const;
    double getSystemMemoryUsage() const;
    
    // Utility
    void logDebug(const std::string& message) const;
    void logError(const std::string& message) const;
    void logInfo(const std::string& message) const;
};

// Template implementation for ThreadPool::submit
template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
    using return_type = typename std::invoke_result<F, Args...>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(tasksMutex_);
        
        if (shutdown_) {
            throw std::runtime_error("ThreadPool is shutdown");
        }
        
        tasks_.emplace([task]() { (*task)(); });
        statistics_.tasksSubmitted++;
        
        // Update peak queue size
        size_t currentSize = tasks_.size();
        size_t peakSize = statistics_.peakQueueSize.load();
        while (currentSize > peakSize && 
               !statistics_.peakQueueSize.compare_exchange_weak(peakSize, currentSize)) {
            peakSize = statistics_.peakQueueSize.load();
        }
    }
    
    condition_.notify_one();
    return result;
}

// Template implementation for RTSPStreamThreadPool::submitRTSPTask
template<typename F, typename... Args>
auto RTSPStreamThreadPool::submitRTSPTask(TaskType type, int priority, F&& f, Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type> {

    using return_type = typename std::invoke_result<F, Args...>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(priorityMutex_);
        
        if (shutdown_) {
            throw std::runtime_error("RTSPStreamThreadPool is shutdown");
        }
        
        // Check queue size limit
        if (priorityTasks_.size() >= maxQueueSize_) {
            rtspStatistics_.droppedTasks++;
            throw std::runtime_error("Task queue is full");
        }
        
        PriorityTask priorityTask;
        priorityTask.task = [task]() { (*task)(); };
        priorityTask.type = type;
        priorityTask.priority = priority;
        priorityTask.streamId = 0; // Will be set by caller if needed
        priorityTask.submitTime = std::chrono::steady_clock::now();
        
        priorityTasks_.push(priorityTask);
        rtspStatistics_.taskCounts[type]++;
    }
    
    priorityCondition_.notify_one();
    return result;
}

} // namespace ojo

#endif // THREAD_POOL_H
