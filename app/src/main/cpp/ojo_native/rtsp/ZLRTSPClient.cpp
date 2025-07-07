#include "ZLRTSPClient.h"
#include <android/log.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <regex>
#include <sstream>
#include <cstring>
#include <set>
#include <map>
#include <chrono>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "OjoRTSPClient"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace ojo {

// RTSP protocol constants
static const char* RTSP_VERSION = "RTSP/1.0";
static const int RTSP_DEFAULT_PORT = 554;
static const int RTP_HEADER_SIZE = 12;
static const int MAX_PACKET_SIZE = 65536;

ZLRTSPClient::ZLRTSPClient()
    : state_(StreamState::IDLE)
    , rtspSocket_(-1)
    , rtpSocket_(-1)
    , rtcpSocket_(-1)
    , cseq_(1)
    , sessionId_("")
    , hasVideo_(false)
    , videoPayloadType_(-1)
    , videoCodec_(CodecType::UNKNOWN)
    , controlUrl_("")
    , shouldStop_(false)
    , isConnected_(false) {
    
    LOGD("ZLRTSPClient created with native implementation");
    
    // Initialize default configuration
    config_.connectionTimeoutMs = 10000;
    config_.readTimeoutMs = 5000;
    config_.maxRetryAttempts = 3;
    config_.enableHardwareDecoding = true;
    
    // Reset statistics
    statistics_.reset();
}

ZLRTSPClient::~ZLRTSPClient() {
    LOGD("ZLRTSPClient destructor called");
    stop();
    disconnect();
}

OjoError ZLRTSPClient::connect(const std::string& rtspUrl) {
    LOGI("Connecting to RTSP URL: %s", rtspUrl.c_str());
    
    if (!isValidRTSPUrl(rtspUrl)) {
        LOGE("Invalid RTSP URL: %s", rtspUrl.c_str());
        return OjoError::INVALID_PARAMETER;
    }
    
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.rtspUrl = rtspUrl;
    
    setState(StreamState::CONNECTING, "Attempting to connect to RTSP stream");
    
    // Parse RTSP URL
    if (!parseRTSPUrl(rtspUrl)) {
        setState(StreamState::ERROR, "Failed to parse RTSP URL");
        return OjoError::INVALID_PARAMETER;
    }
    
    // Establish TCP connection to RTSP server
    if (!connectToServer()) {
        setState(StreamState::ERROR, "Failed to connect to RTSP server");
        return OjoError::CONNECTION_FAILED;
    }
    
    // Perform RTSP handshake
    if (!performRTSPHandshake()) {
        setState(StreamState::ERROR, "RTSP handshake failed");
        return OjoError::CONNECTION_FAILED;
    }
    
    setState(StreamState::CONNECTED, "Successfully connected to RTSP stream");
    isConnected_ = true;
    return OjoError::SUCCESS;
}

void ZLRTSPClient::disconnect() {
    LOGI("Disconnecting RTSP client");
    
    shouldStop_ = true;
    isConnected_ = false;
    
    // Send RTSP TEARDOWN if we have a session
    if (!sessionId_.empty() && rtspSocket_ >= 0) {
        sendTeardown();
    }
    
    // Close sockets
    if (rtspSocket_ >= 0) {
        close(rtspSocket_);
        rtspSocket_ = -1;
    }
    if (rtpSocket_ >= 0) {
        close(rtpSocket_);
        rtpSocket_ = -1;
    }
    if (rtcpSocket_ >= 0) {
        close(rtcpSocket_);
        rtcpSocket_ = -1;
    }
    
    // Stop worker threads
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    
    if (rtpReceiverThread_.joinable()) {
        rtpReceiverThread_.join();
    }
    
    setState(StreamState::IDLE, "Disconnected from RTSP stream");
}

bool ZLRTSPClient::isConnected() const {
    return isConnected_;
}

void ZLRTSPClient::reconnect() {
    LOGI("Attempting to reconnect");
    disconnect();
    if (!config_.rtspUrl.empty()) {
        connect(config_.rtspUrl);
    }
}

void ZLRTSPClient::setConfig(const StreamConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
}

StreamConfig ZLRTSPClient::getConfig() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

void ZLRTSPClient::setFrameCallback(FrameCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    frameCallback_ = callback;
}

void ZLRTSPClient::setStateCallback(StateCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    stateCallback_ = callback;
}

void ZLRTSPClient::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    errorCallback_ = callback;
}

void ZLRTSPClient::setStatisticsCallback(StatisticsCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    statisticsCallback_ = callback;
}

StreamStatistics ZLRTSPClient::getStatistics() const {
    StreamStatistics stats;
    stats.framesReceived = statistics_.framesReceived.load();
    stats.framesDecoded = statistics_.framesDecoded.load();
    stats.framesRendered = statistics_.framesRendered.load();
    stats.framesDropped = statistics_.framesDropped.load();
    stats.bytesReceived = statistics_.bytesReceived.load();
    stats.currentFps = statistics_.currentFps.load();
    stats.averageFps = statistics_.averageFps.load();
    stats.lastFrameTimestamp = statistics_.lastFrameTimestamp.load();
    stats.connectionRetries = statistics_.connectionRetries.load();
    return stats;
}

StreamState ZLRTSPClient::getState() const {
    return state_.load();
}

void ZLRTSPClient::start() {
    LOGI("Starting RTSP client");
    
    if (!isConnected_) {
        LOGW("Cannot start - not connected. Call connect() first.");
        return;
    }
    
    shouldStop_ = false;
    
    // Start worker thread for RTSP keep-alive
    if (!workerThread_.joinable()) {
        workerThread_ = std::thread(&ZLRTSPClient::workerLoop, this);
    }
    
    // Start RTP receiver thread
    if (!rtpReceiverThread_.joinable()) {
        rtpReceiverThread_ = std::thread(&ZLRTSPClient::rtpReceiverLoop, this);
    }
    
    // Send RTSP PLAY command
    if (sendPlay()) {
        setState(StreamState::PLAYING, "RTSP stream started");
    } else {
        setState(StreamState::ERROR, "Failed to start RTSP stream");
    }
}

void ZLRTSPClient::stop() {
    LOGI("Stopping RTSP client");
    
    shouldStop_ = true;
    
    // Send RTSP PAUSE command
    if (isConnected_ && !sessionId_.empty()) {
        sendPause();
    }
    
    setState(StreamState::STOPPED, "RTSP stream stopped");
}

void ZLRTSPClient::pause() {
    LOGI("Pausing RTSP stream");
    if (isConnected_ && !sessionId_.empty()) {
        sendPause();
        setState(StreamState::PAUSED, "RTSP stream paused");
    }
}

void ZLRTSPClient::resume() {
    LOGI("Resuming RTSP stream");
    if (isConnected_ && !sessionId_.empty()) {
        sendPlay();
        setState(StreamState::PLAYING, "RTSP stream resumed");
    }
}

// Private method implementations

bool ZLRTSPClient::parseRTSPUrl(const std::string& url) {
    // Parse rtsp://[user:pass@]host[:port]/path
    std::regex urlRegex(R"(rtsp://(?:([^:]+):([^@]+)@)?([^:/]+)(?::(\d+))?(/.*))");
    std::smatch matches;

    if (!std::regex_match(url, matches, urlRegex)) {
        LOGE("Failed to parse RTSP URL: %s", url.c_str());
        return false;
    }

    rtspHost_ = matches[3].str();
    rtspPort_ = matches[4].matched ? std::stoi(matches[4].str()) : RTSP_DEFAULT_PORT;
    rtspPath_ = matches[5].str();

    if (matches[1].matched) {
        rtspUsername_ = matches[1].str();
        rtspPassword_ = matches[2].str();
    }

    LOGD("Parsed RTSP URL - Host: %s, Port: %d, Path: %s",
         rtspHost_.c_str(), rtspPort_, rtspPath_.c_str());

    return true;
}

bool ZLRTSPClient::connectToServer() {
    LOGD("Connecting to RTSP server %s:%d", rtspHost_.c_str(), rtspPort_);

    rtspSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (rtspSocket_ < 0) {
        LOGE("Failed to create socket: %s", strerror(errno));
        return false;
    }

    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = config_.connectionTimeoutMs / 1000;
    timeout.tv_usec = (config_.connectionTimeoutMs % 1000) * 1000;
    setsockopt(rtspSocket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(rtspSocket_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(rtspPort_);

    if (inet_pton(AF_INET, rtspHost_.c_str(), &serverAddr.sin_addr) <= 0) {
        LOGE("Invalid IP address: %s", rtspHost_.c_str());
        close(rtspSocket_);
        rtspSocket_ = -1;
        return false;
    }

    if (::connect(rtspSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        LOGE("Failed to connect to server: %s", strerror(errno));
        close(rtspSocket_);
        rtspSocket_ = -1;
        return false;
    }

    LOGD("Successfully connected to RTSP server");
    return true;
}

bool ZLRTSPClient::performRTSPHandshake() {
    LOGD("Performing RTSP handshake");

    // Send OPTIONS request
    if (!sendOptions()) {
        LOGE("OPTIONS request failed");
        return false;
    }

    // Send DESCRIBE request
    if (!sendDescribe()) {
        LOGE("DESCRIBE request failed");
        return false;
    }

    // Send SETUP request
    if (!sendSetup()) {
        LOGE("SETUP request failed");
        return false;
    }

    LOGD("RTSP handshake completed successfully");
    return true;
}

bool ZLRTSPClient::sendOptions() {
    std::ostringstream request;
    request << "OPTIONS " << config_.rtspUrl << " " << RTSP_VERSION << "\r\n";
    request << "CSeq: " << cseq_++ << "\r\n";
    request << "User-Agent: OjoRTSPClient/1.0\r\n";
    request << "\r\n";

    return sendRTSPRequest(request.str());
}

bool ZLRTSPClient::sendDescribe() {
    std::ostringstream request;
    request << "DESCRIBE " << config_.rtspUrl << " " << RTSP_VERSION << "\r\n";
    request << "CSeq: " << cseq_++ << "\r\n";
    request << "Accept: application/sdp\r\n";
    request << "User-Agent: OjoRTSPClient/1.0\r\n";
    request << "\r\n";

    if (!sendRTSPRequest(request.str())) {
        return false;
    }

    // Parse SDP from response to get media information
    return parseSDP(lastResponse_);
}

bool ZLRTSPClient::sendSetup() {
    // Create RTP/RTCP sockets
    if (!createRTPSockets()) {
        return false;
    }

    std::ostringstream request;
    std::string setupUrl = config_.rtspUrl;
    if (!controlUrl_.empty() && controlUrl_ != "*") {
        setupUrl += "/" + controlUrl_;
    } else {
        setupUrl += "/trackID=0"; // Fallback
    }
    request << "SETUP " << setupUrl << " " << RTSP_VERSION << "\r\n";
    request << "CSeq: " << cseq_++ << "\r\n";
    request << "Transport: RTP/AVP;unicast;client_port=" << rtpPort_ << "-" << (rtpPort_ + 1) << "\r\n";
    request << "User-Agent: OjoRTSPClient/1.0\r\n";
    request << "\r\n";

    if (!sendRTSPRequest(request.str())) {
        return false;
    }

    // Extract session ID from response
    return parseSessionId(lastResponse_);
}

bool ZLRTSPClient::sendPlay() {
    if (sessionId_.empty()) {
        LOGE("No session ID available for PLAY request");
        return false;
    }

    std::ostringstream request;
    request << "PLAY " << config_.rtspUrl << " " << RTSP_VERSION << "\r\n";
    request << "CSeq: " << cseq_++ << "\r\n";
    request << "Session: " << sessionId_ << "\r\n";
    request << "Range: npt=0.000-\r\n";
    request << "User-Agent: OjoRTSPClient/1.0\r\n";
    request << "\r\n";

    return sendRTSPRequest(request.str());
}

bool ZLRTSPClient::sendPause() {
    if (sessionId_.empty()) {
        LOGE("No session ID available for PAUSE request");
        return false;
    }

    std::ostringstream request;
    request << "PAUSE " << config_.rtspUrl << " " << RTSP_VERSION << "\r\n";
    request << "CSeq: " << cseq_++ << "\r\n";
    request << "Session: " << sessionId_ << "\r\n";
    request << "User-Agent: OjoRTSPClient/1.0\r\n";
    request << "\r\n";

    return sendRTSPRequest(request.str());
}

bool ZLRTSPClient::sendTeardown() {
    if (sessionId_.empty()) {
        LOGE("No session ID available for TEARDOWN request");
        return false;
    }

    std::ostringstream request;
    request << "TEARDOWN " << config_.rtspUrl << " " << RTSP_VERSION << "\r\n";
    request << "CSeq: " << cseq_++ << "\r\n";
    request << "Session: " << sessionId_ << "\r\n";
    request << "User-Agent: OjoRTSPClient/1.0\r\n";
    request << "\r\n";

    return sendRTSPRequest(request.str());
}

bool ZLRTSPClient::sendRTSPRequest(const std::string& request) {
    LOGD("Sending RTSP request:\n%s", request.c_str());

    if (rtspSocket_ < 0) {
        LOGE("RTSP socket not connected");
        return false;
    }

    ssize_t sent = send(rtspSocket_, request.c_str(), request.length(), 0);
    if (sent != static_cast<ssize_t>(request.length())) {
        LOGE("Failed to send complete RTSP request: %s", strerror(errno));
        return false;
    }

    // Read response
    char buffer[4096];
    ssize_t received = recv(rtspSocket_, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        LOGE("Failed to receive RTSP response: %s", strerror(errno));
        return false;
    }

    buffer[received] = '\0';
    lastResponse_ = std::string(buffer);
    LOGD("Received RTSP response:\n%s", lastResponse_.c_str());

    // Check if response indicates success (2xx status code)
    if (lastResponse_.find("RTSP/1.0 2") != 0) {
        LOGE("RTSP request failed with response: %s", lastResponse_.c_str());
        return false;
    }

    return true;
}

bool ZLRTSPClient::createRTPSockets() {
    // Create RTP socket (even port)
    rtpSocket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (rtpSocket_ < 0) {
        LOGE("Failed to create RTP socket: %s", strerror(errno));
        return false;
    }

    // Create RTCP socket (odd port)
    rtcpSocket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (rtcpSocket_ < 0) {
        LOGE("Failed to create RTCP socket: %s", strerror(errno));
        close(rtpSocket_);
        rtpSocket_ = -1;
        return false;
    }

    // Bind RTP socket to any available even port
    struct sockaddr_in rtpAddr;
    memset(&rtpAddr, 0, sizeof(rtpAddr));
    rtpAddr.sin_family = AF_INET;
    rtpAddr.sin_addr.s_addr = INADDR_ANY;

    // Try to find available port pair
    for (int port = 10000; port < 20000; port += 2) {
        rtpAddr.sin_port = htons(port);
        if (bind(rtpSocket_, (struct sockaddr*)&rtpAddr, sizeof(rtpAddr)) == 0) {
            rtpPort_ = port;

            // Bind RTCP socket to next port
            rtpAddr.sin_port = htons(port + 1);
            if (bind(rtcpSocket_, (struct sockaddr*)&rtpAddr, sizeof(rtpAddr)) == 0) {
                LOGD("Created RTP/RTCP sockets on ports %d/%d", port, port + 1);
                return true;
            }
        }
    }

    LOGE("Failed to bind RTP/RTCP sockets");
    close(rtpSocket_);
    close(rtcpSocket_);
    rtpSocket_ = rtcpSocket_ = -1;
    return false;
}

bool ZLRTSPClient::parseSDP(const std::string& response) {
    // Extract SDP content from RTSP response
    size_t sdpStart = response.find("\r\n\r\n");
    if (sdpStart == std::string::npos) {
        LOGE("No SDP content found in DESCRIBE response");
        return false;
    }

    std::string sdp = response.substr(sdpStart + 4);
    LOGD("Parsing SDP:\n%s", sdp.c_str());

    // Parse SDP to extract media information
    std::istringstream sdpStream(sdp);
    std::string line;

    while (std::getline(sdpStream, line)) {
        if (line.empty() || line.back() == '\r') {
            line.pop_back();
        }

        if (line.find("m=video") == 0) {
            // Found video media line
            LOGD("Found video media in SDP");
            hasVideo_ = true;
        } else if (line.find("a=rtpmap:") == 0) {
            // Parse RTP payload mapping
            parseRTPMap(line);
        } else if (line.find("a=control:") == 0) {
            // Parse control URL
            controlUrl_ = line.substr(10); // Remove "a=control:"
            LOGD("Found control URL: %s", controlUrl_.c_str());
        }
    }

    return hasVideo_;
}

void ZLRTSPClient::parseRTPMap(const std::string& line) {
    // Parse "a=rtpmap:96 H264/90000" format
    std::regex rtpmapRegex(R"(a=rtpmap:(\d+)\s+([^/]+)/(\d+))");
    std::smatch matches;

    if (std::regex_match(line, matches, rtpmapRegex)) {
        int payloadType = std::stoi(matches[1].str());
        std::string codec = matches[2].str();
        int clockRate = std::stoi(matches[3].str());

        LOGD("RTP mapping: PT=%d, Codec=%s, Clock=%d", payloadType, codec.c_str(), clockRate);

        if (codec == "H264") {
            videoPayloadType_ = payloadType;
            videoCodec_ = CodecType::H264;
        } else if (codec == "H265" || codec == "HEVC") {
            videoPayloadType_ = payloadType;
            videoCodec_ = CodecType::H265;
        }
    }
}

bool ZLRTSPClient::parseSessionId(const std::string& response) {
    // Extract session ID from "Session: 12345678;timeout=60" format
    std::regex sessionRegex(R"(Session:\s*([^;\s]+))");
    std::smatch matches;

    if (std::regex_search(response, matches, sessionRegex)) {
        sessionId_ = matches[1].str();
        LOGD("Extracted session ID: %s", sessionId_.c_str());
        return true;
    }

    LOGE("Failed to extract session ID from SETUP response");
    return false;
}

bool ZLRTSPClient::isValidRTSPUrl(const std::string& url) const {
    std::regex rtspRegex(R"(^rtsp://[a-zA-Z0-9\-\.]+(?::\d+)?/.*)");
    return std::regex_match(url, rtspRegex);
}

void ZLRTSPClient::setState(StreamState newState, const std::string& message) {
    StreamState oldState = state_.exchange(newState);

    if (oldState != newState) {
        LOGI("State changed: %d -> %d (%s)", static_cast<int>(oldState), static_cast<int>(newState), message.c_str());

        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (stateCallback_) {
            stateCallback_(newState, message);
        }
    }
}

void ZLRTSPClient::reportError(OjoError error, const std::string& message) {
    LOGE("Error reported: %d - %s", static_cast<int>(error), message.c_str());

    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (errorCallback_) {
        errorCallback_(error, message);
    }
}

void ZLRTSPClient::workerLoop() {
    LOGD("Worker loop started");

    auto lastKeepAlive = std::chrono::steady_clock::now();
    const auto keepAliveInterval = std::chrono::seconds(30);

    while (!shouldStop_) {
        try {
            auto now = std::chrono::steady_clock::now();

            // Send keep-alive (GET_PARAMETER) every 30 seconds
            if (now - lastKeepAlive >= keepAliveInterval && !sessionId_.empty()) {
                sendKeepAlive();
                lastKeepAlive = now;
            }

            // Update statistics
            updateStatistics();

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        } catch (const std::exception& e) {
            LOGE("Exception in worker loop: %s", e.what());
            reportError(OjoError::UNKNOWN_ERROR, e.what());
        }
    }

    LOGD("Worker loop ended");
}

void ZLRTSPClient::rtpReceiverLoop() {
    LOGD("RTP receiver loop started");

    uint8_t buffer[MAX_PACKET_SIZE];
    struct sockaddr_in senderAddr;
    socklen_t senderLen = sizeof(senderAddr);

    while (!shouldStop_ && rtpSocket_ >= 0) {
        ssize_t received = recvfrom(rtpSocket_, buffer, sizeof(buffer), 0,
                                   (struct sockaddr*)&senderAddr, &senderLen);

        if (received > 0) {
            processRTPPacket(buffer, received);
        } else if (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            LOGE("RTP receive error: %s", strerror(errno));
            break;
        }
    }

    LOGD("RTP receiver loop ended");
}

void ZLRTSPClient::processRTPPacket(const uint8_t* data, size_t size) {
    if (size < RTP_HEADER_SIZE) {
        LOGW("RTP packet too small: %zu bytes", size);
        return;
    }

    // Parse RTP header
    uint8_t version = (data[0] >> 6) & 0x03;
    uint8_t padding = (data[0] >> 5) & 0x01;
    uint8_t extension = (data[0] >> 4) & 0x01;
    uint8_t csrcCount = data[0] & 0x0F;
    uint8_t marker = (data[1] >> 7) & 0x01;
    uint8_t payloadType = data[1] & 0x7F;
    uint16_t sequenceNumber = (data[2] << 8) | data[3];
    uint32_t timestamp = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    uint32_t ssrc = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];

    if (version != 2) {
        LOGW("Unsupported RTP version: %d", version);
        return;
    }

    if (payloadType != videoPayloadType_) {
        LOGW("Unexpected payload type: %d (expected %d)", payloadType, videoPayloadType_);
        return;
    }

    // Calculate payload offset
    size_t payloadOffset = RTP_HEADER_SIZE + (csrcCount * 4);
    if (extension) {
        if (size < payloadOffset + 4) return;
        uint16_t extLength = (data[payloadOffset + 2] << 8) | data[payloadOffset + 3];
        payloadOffset += 4 + (extLength * 4);
    }

    if (payloadOffset >= size) {
        LOGW("Invalid RTP packet: payload offset %zu >= size %zu", payloadOffset, size);
        return;
    }

    // Extract payload
    const uint8_t* payload = data + payloadOffset;
    size_t payloadSize = size - payloadOffset;

    if (padding && payloadSize > 0) {
        payloadSize -= data[size - 1]; // Remove padding
    }

    // Detailed RTP packet logging
    LOGD("RTP Packet: seq=%u, ts=%u, marker=%d, payload_size=%zu, ssrc=%u",
         sequenceNumber, timestamp, marker, payloadSize, ssrc);

    // Update statistics
    statistics_.framesReceived++;
    statistics_.bytesReceived += payloadSize;
    statistics_.lastFrameTimestamp = timestamp;

    // Process RTP packet for frame assembly
    processRTPPayload(payload, payloadSize, timestamp, sequenceNumber, marker, ssrc);
}

void ZLRTSPClient::processRTPPayload(const uint8_t* payload, size_t payloadSize,
                                    uint32_t timestamp, uint16_t sequenceNumber,
                                    bool marker, uint32_t ssrc) {
    std::lock_guard<std::mutex> lock(frameAssemblyMutex_);

    // Find or create frame assembly for this timestamp
    auto& frameAssembly = frameAssemblyMap_[timestamp];

    if (frameAssembly.timestamp == 0) {
        // New frame
        frameAssembly.timestamp = timestamp;
        frameAssembly.lastUpdate = std::chrono::steady_clock::now();
        frameAssembly.isComplete = false;
        frameAssembly.receivedSequences.clear();
        frameAssembly.data.clear();

        LOGD("Starting new frame assembly: ts=%u, seq=%u, payload_size=%zu",
             timestamp, sequenceNumber, payloadSize);
    }

    // Check for duplicate sequence number
    if (frameAssembly.receivedSequences.find(sequenceNumber) != frameAssembly.receivedSequences.end()) {
        LOGW("Duplicate RTP packet: ts=%u, seq=%u", timestamp, sequenceNumber);
        return;
    }

    // Add sequence number to received set
    frameAssembly.receivedSequences.insert(sequenceNumber);

    // Append payload data (simplified - should handle proper ordering)
    size_t oldSize = frameAssembly.data.size();
    frameAssembly.data.resize(oldSize + payloadSize);
    std::memcpy(frameAssembly.data.data() + oldSize, payload, payloadSize);

    LOGD("Added RTP payload: ts=%u, seq=%u, payload_size=%zu, total_size=%zu, marker=%d",
         timestamp, sequenceNumber, payloadSize, frameAssembly.data.size(), marker);

    // Check if frame is complete (marker bit indicates last packet)
    if (marker) {
        frameAssembly.isComplete = true;

        LOGI("Frame assembly complete: ts=%u, total_size=%zu, packets=%zu",
             timestamp, frameAssembly.data.size(), frameAssembly.receivedSequences.size());

        // Create complete video frame
        auto frame = createVideoFrame(frameAssembly.data.data(), frameAssembly.data.size(), timestamp, true);
        if (frame) {
            // Call frame callback
            std::lock_guard<std::mutex> callbackLock(callbackMutex_);
            if (frameCallback_) {
                frameCallback_(frame);
            }
        }

        // Remove completed frame from assembly map
        frameAssemblyMap_.erase(timestamp);
    }

    // Clean up old incomplete frames (older than 1 second)
    auto now = std::chrono::steady_clock::now();
    auto it = frameAssemblyMap_.begin();
    while (it != frameAssemblyMap_.end()) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.lastUpdate).count() > 1000) {
            LOGW("Removing incomplete frame: ts=%u, age=%lldms",
                 it->first, std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.lastUpdate).count());
            it = frameAssemblyMap_.erase(it);
        } else {
            ++it;
        }
    }
}

std::shared_ptr<VideoFrame> ZLRTSPClient::createVideoFrame(const uint8_t* data, size_t size,
                                                          uint32_t timestamp, bool isMarker) {
    auto frame = std::make_shared<VideoFrame>();

    // Allocate and copy data
    frame->data = std::make_unique<uint8_t[]>(size);
    std::memcpy(frame->data.get(), data, size);
    frame->dataSize = size;

    // Set frame properties
    frame->codec = videoCodec_;
    frame->timestamp = timestamp;
    frame->pts = timestamp;
    frame->dts = timestamp;
    frame->frameId = statistics_.framesReceived.load();
    frame->isKeyFrame = isMarker; // Simplified - should parse NAL units for H.264/H.265

    // Frame dimensions will be set by decoder
    frame->width = 0;
    frame->height = 0;
    frame->format = VideoFormat::UNKNOWN;

    return frame;
}

bool ZLRTSPClient::sendKeepAlive() {
    if (sessionId_.empty()) {
        return false;
    }

    std::ostringstream request;
    request << "GET_PARAMETER " << config_.rtspUrl << " " << RTSP_VERSION << "\r\n";
    request << "CSeq: " << cseq_++ << "\r\n";
    request << "Session: " << sessionId_ << "\r\n";
    request << "User-Agent: OjoRTSPClient/1.0\r\n";
    request << "\r\n";

    return sendRTSPRequest(request.str());
}

void ZLRTSPClient::updateStatistics() {
    auto now = std::chrono::steady_clock::now();
    static auto lastUpdate = now;

    auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate);
    if (timeDiff.count() > 1000) { // Update every second
        uint64_t currentFrames = statistics_.framesReceived.load();
        static uint64_t lastFrameCount = 0;

        double fps = (currentFrames - lastFrameCount) / (timeDiff.count() / 1000.0);
        statistics_.currentFps = fps;

        // Update average FPS (exponential moving average)
        double currentAvg = statistics_.averageFps.load();
        statistics_.averageFps = (currentAvg * 0.9) + (fps * 0.1);

        lastFrameCount = currentFrames;
        lastUpdate = now;

        // Call statistics callback
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (statisticsCallback_) {
            statisticsCallback_(getStatistics());
        }
    }
}

} // namespace ojo
