#include "ojo_jni_bridge.h"
#include <android/log.h>
#include <string>

// Use specific LOG_TAG for this module if not already defined
#ifndef LOG_TAG
#define LOG_TAG "OjoJNIBridge"
#endif

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace ojo {

// errorToString is now defined in ojo_types.h

// Global JNI Bridge Manager implementation
JNIBridgeManager& JNIBridgeManager::getInstance() {
    static JNIBridgeManager instance;
    return instance;
}

long JNIBridgeManager::createPlayer(JNIEnv* env, jobject surface) {
    std::lock_guard<std::mutex> lock(playersMutex_);
    
    long handle = generatePlayerHandle();
    auto player = std::make_unique<OjoJNIBridge>();
    
    // Get native window from surface
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        LOGE("Failed to get native window from surface");
        return 0;
    }
    
    OjoError result = player->createPlayer(env, surface);
    if (result != OjoError::SUCCESS) {
        LOGE("Failed to create player: %s", errorToString(result));
        ANativeWindow_release(window);
        return 0;
    }
    
    players_[handle] = std::move(player);
    LOGI("Created player with handle: %ld", handle);
    
    return handle;
}

bool JNIBridgeManager::destroyPlayer(long playerHandle) {
    std::lock_guard<std::mutex> lock(playersMutex_);
    
    auto it = players_.find(playerHandle);
    if (it != players_.end()) {
        it->second->destroyPlayer();
        players_.erase(it);
        LOGI("Destroyed player with handle: %ld", playerHandle);
        return true;
    }
    
    LOGW("Player handle not found: %ld", playerHandle);
    return false;
}

OjoJNIBridge* JNIBridgeManager::getPlayer(long playerHandle) {
    std::lock_guard<std::mutex> lock(playersMutex_);
    
    auto it = players_.find(playerHandle);
    if (it != players_.end()) {
        return it->second.get();
    }
    
    return nullptr;
}

void JNIBridgeManager::initialize(JavaVM* vm) {
    javaVM_ = vm;
    initialized_ = true;
    LOGI("JNI Bridge Manager initialized");
}

void JNIBridgeManager::cleanup() {
    std::lock_guard<std::mutex> lock(playersMutex_);
    
    for (auto& pair : players_) {
        pair.second->destroyPlayer();
    }
    players_.clear();
    
    initialized_ = false;
    LOGI("JNI Bridge Manager cleaned up");
}

long JNIBridgeManager::generatePlayerHandle() {
    return nextPlayerHandle_++;
}

// OjoJNIBridge implementation
OjoJNIBridge::OjoJNIBridge()
    : nativeWindow_(nullptr)
    , javaVM_(nullptr)
    , javaObject_(nullptr)
    , onStateChangedMethod_(nullptr)
    , onErrorMethod_(nullptr)
    , onStatisticsMethod_(nullptr)
    , playerCreated_(false)
    , streamActive_(false)
    , currentState_(StreamState::IDLE) {
    
    LOGD("OjoJNIBridge created");
}

OjoJNIBridge::~OjoJNIBridge() {
    LOGD("OjoJNIBridge destructor called");
    destroyPlayer();
}

OjoError OjoJNIBridge::createPlayer(JNIEnv* env, jobject surface) {
    if (playerCreated_) {
        LOGW("Player already created");
        return OjoError::SUCCESS;
    }
    
    // Get native window from surface
    nativeWindow_ = ANativeWindow_fromSurface(env, surface);
    if (!nativeWindow_) {
        LOGE("Failed to get native window from surface");
        return OjoError::INITIALIZATION_FAILED;
    }
    
    // Create stream manager (placeholder - will be implemented with actual libraries)
    // streamManager_ = std::make_unique<RTSPStreamManager>();
    
    // Create decoder (placeholder - will be implemented with MPP)
    // decoder_ = std::make_unique<MPPDecoder>();
    
    // Create renderer (placeholder - will be implemented with native renderer)
    // renderer_ = std::make_unique<NativeSurfaceRenderer>();
    
    // Initialize components (placeholder implementation)
    // if (renderer_) {
    //     OjoError result = renderer_->initialize(nativeWindow_);
    //     if (result != OjoError::SUCCESS) {
    //         LOGE("Failed to initialize renderer");
    //         return result;
    //     }
    // }
    
    playerCreated_ = true;
    currentState_ = StreamState::IDLE;
    
    LOGI("Player created successfully");
    return OjoError::SUCCESS;
}

void OjoJNIBridge::destroyPlayer() {
    if (!playerCreated_) {
        return;
    }
    
    // Stop stream if active
    if (streamActive_) {
        stopStream();
    }
    
    // Cleanup components
    // if (renderer_) {
    //     renderer_->cleanup();
    //     renderer_.reset();
    // }
    
    // if (decoder_) {
    //     decoder_->cleanup();
    //     decoder_.reset();
    // }
    
    // streamManager_.reset();
    
    // Release native window
    if (nativeWindow_) {
        ANativeWindow_release(nativeWindow_);
        nativeWindow_ = nullptr;
    }
    
    // Clear Java callbacks
    clearJavaCallbacks();
    
    playerCreated_ = false;
    currentState_ = StreamState::IDLE;
    
    LOGI("Player destroyed");
}

OjoError OjoJNIBridge::startStream(const std::string& rtspUrl) {
    if (!playerCreated_) {
        LOGE("Player not created");
        return OjoError::INITIALIZATION_FAILED;
    }
    
    if (streamActive_) {
        LOGW("Stream already active");
        return OjoError::INVALID_STATE;
    }

    if (rtspUrl.empty()) {
        LOGE("Invalid RTSP URL");
        return OjoError::INVALID_PARAMETER;
    }

    LOGI("Starting stream: %s", rtspUrl.c_str());

    // Update configuration
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        config_.rtspUrl = rtspUrl;
    }

    try {
        // Initialize RTSP client if not already created
        if (!rtspClient_) {
            rtspClient_ = std::make_unique<ZLRTSPClient>();

            // Set up RTSP client callbacks
            rtspClient_->setFrameCallback([this](std::shared_ptr<VideoFrame> frame) {
                onFrameReceived(frame);
            });

            rtspClient_->setStateCallback([this](StreamState state, const std::string& message) {
                onStateChanged(state, message);
            });

            rtspClient_->setErrorCallback([this](OjoError error, const std::string& message) {
                onError(error, message);
            });

            rtspClient_->setStatisticsCallback([this](const StreamStatistics& stats) {
                onStatisticsUpdate(stats);
            });
        }

        // Initialize decoder if not already created
        if (!decoder_) {
            decoder_ = std::make_unique<MPPDecoder>();

            // Set up decoder callbacks
            decoder_->setFrameReadyCallback([this](std::shared_ptr<VideoFrame> frame) {
                // Send decoded frame to renderer
                if (renderer_) {
                    renderer_->renderFrame(*frame);
                }
            });

            // Initialize with H.264 codec
            OjoError decoderResult = decoder_->initialize(CodecType::H264);
            if (decoderResult != OjoError::SUCCESS) {
                LOGE("Failed to initialize decoder: %s", errorToString(decoderResult));
                return decoderResult;
            }

            // Set output format
            decoder_->setOutputFormat(VideoFormat::NV12);
        }

        // Initialize renderer if not already created
        if (!renderer_ && nativeWindow_) {
            renderer_ = std::make_unique<NativeSurfaceRenderer>();
            OjoError rendererResult = renderer_->initialize(nativeWindow_);
            if (rendererResult != OjoError::SUCCESS) {
                LOGE("Failed to initialize renderer: %s", errorToString(rendererResult));
                return rendererResult;
            }
        }

        // Configure and start RTSP client
        StreamConfig config;
        config.rtspUrl = rtspUrl;
        config.connectionTimeoutMs = 10000;
        config.readTimeoutMs = 5000;
        config.maxRetryAttempts = 3;
        config.enableHardwareDecoding = true;
        rtspClient_->setConfig(config);

        // Connect to the stream
        OjoError result = rtspClient_->connect(rtspUrl);
        if (result != OjoError::SUCCESS) {
            LOGE("Failed to connect RTSP client: %s", errorToString(result));
            return result;
        }

        // Start playing the stream
        rtspClient_->start();

        streamActive_ = true;
        currentState_ = StreamState::CONNECTING;
        onStateChanged(StreamState::CONNECTING, "Connecting to RTSP stream");

        LOGI("Stream initialization completed successfully");
        return OjoError::SUCCESS;

    } catch (const std::exception& e) {
        LOGE("Exception during stream start: %s", e.what());
        return OjoError::INITIALIZATION_FAILED;
    }
}

void OjoJNIBridge::stopStream() {
    if (!streamActive_) {
        return;
    }

    LOGI("Stopping stream");

    try {
        // Stop RTSP client
        if (rtspClient_) {
            rtspClient_->stop();
        }

        // Stop decoder
        if (decoder_) {
            decoder_->stop();
        }

        // Stop renderer
        if (renderer_) {
            renderer_->stop();
        }

        streamActive_ = false;
        currentState_ = StreamState::STOPPED;

        onStateChanged(StreamState::STOPPED, "Stream stopped");

        LOGI("Stream stopped successfully");

    } catch (const std::exception& e) {
        LOGE("Exception during stream stop: %s", e.what());
        streamActive_ = false;
        currentState_ = StreamState::ERROR;
        onError(OjoError::UNKNOWN_ERROR, "Failed to stop stream");
    }
}

void OjoJNIBridge::pauseStream() {
    if (!streamActive_) {
        return;
    }

    LOGI("Pausing stream");

    try {
        // Pause RTSP client
        if (rtspClient_) {
            rtspClient_->pause();
        }

        currentState_ = StreamState::PAUSED;
        onStateChanged(StreamState::PAUSED, "Stream paused");

        LOGI("Stream paused successfully");

    } catch (const std::exception& e) {
        LOGE("Exception during stream pause: %s", e.what());
        onError(OjoError::UNKNOWN_ERROR, "Failed to pause stream");
    }
}

void OjoJNIBridge::resumeStream() {
    if (!streamActive_ || currentState_ != StreamState::PAUSED) {
        return;
    }

    LOGI("Resuming stream");

    try {
        // Resume RTSP client
        if (rtspClient_) {
            rtspClient_->resume();
        }

        currentState_ = StreamState::PLAYING;
        onStateChanged(StreamState::PLAYING, "Stream resumed");

        LOGI("Stream resumed successfully");

    } catch (const std::exception& e) {
        LOGE("Exception during stream resume: %s", e.what());
        onError(OjoError::UNKNOWN_ERROR, "Failed to resume stream");
    }
}

bool OjoJNIBridge::isStreamActive() const {
    return streamActive_;
}

StreamState OjoJNIBridge::getStreamState() const {
    return currentState_;
}

void OjoJNIBridge::setJavaCallbacks(JNIEnv* env, jobject javaObject) {
    std::lock_guard<std::mutex> lock(javaCallbackMutex_);
    
    // Get Java VM
    env->GetJavaVM(&javaVM_);
    
    // Create global reference to Java object
    javaObject_ = env->NewGlobalRef(javaObject);
    
    // Get method IDs
    jclass clazz = env->GetObjectClass(javaObject);
    onStateChangedMethod_ = env->GetMethodID(clazz, "onStateChanged", "(ILjava/lang/String;)V");
    onErrorMethod_ = env->GetMethodID(clazz, "onError", "(ILjava/lang/String;)V");
    onStatisticsMethod_ = env->GetMethodID(clazz, "onStatistics", "(Lit/danieleverducci/ojo/native_player/OjoNativePlayer$PlaybackStatistics;)V");
    
    LOGI("Java callbacks set up successfully");
}

void OjoJNIBridge::clearJavaCallbacks() {
    std::lock_guard<std::mutex> lock(javaCallbackMutex_);

    if (javaObject_ && javaVM_) {
        JNIEnv* env;
        if (javaVM_->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_OK) {
            env->DeleteGlobalRef(javaObject_);
        }
    }

    javaObject_ = nullptr;
    javaVM_ = nullptr;
    onStateChangedMethod_ = nullptr;
    onErrorMethod_ = nullptr;
    onStatisticsMethod_ = nullptr;

    LOGI("Java callbacks cleared");
}

// Callback implementations
void OjoJNIBridge::onFrameReceived(std::shared_ptr<VideoFrame> frame) {
    if (!frame) {
        return;
    }

    try {
        // Send frame to decoder for hardware decoding
        if (decoder_ && frame->data && frame->dataSize > 0) {
            OjoError result = decoder_->decode(frame->data.get(), frame->dataSize, frame->timestamp);
            if (result != OjoError::SUCCESS) {
                LOGW("Failed to decode frame: %s", errorToString(result));
            }
        } else {
            LOGW("No decoder available or invalid frame data");
        }

    } catch (const std::exception& e) {
        LOGE("Exception in onFrameReceived: %s", e.what());
    }
}

void OjoJNIBridge::onStateChanged(StreamState state, const std::string& message) {
    currentState_ = state;
    
    std::lock_guard<std::mutex> lock(javaCallbackMutex_);
    if (javaObject_ && javaVM_ && onStateChangedMethod_) {
        JNIEnv* env;
        if (javaVM_->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_OK) {
            jstring jMessage = env->NewStringUTF(message.c_str());
            env->CallVoidMethod(javaObject_, onStateChangedMethod_, static_cast<int>(state), jMessage);
            env->DeleteLocalRef(jMessage);
        }
    }
}

void OjoJNIBridge::onError(OjoError error, const std::string& message) {
    std::lock_guard<std::mutex> lock(javaCallbackMutex_);
    if (javaObject_ && javaVM_ && onErrorMethod_) {
        JNIEnv* env;
        if (javaVM_->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_OK) {
            jstring jMessage = env->NewStringUTF(message.c_str());
            env->CallVoidMethod(javaObject_, onErrorMethod_, static_cast<int>(error), jMessage);
            env->DeleteLocalRef(jMessage);
        }
    }
}

void OjoJNIBridge::onStatisticsUpdate(const StreamStatistics& stats) {
    LOGD("Statistics update: frames=%llu, fps=%.2f, bytes=%llu",
         static_cast<unsigned long long>(stats.framesReceived),
         stats.currentFps,
         static_cast<unsigned long long>(stats.bytesReceived));

    // For now, just log the statistics
    // In the future, we could call a Java callback method to update UI
    // std::lock_guard<std::mutex> lock(javaCallbackMutex_);
    // if (javaObject_ && javaVM_ && onStatisticsMethod_) {
    //     JNIEnv* env;
    //     if (javaVM_->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_OK) {
    //         // Call Java statistics callback
    //     }
    // }
}

} // namespace ojo

// JNI function implementations
extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("JNI_OnLoad called");
    
    ojo::JNIBridgeManager::getInstance().initialize(vm);
    
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    LOGI("JNI_OnUnload called");
    
    ojo::JNIBridgeManager::getInstance().cleanup();
}

JNIEXPORT jlong JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeCreatePlayer(
    JNIEnv* env, jobject thiz, jobject surface) {
    
    return ojo::JNIBridgeManager::getInstance().createPlayer(env, surface);
}

JNIEXPORT void JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeDestroyPlayer(
    JNIEnv* env, jobject thiz, jlong playerHandle) {
    
    ojo::JNIBridgeManager::getInstance().destroyPlayer(playerHandle);
}

JNIEXPORT jboolean JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeStartStream(
    JNIEnv* env, jobject thiz, jlong playerHandle, jstring rtspUrl) {
    
    ojo::OjoJNIBridge* player = ojo::JNIBridgeManager::getInstance().getPlayer(playerHandle);
    if (!player) {
        return JNI_FALSE;
    }
    
    const char* urlStr = env->GetStringUTFChars(rtspUrl, nullptr);
    ojo::OjoError result = player->startStream(std::string(urlStr));
    env->ReleaseStringUTFChars(rtspUrl, urlStr);
    
    return (result == ojo::OjoError::SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeStopStream(
    JNIEnv* env, jobject thiz, jlong playerHandle) {
    
    ojo::OjoJNIBridge* player = ojo::JNIBridgeManager::getInstance().getPlayer(playerHandle);
    if (player) {
        player->stopStream();
    }
}

JNIEXPORT jboolean JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeIsPlaying(
    JNIEnv* env, jobject thiz, jlong playerHandle) {
    
    ojo::OjoJNIBridge* player = ojo::JNIBridgeManager::getInstance().getPlayer(playerHandle);
    if (player) {
        return player->isStreamActive() ? JNI_TRUE : JNI_FALSE;
    }
    
    return JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeGetState(
    JNIEnv* env, jobject thiz, jlong playerHandle) {
    
    ojo::OjoJNIBridge* player = ojo::JNIBridgeManager::getInstance().getPlayer(playerHandle);
    if (player) {
        return static_cast<int>(player->getStreamState());
    }
    
    return static_cast<int>(ojo::StreamState::IDLE);
}

JNIEXPORT void JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeSetCallbacks(
    JNIEnv* env, jobject thiz, jlong playerHandle) {
    
    ojo::OjoJNIBridge* player = ojo::JNIBridgeManager::getInstance().getPlayer(playerHandle);
    if (player) {
        player->setJavaCallbacks(env, thiz);
    }
}

// Placeholder implementations for other JNI functions
JNIEXPORT void JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativePauseStream(
    JNIEnv* env, jobject thiz, jlong playerHandle) {
    (void)env; (void)thiz; // Suppress unused parameter warnings

    ojo::OjoJNIBridge* player = ojo::JNIBridgeManager::getInstance().getPlayer(playerHandle);
    if (player) {
        player->pauseStream();
    }
}

JNIEXPORT void JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeResumeStream(
    JNIEnv* env, jobject thiz, jlong playerHandle) {
    (void)env; (void)thiz; // Suppress unused parameter warnings

    ojo::OjoJNIBridge* player = ojo::JNIBridgeManager::getInstance().getPlayer(playerHandle);
    if (player) {
        player->resumeStream();
    }
}

JNIEXPORT jboolean JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeSetSurface(
    JNIEnv* env, jobject thiz, jlong playerHandle, jobject surface) {
    // TODO: Implement surface setting
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeClearSurface(
    JNIEnv* env, jobject thiz, jlong playerHandle) {
    // TODO: Implement surface clearing
}

JNIEXPORT jboolean JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeSetDisplayRect(
    JNIEnv* env, jobject thiz, jlong playerHandle, jint x, jint y, jint width, jint height) {
    // TODO: Implement display rect setting
    return JNI_TRUE;
}

JNIEXPORT jobject JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeGetStatistics(
    JNIEnv* env, jobject thiz, jlong playerHandle) {
    // TODO: Implement statistics retrieval
    return nullptr;
}

JNIEXPORT void JNICALL
Java_it_danieleverducci_ojo_native_1player_OjoNativePlayer_nativeSetConfig(
    JNIEnv* env, jobject thiz, jlong playerHandle, jobject config) {
    // TODO: Implement configuration setting
}

} // extern "C"
