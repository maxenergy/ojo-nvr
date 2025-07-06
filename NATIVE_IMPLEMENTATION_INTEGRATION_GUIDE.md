# Native Implementation Integration Guide
## MPP + ZLMediaKit Migration for Ojo RTSP Surveillance

### Overview

This guide provides step-by-step instructions for integrating the new native C++ implementation using Rockchip MPP and ZLMediaKit to replace the current Android MediaPlayer-based RTSP streaming.

## Prerequisites

### Development Environment
- Android Studio 4.2 or later
- Android NDK 23.1.7779620 or later
- CMake 3.18.1 or later
- Git for version control

### Target Platform
- Rockchip RK3588 Android device
- Android API level 21 or higher
- ARM64-v8a architecture (primary)
- ARMv7a architecture (fallback)

### Required Libraries
1. **ZLMediaKit** - RTSP client library
2. **Rockchip MPP** - Hardware video decoder
3. **RGA** - Hardware graphics acceleration
4. **Android NDK** - Native development kit

## Integration Steps

### Phase 1: Library Setup

#### 1.1 Download and Setup ZLMediaKit
```bash
cd app/src/main/cpp/3rdparty
git clone https://github.com/ZLMediaKit/ZLMediaKit.git zlmediakit
cd zlmediakit
git submodule update --init --recursive

# Build for Android
mkdir build_android
cd build_android
cmake -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a \
      -DANDROID_PLATFORM=android-21 \
      -DCMAKE_BUILD_TYPE=Release \
      ..
make -j$(nproc)
```

#### 1.2 Setup Rockchip MPP
```bash
cd app/src/main/cpp/3rdparty
# Download MPP from Rockchip official repository
git clone https://github.com/rockchip-linux/mpp.git
cd mpp
mkdir build_android
cd build_android
cmake -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a \
      -DANDROID_PLATFORM=android-21 \
      -DCMAKE_BUILD_TYPE=Release \
      ..
make -j$(nproc)
```

#### 1.3 Setup RGA Library
```bash
cd app/src/main/cpp/3rdparty
# Download RGA from Rockchip
git clone https://github.com/airockchip/librga.git rga
cd rga
# Follow RGA build instructions for Android
```

### Phase 2: Native Code Integration

#### 2.1 Update SurveillanceFragment
Replace the current MediaPlayer implementation with native player:

```java
// In SurveillanceFragment.java
private OjoNativePlayer nativePlayer;

private void initializeNativePlayer() {
    nativePlayer = new OjoNativePlayer();
    nativePlayer.setListener(new OjoNativePlayer.PlayerListener() {
        @Override
        public void onStateChanged(int state, String message) {
            runOnUiThread(() -> handleStateChange(state, message));
        }
        
        @Override
        public void onError(int errorCode, String message) {
            runOnUiThread(() -> handleError(errorCode, message));
        }
        
        @Override
        public void onStatistics(OjoNativePlayer.PlaybackStatistics stats) {
            runOnUiThread(() -> updateStatistics(stats));
        }
    });
}

private void startRTSPStream(String rtspUrl) {
    if (nativePlayer != null && surfaceView != null) {
        Surface surface = surfaceView.getHolder().getSurface();
        if (nativePlayer.createPlayer(surface)) {
            nativePlayer.startStream(rtspUrl);
        }
    }
}
```

#### 2.2 Update Camera Configuration
Modify camera configuration to use native player:

```java
private void configureCamera(Camera camera) {
    String rtspUrl = camera.getStreamUrl();
    
    // Configure native player
    OjoNativePlayer.StreamConfig config = new OjoNativePlayer.StreamConfig();
    config.rtspUrl = rtspUrl;
    config.enableHardwareDecoding = true;
    config.connectionTimeoutMs = 10000;
    config.maxRetryAttempts = 3;
    
    nativePlayer.setConfig(config);
    
    // Set display rectangle based on layout
    int[] location = new int[2];
    surfaceView.getLocationOnScreen(location);
    nativePlayer.setDisplayRect(
        location[0], location[1],
        surfaceView.getWidth(), surfaceView.getHeight()
    );
}
```

### Phase 3: Testing and Validation

#### 3.1 RTSP Stream Compatibility Testing

**Test Streams:**
- Primary: `rtsp://192.168.31.64:8554/unicast`
- Secondary: `rtsp://192.168.31.22:8554/unicast`

**Test Procedure:**
1. Build and install the application
2. Configure cameras with test RTSP URLs
3. Monitor logcat for native player events
4. Verify video playback quality and performance
5. Test concurrent stream playback
6. Validate error handling and recovery

#### 3.2 Performance Validation

**Metrics to Monitor:**
- Frame rate (target: 30 FPS)
- Frame drops (target: < 1%)
- CPU usage (target: < 40%)
- Memory usage (target: < 600MB)
- Network latency (target: < 100ms)

**Validation Commands:**
```bash
# Monitor application logs
adb logcat -s "OjoNative:*" "OjoRTSPClient:*" "OjoMPPDecoder:*"

# Monitor system performance
adb shell top -p $(adb shell pidof it.danieleverducci.ojo)

# Monitor network statistics
adb shell cat /proc/net/dev
```

#### 3.3 Error Handling Testing

**Test Scenarios:**
1. Network disconnection during playback
2. Invalid RTSP URLs
3. Codec incompatibility
4. Memory pressure conditions
5. Surface lifecycle changes

### Phase 4: Performance Optimization

#### 4.1 Hardware Acceleration Verification
```bash
# Check MPP decoder status
adb logcat | grep -E "(mpp|decoder|hardware)"

# Verify RGA acceleration
adb logcat | grep -E "(rga|acceleration)"

# Monitor frame processing
adb logcat | grep -E "(frame|render|surface)"
```

#### 4.2 Memory Optimization
- Monitor memory usage patterns
- Implement memory pool for frame buffers
- Optimize garbage collection impact
- Validate memory leak prevention

#### 4.3 Threading Optimization
- Verify thread pool efficiency
- Monitor thread contention
- Optimize task scheduling
- Validate concurrent stream handling

### Phase 5: Documentation and Deployment

#### 5.1 Update Architecture Documentation
Update `architect.md` with:
- New native architecture diagram
- Component interaction flows
- Performance characteristics
- Troubleshooting guide

#### 5.2 Create Build Instructions
Document complete build process:
- Environment setup
- Dependency management
- Build configuration
- Deployment procedures

#### 5.3 Performance Benchmarks
Document baseline performance:
- Frame rate measurements
- Latency characteristics
- Resource utilization
- Comparison with MediaPlayer

## Troubleshooting

### Common Issues

#### 1. Library Linking Errors
```bash
# Check library dependencies
adb shell ldd /data/app/*/lib/arm64/libojo_native.so

# Verify library loading
adb logcat | grep -E "(dlopen|library|symbol)"
```

#### 2. Surface Rendering Issues
```bash
# Monitor surface events
adb logcat | grep -E "(Surface|ANativeWindow|render)"

# Check surface format compatibility
adb logcat | grep -E "(format|pixel|buffer)"
```

#### 3. RTSP Connection Problems
```bash
# Test RTSP connectivity
adb shell "nc -v 192.168.31.64 8554"

# Monitor network events
adb logcat | grep -E "(network|rtsp|connection)"
```

### Performance Issues

#### 1. Frame Drops
- Check decoder queue size
- Verify surface buffer management
- Monitor thread pool utilization
- Validate memory allocation patterns

#### 2. High CPU Usage
- Profile decoder performance
- Check thread affinity settings
- Optimize format conversion
- Validate hardware acceleration

#### 3. Memory Leaks
- Monitor native heap usage
- Check frame buffer lifecycle
- Validate callback cleanup
- Test extended playback sessions

## Success Criteria

### Functional Requirements
- ✅ RTSP streams connect successfully
- ✅ Video playback is smooth and stable
- ✅ Error handling works correctly
- ✅ Multiple streams can play concurrently
- ✅ Surface lifecycle is handled properly

### Performance Requirements
- ✅ Frame rate: 30 FPS sustained
- ✅ Frame drops: < 1% under normal conditions
- ✅ CPU usage: < 40% for dual streams
- ✅ Memory usage: < 600MB total
- ✅ Startup time: < 3 seconds per stream

### Quality Requirements
- ✅ No visible stuttering or artifacts
- ✅ Audio/video synchronization maintained
- ✅ Graceful degradation under stress
- ✅ Robust error recovery
- ✅ Consistent performance over time

## Next Steps

1. **Complete Library Integration** - Finish ZLMediaKit and MPP setup
2. **Implement Missing Components** - Complete renderer and JNI bridge
3. **Comprehensive Testing** - Execute full test suite
4. **Performance Tuning** - Optimize based on test results
5. **Documentation** - Complete user and developer guides
6. **Deployment** - Prepare for production release

This migration will provide a robust, high-performance RTSP streaming solution that fully utilizes RK3588 hardware capabilities while eliminating current MediaPlayer limitations.
