# MPP + ZLMediaKit Migration Architecture for Ojo RTSP Surveillance

## Executive Summary

This document outlines the complete architecture migration strategy from Android MediaPlayer to a native C++ solution using Rockchip MPP (Media Process Platform) and ZLMediaKit for RTSP streaming. This migration aims to eliminate the RK3588 rockit video sink frame errors and achieve smooth, libvlc-quality video performance.

## Current vs Target Architecture

### Current Architecture (MediaPlayer-based)
```
Android UI Layer (Java)
    ↓
MediaPlayer/ExoPlayer (Android Framework)
    ↓
RK3588 Rockit Video Sink (Hardware)
    ↓
Surface Rendering (Android)
```

**Issues**:
- Frame errors in RK3588 rockit video sink
- Limited control over video pipeline
- Periodic stuttering due to hardware-level frame drops

### Target Architecture (MPP + ZLMediaKit)
```
Android UI Layer (Java)
    ↓ JNI Bridge
Native C++ Layer
    ├── ZLMediaKit RTSP Client
    ├── Rockchip MPP Decoder
    ├── RGA Hardware Acceleration
    └── ANativeWindow Direct Rendering
```

**Benefits**:
- Direct hardware access bypassing Android framework
- Complete control over video pipeline
- Elimination of rockit video sink issues
- Better performance and lower latency

## Reference Architecture Analysis

Based on `../yolov5rtspthreadpool/architect.md`, the reference implementation provides:

### Key Components to Adopt:
1. **ZLMediaKit Integration** (`3rdparty/zlmediakit/`)
   - RTSP client for network streaming
   - Robust connection management
   - Error handling and reconnection logic

2. **MPP Hardware Decoder** (`rkmedia/utils/mpp_decoder.cpp`)
   - Direct RK3588 hardware decoding
   - H.264 format support
   - Hardware-accelerated processing

3. **Thread Pool Management** (`task/yolov5_thread_pool.h`)
   - Concurrent stream processing
   - Resource management
   - Performance optimization

4. **Memory Management** (`include/user_comm.h`)
   - Smart pointer usage
   - Zero-copy design
   - Memory pool optimization

5. **JNI Bridge** (`native-lib.cpp`)
   - Java-C++ communication
   - Surface management
   - Lifecycle handling

## Detailed Migration Strategy

### Phase 1: Foundation Setup
1. **Native Module Structure**
   ```
   app/src/main/cpp/
   ├── ojo_native/
   │   ├── rtsp/
   │   │   ├── ZLRTSPClient.cpp/.h
   │   │   └── RTSPStreamManager.cpp/.h
   │   ├── decoder/
   │   │   ├── MPPDecoder.cpp/.h
   │   │   └── VideoFrameProcessor.cpp/.h
   │   ├── renderer/
   │   │   ├── NativeSurfaceRenderer.cpp/.h
   │   │   └── RGAProcessor.cpp/.h
   │   ├── jni/
   │   │   ├── ojo_jni_bridge.cpp/.h
   │   │   └── surface_manager.cpp/.h
   │   └── utils/
   │       ├── memory_pool.cpp/.h
   │       ├── thread_pool.cpp/.h
   │       └── performance_monitor.cpp/.h
   ```

2. **Dependencies Integration**
   - ZLMediaKit library compilation
   - Rockchip MPP SDK integration
   - RGA library for hardware acceleration
   - OpenCV for image processing (optional)

### Phase 2: Core Components Implementation

#### 2.1 RTSP Client (ZLMediaKit Integration)
```cpp
class OjoRTSPClient {
private:
    std::string rtspUrl_;
    std::unique_ptr<ZLMediaKit::PlayerProxy> player_;
    std::function<void(const VideoFrame&)> frameCallback_;
    
public:
    bool connect(const std::string& rtspUrl);
    void disconnect();
    void setFrameCallback(std::function<void(const VideoFrame&)> callback);
    bool isConnected() const;
    void reconnect();
};
```

#### 2.2 MPP Hardware Decoder
```cpp
class OjoMPPDecoder {
private:
    MppCtx ctx_;
    MppApi* mpi_;
    MppBufferGroup frameGroup_;
    
public:
    bool initialize(MppCodingType codecType);
    bool decode(const uint8_t* data, size_t size);
    std::shared_ptr<VideoFrame> getDecodedFrame();
    void cleanup();
};
```

#### 2.3 Native Surface Renderer
```cpp
class OjoNativeRenderer {
private:
    ANativeWindow* nativeWindow_;
    std::unique_ptr<RGAProcessor> rgaProcessor_;
    
public:
    bool initialize(ANativeWindow* window);
    bool renderFrame(const VideoFrame& frame);
    void setDisplayRect(int x, int y, int width, int height);
    void cleanup();
};
```

### Phase 3: JNI Bridge Implementation

#### 3.1 Java Interface
```java
public class OjoNativePlayer {
    static {
        System.loadLibrary("ojo_native");
    }
    
    private long nativePlayerHandle = 0;
    
    public native boolean createPlayer(Surface surface);
    public native boolean startStream(String rtspUrl);
    public native void stopStream();
    public native void destroyPlayer();
    public native boolean isPlaying();
    public native void setDisplayRect(int x, int y, int width, int height);
}
```

#### 3.2 JNI Implementation
```cpp
extern "C" {
    JNIEXPORT jboolean JNICALL
    Java_it_danieleverducci_ojo_OjoNativePlayer_createPlayer(
        JNIEnv* env, jobject thiz, jobject surface);
        
    JNIEXPORT jboolean JNICALL
    Java_it_danieleverducci_ojo_OjoNativePlayer_startStream(
        JNIEnv* env, jobject thiz, jstring rtspUrl);
        
    JNIEXPORT void JNICALL
    Java_it_danieleverducci_ojo_OjoNativePlayer_stopStream(
        JNIEnv* env, jobject thiz);
}
```

## Performance Optimizations

### 1. Memory Management
- **Smart Pointers**: Use `std::shared_ptr` for frame data sharing
- **Memory Pool**: Pre-allocate frame buffers to avoid runtime allocation
- **Zero-Copy**: Direct memory mapping between components

### 2. Threading Model
- **RTSP Receiver Thread**: Dedicated thread for network I/O
- **Decoder Thread**: Hardware decoding in separate thread
- **Renderer Thread**: Surface rendering with VSync synchronization
- **Main Thread**: UI updates and lifecycle management

### 3. Hardware Acceleration
- **MPP Decoder**: Direct RK3588 hardware decoding
- **RGA Processor**: Hardware-accelerated format conversion
- **ANativeWindow**: Direct surface rendering bypassing Android framework

## Migration Implementation Plan

### Week 1: Foundation
- [ ] Set up native module structure
- [ ] Integrate ZLMediaKit and MPP dependencies
- [ ] Create basic JNI bridge
- [ ] Implement core data structures

### Week 2: RTSP Client
- [ ] Implement ZLMediaKit RTSP client wrapper
- [ ] Add connection management and error handling
- [ ] Test RTSP connectivity with existing streams
- [ ] Implement reconnection logic

### Week 3: MPP Decoder
- [ ] Implement MPP hardware decoder wrapper
- [ ] Add H.264 decoding support
- [ ] Test decoder with RTSP stream data
- [ ] Optimize decoder performance

### Week 4: Surface Rendering
- [ ] Implement native surface renderer
- [ ] Add RGA hardware acceleration
- [ ] Test frame rendering to Android Surface
- [ ] Optimize rendering performance

### Week 5: Integration & Testing
- [ ] Integrate all components
- [ ] Test complete video pipeline
- [ ] Performance optimization and tuning
- [ ] Compare with MediaPlayer baseline

## Expected Outcomes

### Performance Improvements
- **Eliminate Frame Drops**: Direct hardware access bypasses rockit video sink issues
- **Reduce Latency**: Lower end-to-end latency through optimized pipeline
- **Improve Stability**: Better error handling and recovery mechanisms
- **Enhanced Control**: Complete control over video processing pipeline

### Technical Benefits
- **Hardware Utilization**: Full utilization of RK3588 capabilities
- **Scalability**: Support for multiple concurrent streams
- **Maintainability**: Modular architecture for easy maintenance
- **Extensibility**: Foundation for future enhancements

## Risk Mitigation

### Technical Risks
- **Complexity**: Native development increases complexity
- **Debugging**: More challenging debugging across JNI boundary
- **Compatibility**: Ensure compatibility across Android versions

### Mitigation Strategies
- **Incremental Development**: Implement and test components incrementally
- **Comprehensive Testing**: Extensive testing on target hardware
- **Fallback Mechanism**: Keep MediaPlayer as fallback option
- **Documentation**: Detailed documentation for maintenance

## Success Metrics

1. **Frame Drop Elimination**: Zero "RTNodeVideoSink frame error" messages
2. **Performance**: Smooth 30fps playback without stuttering
3. **Latency**: End-to-end latency < 100ms
4. **Stability**: 24+ hours continuous operation
5. **Resource Usage**: CPU usage < 40%, Memory < 600MB

This migration will provide a robust, high-performance RTSP streaming solution that fully utilizes the RK3588 hardware capabilities while eliminating current framework limitations.
