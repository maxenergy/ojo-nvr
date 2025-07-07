#include "ZLRTSPClient.h"
#include <android/log.h>

// STUB IMPLEMENTATION - ZLMediaKit libraries need to be rebuilt for Android
// This is a temporary stub to allow compilation while we fix the library issues

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "OjoRTSPClient"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace ojo {

ZLRTSPClient::ZLRTSPClient() : state_(StreamState::IDLE) {
    LOGW("ZLRTSPClient STUB: Constructor called - ZLMediaKit not available");
}

ZLRTSPClient::~ZLRTSPClient() {
    LOGW("ZLRTSPClient STUB: Destructor called");
}

OjoError ZLRTSPClient::connect(const std::string& rtspUrl) {
    LOGW("ZLRTSPClient STUB: connect(%s) - not implemented", rtspUrl.c_str());
    return OjoError::LIBRARY_NOT_AVAILABLE;
}

void ZLRTSPClient::disconnect() {
    LOGW("ZLRTSPClient STUB: disconnect() - not implemented");
}

bool ZLRTSPClient::isConnected() const {
    return false;
}

void ZLRTSPClient::reconnect() {
    LOGW("ZLRTSPClient STUB: reconnect() - not implemented");
}

void ZLRTSPClient::setConfig(const StreamConfig& config) {
    LOGW("ZLRTSPClient STUB: setConfig() - not implemented");
}

StreamConfig ZLRTSPClient::getConfig() const {
    return StreamConfig{};
}

void ZLRTSPClient::setFrameCallback(FrameCallback callback) {
    LOGW("ZLRTSPClient STUB: setFrameCallback() - not implemented");
}

void ZLRTSPClient::setStateCallback(StateCallback callback) {
    LOGW("ZLRTSPClient STUB: setStateCallback() - not implemented");
}

void ZLRTSPClient::setErrorCallback(ErrorCallback callback) {
    LOGW("ZLRTSPClient STUB: setErrorCallback() - not implemented");
}

void ZLRTSPClient::setStatisticsCallback(StatisticsCallback callback) {
    LOGW("ZLRTSPClient STUB: setStatisticsCallback() - not implemented");
}

StreamStatistics ZLRTSPClient::getStatistics() const {
    return StreamStatistics{};
}

StreamState ZLRTSPClient::getState() const {
    return state_;
}

void ZLRTSPClient::start() {
    LOGW("ZLRTSPClient STUB: start() - not implemented");
}

void ZLRTSPClient::stop() {
    LOGW("ZLRTSPClient STUB: stop() - not implemented");
}

void ZLRTSPClient::pause() {
    LOGW("ZLRTSPClient STUB: pause() - not implemented");
}

void ZLRTSPClient::resume() {
    LOGW("ZLRTSPClient STUB: resume() - not implemented");
}

} // namespace ojo
