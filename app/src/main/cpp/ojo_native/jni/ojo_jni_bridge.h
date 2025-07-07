#ifndef OJO_JNI_BRIDGE_H
#define OJO_JNI_BRIDGE_H

#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "../utils/ojo_types.h"
#include "../rtsp/ZLRTSPClient.h"
#include "../decoder/MPPDecoder.h"
#include "../renderer/NativeSurfaceRenderer.h"
#include <memory>
#include <map>
#include <mutex>

namespace ojo {

/**
 * JNI Bridge for Ojo Native Player
 * Provides interface between Java layer and native C++ implementation
 */
class OjoJNIBridge {
public:
    OjoJNIBridge();
    ~OjoJNIBridge();
    
    // Player lifecycle
    OjoError createPlayer(JNIEnv* env, jobject surface);
    void destroyPlayer();
    bool isPlayerCreated() const;
    
    // Stream management
    OjoError startStream(const std::string& rtspUrl);
    void stopStream();
    void pauseStream();
    void resumeStream();
    bool isStreamActive() const;
    
    // Surface management
    OjoError setSurface(JNIEnv* env, jobject surface);
    void clearSurface();
    OjoError setDisplayRect(int x, int y, int width, int height);
    
    // Configuration
    void setStreamConfig(const StreamConfig& config);
    StreamConfig getStreamConfig() const;
    
    // Status and statistics
    StreamState getStreamState() const;
    StreamStatistics getStreamStatistics() const;
    
    // Callbacks to Java
    void setJavaCallbacks(JNIEnv* env, jobject javaObject);
    void clearJavaCallbacks();
    
private:
    // Core components
    std::unique_ptr<ZLRTSPClient> rtspClient_;
    std::unique_ptr<MPPDecoder> decoder_;
    std::unique_ptr<NativeSurfaceRenderer> renderer_;
    
    // Surface management
    ANativeWindow* nativeWindow_;
    std::mutex surfaceMutex_;
    
    // Configuration
    StreamConfig config_;
    std::mutex configMutex_;
    
    // Java callbacks
    JavaVM* javaVM_;
    jobject javaObject_;
    jmethodID onStateChangedMethod_;
    jmethodID onErrorMethod_;
    jmethodID onStatisticsMethod_;
    std::mutex javaCallbackMutex_;
    
    // State management
    std::atomic<bool> playerCreated_;
    std::atomic<bool> streamActive_;
    std::atomic<StreamState> currentState_;
    
    // Internal methods
    void setupCallbacks();
    void onFrameReceived(std::shared_ptr<VideoFrame> frame);
    void onStateChanged(StreamState state, const std::string& message);
    void onError(OjoError error, const std::string& message);
    void onStatisticsUpdate(const StreamStatistics& stats);
    
    // Java callback helpers
    void callJavaMethod(const char* methodName, const char* signature, ...);
    void callOnStateChanged(StreamState state, const std::string& message);
    void callOnError(OjoError error, const std::string& message);
    void callOnStatistics(const StreamStatistics& stats);
    
    // Utility
    void logDebug(const std::string& message) const;
    void logError(const std::string& message) const;
    void logInfo(const std::string& message) const;
};

/**
 * Global JNI Bridge Manager
 * Manages multiple player instances and provides global JNI functions
 */
class JNIBridgeManager {
public:
    static JNIBridgeManager& getInstance();
    
    // Player management
    long createPlayer(JNIEnv* env, jobject surface);
    bool destroyPlayer(long playerHandle);
    OjoJNIBridge* getPlayer(long playerHandle);
    
    // Global initialization
    void initialize(JavaVM* vm);
    void cleanup();
    
private:
    JNIBridgeManager() = default;
    ~JNIBridgeManager() = default;
    
    // Player storage
    std::map<long, std::unique_ptr<OjoJNIBridge>> players_;
    std::mutex playersMutex_;
    std::atomic<long> nextPlayerHandle_;
    
    // Global state
    JavaVM* javaVM_;
    std::atomic<bool> initialized_;
    
    // Utility
    long generatePlayerHandle();
};

} // namespace ojo

// JNI function declarations
extern "C" {

// Library lifecycle
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved);
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved);

// Player lifecycle
JNIEXPORT jlong JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeCreatePlayer(
    JNIEnv* env, jobject thiz, jobject surface);

JNIEXPORT void JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeDestroyPlayer(
    JNIEnv* env, jobject thiz, jlong playerHandle);

// Stream control
JNIEXPORT jboolean JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeStartStream(
    JNIEnv* env, jobject thiz, jlong playerHandle, jstring rtspUrl);

JNIEXPORT void JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeStopStream(
    JNIEnv* env, jobject thiz, jlong playerHandle);

JNIEXPORT void JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativePauseStream(
    JNIEnv* env, jobject thiz, jlong playerHandle);

JNIEXPORT void JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeResumeStream(
    JNIEnv* env, jobject thiz, jlong playerHandle);

// Surface management
JNIEXPORT jboolean JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeSetSurface(
    JNIEnv* env, jobject thiz, jlong playerHandle, jobject surface);

JNIEXPORT void JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeClearSurface(
    JNIEnv* env, jobject thiz, jlong playerHandle);

JNIEXPORT jboolean JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeSetDisplayRect(
    JNIEnv* env, jobject thiz, jlong playerHandle, jint x, jint y, jint width, jint height);

// Status queries
JNIEXPORT jboolean JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeIsPlaying(
    JNIEnv* env, jobject thiz, jlong playerHandle);

JNIEXPORT jint JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeGetState(
    JNIEnv* env, jobject thiz, jlong playerHandle);

// Configuration
JNIEXPORT void JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeSetConfig(
    JNIEnv* env, jobject thiz, jlong playerHandle, jobject config);

JNIEXPORT jobject JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeGetStatistics(
    JNIEnv* env, jobject thiz, jlong playerHandle);

// Callback setup
JNIEXPORT void JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeSetCallbacks(
    JNIEnv* env, jobject thiz, jlong playerHandle);

}

#endif // OJO_JNI_BRIDGE_H
