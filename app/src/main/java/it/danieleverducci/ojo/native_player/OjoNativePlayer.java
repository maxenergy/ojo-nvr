package it.danieleverducci.ojo.native_player;

import android.view.Surface;

/**
 * Native RTSP player using ZLMediaKit + Rockchip MPP
 * Provides hardware-accelerated RTSP streaming for Ojo surveillance system
 */
public class OjoNativePlayer {
    
    static {
        System.loadLibrary("ojo_native");
    }
    
    // Player states (must match native StreamState enum)
    public static final int STATE_IDLE = 0;
    public static final int STATE_CONNECTING = 1;
    public static final int STATE_CONNECTED = 2;
    public static final int STATE_PLAYING = 3;
    public static final int STATE_PAUSED = 4;
    public static final int STATE_STOPPED = 5;
    public static final int STATE_ERROR = 6;
    
    // Error codes (must match native OjoError enum)
    public static final int ERROR_SUCCESS = 0;
    public static final int ERROR_INVALID_PARAMETER = 1;
    public static final int ERROR_INITIALIZATION_FAILED = 2;
    public static final int ERROR_CONNECTION_FAILED = 3;
    public static final int ERROR_DECODE_FAILED = 4;
    public static final int ERROR_RENDER_FAILED = 5;
    public static final int ERROR_OUT_OF_MEMORY = 6;
    public static final int ERROR_TIMEOUT = 7;
    public static final int ERROR_UNKNOWN = 8;
    
    private long nativePlayerHandle = 0;
    private PlayerListener listener;
    
    /**
     * Player event listener interface
     */
    public interface PlayerListener {
        /**
         * Called when player state changes
         * @param state New player state (STATE_*)
         * @param message Descriptive message
         */
        void onStateChanged(int state, String message);
        
        /**
         * Called when an error occurs
         * @param errorCode Error code (ERROR_*)
         * @param message Error message
         */
        void onError(int errorCode, String message);
        
        /**
         * Called periodically with playback statistics
         * @param statistics Current playback statistics
         */
        void onStatistics(PlaybackStatistics statistics);
    }
    
    /**
     * Playback statistics data class
     */
    public static class PlaybackStatistics {
        public long framesReceived;
        public long framesDecoded;
        public long framesRendered;
        public long framesDropped;
        public long bytesReceived;
        public double currentFps;
        public double averageFps;
        public long lastFrameTimestamp;
        public int connectionRetries;
        
        public PlaybackStatistics() {}
        
        public PlaybackStatistics(long framesReceived, long framesDecoded, long framesRendered,
                                long framesDropped, long bytesReceived, double currentFps,
                                double averageFps, long lastFrameTimestamp, int connectionRetries) {
            this.framesReceived = framesReceived;
            this.framesDecoded = framesDecoded;
            this.framesRendered = framesRendered;
            this.framesDropped = framesDropped;
            this.bytesReceived = bytesReceived;
            this.currentFps = currentFps;
            this.averageFps = averageFps;
            this.lastFrameTimestamp = lastFrameTimestamp;
            this.connectionRetries = connectionRetries;
        }
        
        @Override
        public String toString() {
            return String.format("PlaybackStatistics{received=%d, decoded=%d, rendered=%d, " +
                    "dropped=%d, bytes=%d, fps=%.1f/%.1f, retries=%d}",
                    framesReceived, framesDecoded, framesRendered, framesDropped,
                    bytesReceived, currentFps, averageFps, connectionRetries);
        }
    }
    
    /**
     * Stream configuration data class
     */
    public static class StreamConfig {
        public String rtspUrl;
        public int streamId;
        public int displayX, displayY;
        public int displayWidth, displayHeight;
        public boolean enableHardwareDecoding;
        public int connectionTimeoutMs;
        public int readTimeoutMs;
        public int maxRetryAttempts;
        
        public StreamConfig() {
            streamId = 0;
            displayX = displayY = 0;
            displayWidth = 1280;
            displayHeight = 720;
            enableHardwareDecoding = true;
            connectionTimeoutMs = 10000;
            readTimeoutMs = 5000;
            maxRetryAttempts = 3;
        }
    }
    
    /**
     * Create native player instance
     * @param surface Android Surface for video rendering
     * @return true if successful, false otherwise
     */
    public boolean createPlayer(Surface surface) {
        if (nativePlayerHandle != 0) {
            return false; // Already created
        }
        
        nativePlayerHandle = nativeCreatePlayer(surface);
        if (nativePlayerHandle != 0) {
            nativeSetCallbacks(nativePlayerHandle);
            return true;
        }
        return false;
    }
    
    /**
     * Destroy native player instance
     */
    public void destroyPlayer() {
        if (nativePlayerHandle != 0) {
            nativeDestroyPlayer(nativePlayerHandle);
            nativePlayerHandle = 0;
        }
    }
    
    /**
     * Start RTSP stream playback
     * @param rtspUrl RTSP stream URL
     * @return true if successful, false otherwise
     */
    public boolean startStream(String rtspUrl) {
        if (nativePlayerHandle == 0) {
            return false;
        }
        return nativeStartStream(nativePlayerHandle, rtspUrl);
    }
    
    /**
     * Stop RTSP stream playback
     */
    public void stopStream() {
        if (nativePlayerHandle != 0) {
            nativeStopStream(nativePlayerHandle);
        }
    }
    
    /**
     * Pause RTSP stream playback
     */
    public void pauseStream() {
        if (nativePlayerHandle != 0) {
            nativePauseStream(nativePlayerHandle);
        }
    }
    
    /**
     * Resume RTSP stream playback
     */
    public void resumeStream() {
        if (nativePlayerHandle != 0) {
            nativeResumeStream(nativePlayerHandle);
        }
    }
    
    /**
     * Set video surface
     * @param surface Android Surface for video rendering
     * @return true if successful, false otherwise
     */
    public boolean setSurface(Surface surface) {
        if (nativePlayerHandle == 0) {
            return false;
        }
        return nativeSetSurface(nativePlayerHandle, surface);
    }
    
    /**
     * Clear video surface
     */
    public void clearSurface() {
        if (nativePlayerHandle != 0) {
            nativeClearSurface(nativePlayerHandle);
        }
    }
    
    /**
     * Set display rectangle
     * @param x X coordinate
     * @param y Y coordinate
     * @param width Width
     * @param height Height
     * @return true if successful, false otherwise
     */
    public boolean setDisplayRect(int x, int y, int width, int height) {
        if (nativePlayerHandle == 0) {
            return false;
        }
        return nativeSetDisplayRect(nativePlayerHandle, x, y, width, height);
    }
    
    /**
     * Check if stream is currently playing
     * @return true if playing, false otherwise
     */
    public boolean isPlaying() {
        if (nativePlayerHandle == 0) {
            return false;
        }
        return nativeIsPlaying(nativePlayerHandle);
    }
    
    /**
     * Get current player state
     * @return Current state (STATE_*)
     */
    public int getState() {
        if (nativePlayerHandle == 0) {
            return STATE_IDLE;
        }
        return nativeGetState(nativePlayerHandle);
    }
    
    /**
     * Get current playback statistics
     * @return Current statistics or null if not available
     */
    public PlaybackStatistics getStatistics() {
        if (nativePlayerHandle == 0) {
            return null;
        }
        return nativeGetStatistics(nativePlayerHandle);
    }
    
    /**
     * Set player event listener
     * @param listener Event listener
     */
    public void setListener(PlayerListener listener) {
        this.listener = listener;
    }
    
    /**
     * Set stream configuration
     * @param config Stream configuration
     */
    public void setConfig(StreamConfig config) {
        if (nativePlayerHandle != 0) {
            nativeSetConfig(nativePlayerHandle, config);
        }
    }
    
    // Native callback methods (called from JNI)
    private void onStateChanged(int state, String message) {
        if (listener != null) {
            listener.onStateChanged(state, message);
        }
    }
    
    private void onError(int errorCode, String message) {
        if (listener != null) {
            listener.onError(errorCode, message);
        }
    }
    
    private void onStatistics(PlaybackStatistics statistics) {
        if (listener != null) {
            listener.onStatistics(statistics);
        }
    }
    
    // Native method declarations
    private native long nativeCreatePlayer(Surface surface);
    private native void nativeDestroyPlayer(long playerHandle);
    private native boolean nativeStartStream(long playerHandle, String rtspUrl);
    private native void nativeStopStream(long playerHandle);
    private native void nativePauseStream(long playerHandle);
    private native void nativeResumeStream(long playerHandle);
    private native boolean nativeSetSurface(long playerHandle, Surface surface);
    private native void nativeClearSurface(long playerHandle);
    private native boolean nativeSetDisplayRect(long playerHandle, int x, int y, int width, int height);
    private native boolean nativeIsPlaying(long playerHandle);
    private native int nativeGetState(long playerHandle);
    private native PlaybackStatistics nativeGetStatistics(long playerHandle);
    private native void nativeSetConfig(long playerHandle, StreamConfig config);
    private native void nativeSetCallbacks(long playerHandle);
}
