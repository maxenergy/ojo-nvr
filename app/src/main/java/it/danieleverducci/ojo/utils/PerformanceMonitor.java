package it.danieleverducci.ojo.utils;

import android.app.ActivityManager;
import android.content.Context;
import android.os.Debug;
import android.util.Log;

import java.util.HashMap;
import java.util.Map;

/**
 * 性能监控工具类
 * 用于监控应用的内存使用、CPU使用和播放器性能
 */
public class PerformanceMonitor {
    
    private static final String TAG = "PerformanceMonitor";
    private static PerformanceMonitor instance;
    private Context context;
    private Map<String, Long> startTimes = new HashMap<>();
    private Map<String, Integer> counters = new HashMap<>();
    
    private PerformanceMonitor(Context context) {
        this.context = context.getApplicationContext();
    }
    
    public static synchronized PerformanceMonitor getInstance(Context context) {
        if (instance == null) {
            instance = new PerformanceMonitor(context);
        }
        return instance;
    }
    
    /**
     * 开始计时
     */
    public void startTiming(String operation) {
        startTimes.put(operation, System.currentTimeMillis());
        Log.d(TAG, "Started timing: " + operation);
    }
    
    /**
     * 结束计时并记录
     */
    public long endTiming(String operation) {
        Long startTime = startTimes.get(operation);
        if (startTime == null) {
            Log.w(TAG, "No start time found for operation: " + operation);
            return -1;
        }
        
        long duration = System.currentTimeMillis() - startTime;
        Log.i(TAG, "Operation '" + operation + "' took " + duration + "ms");
        startTimes.remove(operation);
        return duration;
    }
    
    /**
     * 增加计数器
     */
    public void incrementCounter(String counterName) {
        int count = counters.getOrDefault(counterName, 0) + 1;
        counters.put(counterName, count);
        Log.d(TAG, "Counter '" + counterName + "': " + count);
    }
    
    /**
     * 获取计数器值
     */
    public int getCounter(String counterName) {
        return counters.getOrDefault(counterName, 0);
    }
    
    /**
     * 重置计数器
     */
    public void resetCounter(String counterName) {
        counters.put(counterName, 0);
        Log.d(TAG, "Reset counter: " + counterName);
    }
    
    /**
     * 获取当前内存使用情况
     */
    public MemoryInfo getMemoryInfo() {
        ActivityManager activityManager = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        ActivityManager.MemoryInfo memoryInfo = new ActivityManager.MemoryInfo();
        activityManager.getMemoryInfo(memoryInfo);
        
        // 获取应用私有内存使用
        Debug.MemoryInfo debugMemoryInfo = new Debug.MemoryInfo();
        Debug.getMemoryInfo(debugMemoryInfo);
        
        MemoryInfo info = new MemoryInfo();
        info.totalMemoryMB = memoryInfo.totalMem / (1024 * 1024);
        info.availableMemoryMB = memoryInfo.availMem / (1024 * 1024);
        info.usedMemoryMB = info.totalMemoryMB - info.availableMemoryMB;
        info.appPrivateMemoryKB = debugMemoryInfo.getTotalPrivateDirty();
        info.appSharedMemoryKB = debugMemoryInfo.getTotalSharedDirty();
        info.lowMemory = memoryInfo.lowMemory;
        
        return info;
    }
    
    /**
     * 记录内存使用情况
     */
    public void logMemoryUsage(String context) {
        MemoryInfo memInfo = getMemoryInfo();
        Log.i(TAG, String.format("[%s] Memory - Total: %dMB, Available: %dMB, Used: %dMB, " +
                "App Private: %dKB, App Shared: %dKB, Low Memory: %s",
                context,
                memInfo.totalMemoryMB,
                memInfo.availableMemoryMB,
                memInfo.usedMemoryMB,
                memInfo.appPrivateMemoryKB,
                memInfo.appSharedMemoryKB,
                memInfo.lowMemory ? "YES" : "NO"));
    }
    
    /**
     * 检查是否处于低内存状态
     */
    public boolean isLowMemory() {
        return getMemoryInfo().lowMemory;
    }
    
    /**
     * 获取应用私有内存使用（MB）
     */
    public double getAppMemoryUsageMB() {
        return getMemoryInfo().appPrivateMemoryKB / 1024.0;
    }
    
    /**
     * 记录播放器性能指标
     */
    public void logPlayerPerformance(String cameraName, String event, long value) {
        Log.i(TAG, String.format("Player Performance [%s] %s: %d", cameraName, event, value));
    }
    
    /**
     * 记录网络性能指标
     */
    public void logNetworkPerformance(String cameraName, String metric, long value) {
        Log.i(TAG, String.format("Network Performance [%s] %s: %d", cameraName, metric, value));
    }
    
    /**
     * 生成性能报告
     */
    public void generatePerformanceReport() {
        Log.i(TAG, "=== Performance Report ===");
        
        // 内存报告
        logMemoryUsage("Performance Report");
        
        // 计数器报告
        Log.i(TAG, "Counters:");
        for (Map.Entry<String, Integer> entry : counters.entrySet()) {
            Log.i(TAG, "  " + entry.getKey() + ": " + entry.getValue());
        }
        
        // 活跃计时器报告
        Log.i(TAG, "Active Timers:");
        for (String operation : startTimes.keySet()) {
            Log.i(TAG, "  " + operation + " (still running)");
        }
        
        Log.i(TAG, "=== End Performance Report ===");
    }
    
    /**
     * 清理资源
     */
    public void cleanup() {
        startTimes.clear();
        counters.clear();
        Log.d(TAG, "Performance monitor cleaned up");
    }
    
    /**
     * 内存信息数据类
     */
    public static class MemoryInfo {
        public long totalMemoryMB;
        public long availableMemoryMB;
        public long usedMemoryMB;
        public int appPrivateMemoryKB;
        public int appSharedMemoryKB;
        public boolean lowMemory;
    }
    
    /**
     * 性能监控常量
     */
    public static class Metrics {
        public static final String CAMERA_INIT_TIME = "camera_init_time";
        public static final String STREAM_START_TIME = "stream_start_time";
        public static final String FIRST_FRAME_TIME = "first_frame_time";
        public static final String ERROR_COUNT = "error_count";
        public static final String RETRY_COUNT = "retry_count";
        public static final String SUCCESSFUL_CONNECTIONS = "successful_connections";
        public static final String FAILED_CONNECTIONS = "failed_connections";
        public static final String BUFFER_UNDERRUNS = "buffer_underruns";
        public static final String NETWORK_ERRORS = "network_errors";
    }
}
