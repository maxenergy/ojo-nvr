# Native RTSP Implementation Guide
## Ojo Surveillance System - Hardware-Accelerated Video Pipeline

### Overview

This document provides comprehensive documentation for the native C++ RTSP implementation in the Ojo surveillance system. The implementation leverages Rockchip MPP (Media Process Platform) and ZLMediaKit to provide hardware-accelerated video streaming with superior performance compared to the previous MediaPlayer-based solution.

## Architecture Overview

### Multi-Tier Fallback System

The implementation uses an intelligent 3-tier fallback mechanism:

1. **Primary**: ExoPlayer with enhanced RTSP configuration
2. **Secondary**: OjoNativePlayer with MPP hardware acceleration  
3. **Tertiary**: Android MediaPlayer with optimized settings

### Native Components

#### 1. RTSP Client (`ojo_native/rtsp/`)
- **ZLRTSPClient**: ZLMediaKit-based RTSP streaming client
- **Features**: Connection management, automatic reconnection, error recovery
- **Performance**: Direct network handling with minimal latency

#### 2. MPP Hardware Decoder (`ojo_native/decoder/`)
- **MPPDecoder**: Rockchip MPP hardware-accelerated video decoding
- **Formats**: H.264, H.265, MJPEG support
- **Optimization**: Zero-copy processing, hardware buffer management

#### 3. Native Surface Renderer (`ojo_native/renderer/`)
- **NativeSurfaceRenderer**: Direct ANativeWindow rendering
- **Acceleration**: RGA hardware support with software fallback
- **Features**: Format conversion, scaling optimization

#### 4. JNI Bridge (`ojo_native/jni/`)
- **ojo_jni_bridge**: Java-C++ communication interface
- **Management**: Surface lifecycle, player state callbacks
- **Integration**: Seamless Android application integration

## Build Configuration

### Prerequisites
- Android NDK 27.0.12077973
- CMake 3.18.1 or later
- C++17 standard support
- Target: RK3588 platform (arm64-v8a primary)

### Build Process
```bash
# Clean build
./gradlew clean

# Build with native library
./gradlew assembleDebug

# Install on device
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

### Native Library Verification
```bash
# Check native library inclusion
unzip -l app/build/outputs/apk/debug/app-debug.apk | grep libojo_native.so

# Expected output:
# lib/arm64-v8a/libojo_native.so
# lib/armeabi-v7a/libojo_native.so
```

## API Usage

### Java Integration

#### OjoNativePlayer Class
```java
// Initialize native player
OjoNativePlayer nativePlayer = new OjoNativePlayer();

// Set up listener for state changes
nativePlayer.setListener(new OjoNativePlayer.PlayerListener() {
    @Override
    public void onStateChanged(int state, String message) {
        // Handle state changes
    }
    
    @Override
    public void onError(int errorCode, String message) {
        // Handle errors
    }
    
    @Override
    public void onStatistics(OjoNativePlayer.PlaybackStatistics stats) {
        // Monitor performance metrics
    }
});

// Create player with surface
if (nativePlayer.createPlayer(surfaceView.getHolder().getSurface())) {
    // Start RTSP stream
    nativePlayer.startStream("rtsp://192.168.31.64:8554/unicast");
}
```

#### Automatic Fallback Integration
The SurveillanceFragment automatically handles fallback:

```java
// ExoPlayer fails with SDP parsing error
// → Automatically switches to OjoNativePlayer
// → If native player fails, falls back to MediaPlayer
```

## Performance Monitoring

### Key Metrics
- **FPS**: Real-time frame rate monitoring
- **Frame Statistics**: Received/decoded frame counts
- **Buffer Health**: Buffer utilization and recovery
- **Hardware Efficiency**: Decoder performance metrics

### Logging
Monitor performance with logcat:
```bash
adb logcat | grep -E "(SurveillanceFragment|OjoNative|ExoPlayer)"
```

## Testing Procedures

### RTSP Stream Testing
1. **Test Streams**:
   - `rtsp://192.168.31.22:8554/unicast`
   - `rtsp://192.168.31.64:8554/unicast`

2. **Verification Steps**:
   - Install and launch application
   - Add camera with RTSP URL
   - Monitor logcat for connection status
   - Verify video playback quality

3. **Expected Behavior**:
   - ExoPlayer attempts connection first
   - On SDP parsing errors, switches to OjoNativePlayer
   - Smooth 1280x720 30fps playback
   - Hardware acceleration utilization

### Performance Validation
```bash
# Monitor application logs
adb logcat | grep -E "(Hardware decoder|FPS|Frame|Buffer)"

# Check hardware acceleration
adb logcat | grep "RK3588"
```

## Troubleshooting

### Common Issues

#### 1. Native Library Loading
**Symptom**: UnsatisfiedLinkError
**Solution**: Verify NDK configuration and library inclusion

#### 2. RTSP Connection Failures
**Symptom**: Connection timeout or SDP parsing errors
**Solution**: Check network connectivity and stream availability

#### 3. Hardware Acceleration Issues
**Symptom**: Software fallback warnings
**Solution**: Verify RK3588 platform and MPP library availability

### Debug Commands
```bash
# Check device architecture
adb shell getprop ro.product.cpu.abi

# Monitor memory usage
adb shell dumpsys meminfo it.danieleverducci.ojo

# Check hardware capabilities
adb shell dumpsys media.codec
```

## Integration Points

### Existing Application Compatibility
- **No UI Changes**: Maintains existing SurfaceView rendering
- **Configuration Preservation**: All camera settings preserved
- **Backward Compatibility**: Fallback ensures universal compatibility

### Performance Improvements
- **Reduced Latency**: Direct hardware access
- **Better Resource Management**: Optimized memory usage
- **Enhanced Stability**: Robust error handling

## Future Enhancements

### Planned Features
1. **Multi-stream Optimization**: Concurrent stream processing
2. **Adaptive Quality Control**: Dynamic bitrate adjustment
3. **Advanced Error Recovery**: Enhanced reconnection logic
4. **Performance Analytics**: Detailed metrics dashboard

### Scalability Considerations
- **Thread Pool Expansion**: Support for more concurrent streams
- **Memory Pool Optimization**: Larger buffer management
- **Network Optimization**: Adaptive streaming protocols
