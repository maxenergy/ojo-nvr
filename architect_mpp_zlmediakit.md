# Ojo RTSP Surveillance App - MPP + ZLMediaKit Architecture

## Executive Summary

This document describes the next-generation architecture for the Ojo RTSP Surveillance App using native C++ implementation with Rockchip MPP (Media Process Platform) and ZLMediaKit for RTSP streaming. This architecture addresses the current hardware-level frame drop issues and provides optimal performance on RK3588 platform.

## Architecture Overview

### Current vs Next-Generation Architecture

#### Current Architecture (MediaPlayer-based)
```
┌─────────────────────────────────────────────────────────────┐
│                    Android UI Layer (Java)                  │
├─────────────────────────────────────────────────────────────┤
│              MediaPlayer/ExoPlayer Framework                │
├─────────────────────────────────────────────────────────────┤
│                RK3588 Rockit Video Sink                    │
│              (Hardware - Frame Errors)                      │
├─────────────────────────────────────────────────────────────┤
│                  Surface Rendering                          │
└─────────────────────────────────────────────────────────────┘
```

**Issues:**
- Frame errors in RK3588 rockit video sink
- Limited control over video pipeline
- Periodic stuttering due to hardware-level frame drops

#### Next-Generation Architecture (MPP + ZLMediaKit)
```
┌─────────────────────────────────────────────────────────────┐
│                    Android UI Layer (Java)                  │
├─────────────────────────────────────────────────────────────┤
│                      JNI Bridge                             │
├─────────────────────────────────────────────────────────────┤
│                  Native C++ Layer                           │
│  ┌─────────────┬─────────────┬─────────────┬─────────────┐  │
│  │ ZLMediaKit  │ Rockchip    │ RGA Hardware│ ANativeWindow│  │
│  │ RTSP Client │ MPP Decoder │ Acceleration│ Rendering   │  │
│  └─────────────┴─────────────┴─────────────┴─────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                Thread Pool Management                       │
└─────────────────────────────────────────────────────────────┘
```

**Benefits:**
- Direct hardware access bypassing Android framework
- Complete control over video pipeline
- Elimination of rockit video sink issues
- Better performance and lower latency

## Component Architecture

### 1. Core Components

#### 1.1 ZLMediaKit RTSP Client (`ojo_native/rtsp/`)
- **Purpose**: Network RTSP streaming and protocol handling
- **Key Features**:
  - Robust RTSP connection management
  - Automatic reconnection logic
  - Error handling and recovery
  - Multi-stream support
- **Files**: `ZLRTSPClient.h/.cpp`

#### 1.2 Rockchip MPP Decoder (`ojo_native/decoder/`)
- **Purpose**: Hardware-accelerated video decoding
- **Key Features**:
  - Direct RK3588 hardware access
  - H.264/H.265/MJPEG support
  - Zero-copy frame processing
  - Hardware buffer management
- **Files**: `MPPDecoder.h/.cpp`

#### 1.3 Native Surface Renderer (`ojo_native/renderer/`)
- **Purpose**: Direct video frame rendering to Android Surface
- **Key Features**:
  - ANativeWindow direct rendering
  - RGA hardware acceleration
  - Format conversion optimization
  - Multi-stream layout management
- **Files**: `NativeSurfaceRenderer.h/.cpp`

#### 1.4 JNI Bridge (`ojo_native/jni/`)
- **Purpose**: Java-C++ communication interface
- **Key Features**:
  - Lifecycle management
  - Callback handling
  - Surface management
  - Configuration interface
- **Files**: `ojo_jni_bridge.h/.cpp`

### 2. Utility Components

#### 2.1 Thread Pool Management (`ojo_native/utils/`)
- **Purpose**: Concurrent stream processing
- **Key Features**:
  - Priority-based task scheduling
  - Resource management
  - Performance monitoring
  - Thread affinity optimization
- **Files**: `thread_pool.h/.cpp`

#### 2.2 Memory Management (`ojo_native/utils/`)
- **Purpose**: Efficient memory allocation and management
- **Key Features**:
  - Memory pool allocation
  - Zero-copy design
  - Smart pointer usage
  - Leak prevention
- **Files**: `memory_pool.h/.cpp`

#### 2.3 Performance Monitor (`ojo_native/utils/`)
- **Purpose**: Real-time performance monitoring
- **Key Features**:
  - Frame rate monitoring
  - Resource usage tracking
  - Error detection
  - Statistics collection
- **Files**: `performance_monitor.h/.cpp`

## Data Flow Architecture

### 1. RTSP Stream Processing Pipeline

```
RTSP Stream → ZLMediaKit Client → MPP Decoder → RGA Processor → ANativeWindow
     ↓              ↓                ↓             ↓              ↓
Network Thread → Decode Thread → Process Thread → Render Thread → Display
```

### 2. Threading Model

#### 2.1 Thread Allocation
- **RTSP Receiver Thread**: Network I/O and protocol handling
- **Decoder Thread Pool**: Hardware decoding (per stream)
- **Renderer Thread**: Surface rendering with VSync
- **Utility Thread**: Statistics and monitoring

#### 2.2 Thread Synchronization
- **Lock-free queues**: For frame data passing
- **Atomic operations**: For state management
- **Condition variables**: For thread coordination
- **Memory barriers**: For cache coherency

### 3. Memory Architecture

#### 3.1 Buffer Management
- **Input Buffers**: Network packet buffers (ZLMediaKit)
- **Decode Buffers**: MPP hardware buffers
- **Frame Buffers**: Decoded video frames
- **Render Buffers**: Surface rendering buffers

#### 3.2 Memory Optimization
- **Zero-copy design**: Direct buffer sharing
- **Memory pools**: Pre-allocated buffer pools
- **Smart pointers**: Automatic memory management
- **Buffer recycling**: Efficient buffer reuse

## Performance Characteristics

### 1. Target Performance Metrics

| Metric | Target | Current MediaPlayer | Expected MPP+ZLMediaKit |
|--------|--------|-------------------|------------------------|
| Frame Rate | 30 FPS | 25-28 FPS (drops) | 30 FPS (stable) |
| Frame Drops | < 1% | 5-10% | < 0.5% |
| CPU Usage | < 40% | 45-60% | 30-40% |
| Memory Usage | < 600MB | 400-500MB | 350-450MB |
| Latency | < 100ms | 150-200ms | 80-120ms |

### 2. Hardware Utilization

#### 2.1 RK3588 Optimization
- **MPP Decoder**: Direct hardware video decoding
- **RGA Acceleration**: Hardware format conversion
- **NEON Instructions**: ARM64 SIMD optimization
- **Cache Optimization**: Memory access patterns

#### 2.2 Resource Management
- **CPU Cores**: Efficient thread distribution
- **Memory Bandwidth**: Optimized data paths
- **GPU Resources**: Hardware acceleration
- **Power Management**: Efficient power usage

## Integration Strategy

### 1. Migration Phases

#### Phase 1: Foundation (Week 1)
- Set up native module structure
- Integrate ZLMediaKit and MPP dependencies
- Create basic JNI bridge
- Implement core data structures

#### Phase 2: Core Implementation (Weeks 2-3)
- Implement RTSP client wrapper
- Develop MPP decoder integration
- Create surface renderer
- Build thread pool management

#### Phase 3: Integration (Week 4)
- Integrate all components
- Implement Java interface
- Add error handling
- Performance optimization

#### Phase 4: Testing & Validation (Week 5)
- Comprehensive testing
- Performance benchmarking
- Documentation completion
- Production deployment

### 2. Compatibility Strategy

#### 2.1 Fallback Mechanism
- Keep MediaPlayer as fallback option
- Runtime capability detection
- Graceful degradation
- Error recovery

#### 2.2 Configuration Management
- Dynamic library loading
- Feature flag control
- Performance monitoring
- Adaptive optimization

## Quality Assurance

### 1. Testing Strategy

#### 1.1 Unit Testing
- Component isolation testing
- Mock object usage
- Automated test suites
- Continuous integration

#### 1.2 Integration Testing
- End-to-end pipeline testing
- Multi-stream scenarios
- Error condition testing
- Performance validation

#### 1.3 System Testing
- Real-world stream testing
- Extended duration testing
- Resource stress testing
- Platform compatibility

### 2. Performance Validation

#### 2.1 Benchmarking
- Frame rate measurements
- Latency analysis
- Resource utilization
- Comparative analysis

#### 2.2 Monitoring
- Real-time metrics
- Error tracking
- Performance alerts
- Trend analysis

## Deployment Strategy

### 1. Build Configuration

#### 1.1 CMake Setup
- Multi-architecture support
- Dependency management
- Optimization flags
- Debug configuration

#### 1.2 Android Integration
- Gradle configuration
- NDK integration
- Library packaging
- APK optimization

### 2. Distribution

#### 2.1 Library Management
- Shared library packaging
- Version management
- Update mechanisms
- Rollback procedures

#### 2.2 Platform Support
- RK3588 optimization
- ARM64 primary target
- ARMv7 fallback support
- x86 development support

## Future Enhancements

### 1. Advanced Features
- AI-powered video analytics
- Edge computing integration
- Cloud streaming support
- Advanced codec support

### 2. Platform Expansion
- Additional hardware platforms
- Cross-platform compatibility
- Performance optimization
- Feature enhancement

## Conclusion

The MPP + ZLMediaKit architecture provides a robust, high-performance solution for RTSP video streaming on RK3588 platform. By leveraging native hardware capabilities and eliminating Android framework limitations, this implementation delivers superior performance, reliability, and scalability for surveillance applications.

**Key Benefits:**
- ✅ Eliminates hardware-level frame drops
- ✅ Achieves optimal RK3588 performance
- ✅ Provides complete pipeline control
- ✅ Enables future enhancements
- ✅ Maintains compatibility and reliability
