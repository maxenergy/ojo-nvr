# Surface Cross-Contamination Testing Guide

## Overview

This document provides comprehensive testing procedures to validate the fixes for video cross-contamination issues in the Ojo RTSP surveillance Android application. The issue manifested as the first split window alternately displaying video content from multiple camera feeds, causing flickering and cross-contamination.

## Problem Description

**Original Issue:**
- First split window showed alternating video content from window 1 and window 2
- Flickering/flashing in the first window's rendered image
- Issue only occurred when viewing directly on device (not via scrcpy)
- Problem appeared with 3+ concurrent RTSP streams

**Root Cause:**
- Shared layout parameters between camera views
- Improper surface management during ExoPlayer → MediaPlayer transitions
- Lack of surface isolation between concurrent streams

## Fixes Implemented

### 1. Individual Layout Parameters
- Each `CameraView` now has its own `individualLayoutParams` instance
- Prevents layout modifications from affecting other camera views
- Eliminates shared reference issues

### 2. Surface Isolation
- Added unique surface tags for identification
- Proper parent container tracking (`originalParentContainer`)
- Enhanced surface recreation logic with individual parameters

### 3. Testing Framework
- `SurfaceTestingUtils` class for validation and monitoring
- Real-time surface assignment tracking
- Layout parameter isolation validation

## Testing Procedures

### Prerequisites

1. **Device Setup:**
   ```bash
   adb devices  # Ensure device is connected
   adb logcat -c  # Clear logcat buffer
   ```

2. **RTSP Streams:**
   - Ensure test streams are available:
     - `rtsp://192.168.31.22:8554/unicast`
     - `rtsp://192.168.31.64:8554/unicast`
   - Configure at least 3 cameras in app settings

3. **Build and Install:**
   ```bash
   ./gradlew assembleDebug
   adb install -r app/build/outputs/apk/debug/app-debug.apk
   ```

### Automated Testing

#### 1. Run Test Script
```bash
./test_surface_cross_contamination.sh
```

This script will:
- Monitor logcat for surface management events
- Validate surface isolation in real-time
- Generate detailed test reports
- Provide color-coded status updates

#### 2. Manual Logcat Monitoring
```bash
# Monitor surface management
adb logcat -s SurveillanceFragment:D SurfaceTestingUtils:I

# Monitor validation results
adb logcat | grep -E "(validation PASSED|validation FAILED|CONTAMINATION DETECTED)"

# Monitor surface assignments
adb logcat | grep -E "(surface tag|individual layout params|holder:)"
```

### Manual Testing Scenarios

#### Scenario 1: Multi-Stream Cross-Contamination Test
1. **Setup:** Configure 3 cameras in app settings
2. **Action:** Open surveillance view with all 3 streams
3. **Expected:** Each window shows only its assigned camera feed
4. **Validation:** Look for "Surface management validation PASSED" in logs

**Key Log Messages to Monitor:**
```
✓ Surface management validation PASSED - No cross-contamination detected
✓ Using individual layout parameters
ℹ Surface tracking: surface tag: camera_surface_Camera1
```

#### Scenario 2: Player Fallback Transition Test
1. **Setup:** Use RTSP streams that trigger ExoPlayer → MediaPlayer fallback
2. **Action:** Monitor player transitions during stream startup
3. **Expected:** Smooth transition without surface contamination
4. **Validation:** Check for proper surface recreation logs

**Key Log Messages to Monitor:**
```
Recreating SurfaceView for MediaPlayer compatibility for camera: Camera1
New SurfaceView added to original parent container with individual layout params
Native MediaPlayer surface set successfully for camera: Camera1
```

#### Scenario 3: Layout Toggle Test
1. **Setup:** Multiple cameras in grid view
2. **Action:** Toggle between fullscreen and multi-view modes
3. **Expected:** Layout changes don't affect other camera surfaces
4. **Validation:** Verify individual layout parameters are restored

**Key Log Messages to Monitor:**
```
Restored individual layout params for camera: Camera1
✓ Layout parameter isolation validation PASSED
```

#### Scenario 4: Surface Recreation Stress Test
1. **Setup:** Multiple active streams
2. **Action:** Force multiple ExoPlayer → MediaPlayer transitions
3. **Expected:** No surface holder conflicts or contamination
4. **Validation:** Check surface registry consistency

### Validation Criteria

#### ✅ Success Indicators
- `Surface management validation PASSED`
- `Layout parameter isolation validation PASSED`
- No `CONTAMINATION DETECTED` messages
- Each camera has unique surface tags
- Proper surface holder assignments

#### ❌ Failure Indicators
- `Surface management validation FAILED`
- `SURFACE CONTAMINATION DETECTED`
- `LAYOUT PARAMETER SHARING DETECTED`
- Multiple cameras sharing same surface holder
- Video content appearing in wrong windows

### Performance Impact Assessment

#### Memory Usage
```bash
# Monitor memory usage during testing
adb logcat | grep "logMemoryUsage"
```

#### Surface Count
```bash
# Check surface registry size
adb logcat | grep "Total registered surfaces"
```

#### Frame Rate Impact
- Monitor for any performance degradation
- Ensure 30fps target is maintained
- Check for additional buffer allocations

### Troubleshooting

#### Common Issues

1. **Surface Not Registered:**
   ```
   Warning: Surface was not tagged, assigned: untagged_surface_X
   ```
   - **Cause:** Surface creation without proper tagging
   - **Fix:** Ensure all surfaces get unique tags during creation

2. **Parent Container Not Found:**
   ```
   Error: Could not find original parent container for new SurfaceView
   ```
   - **Cause:** Surface recreation without proper parent tracking
   - **Fix:** Verify `originalParentContainer` is set correctly

3. **Layout Parameter Conflicts:**
   ```
   Error: LAYOUT PARAMETER SHARING DETECTED
   ```
   - **Cause:** Multiple views sharing same layout parameter instance
   - **Fix:** Ensure each view uses `individualLayoutParams`

#### Debug Commands

```bash
# Generate surface report
adb logcat | grep "SURFACE ASSIGNMENT REPORT" -A 20

# Check validation results
adb logcat | grep -E "(validation.*PASSED|validation.*FAILED)"

# Monitor surface lifecycle
adb logcat | grep -E "(Surface created|Surface destroyed)" | grep SurveillanceFragment
```

### Test Report Generation

The test script automatically generates detailed reports:
- **File:** `surface_test_report_YYYYMMDD_HHMMSS.log`
- **Contents:** Complete logcat output, validation results, error analysis
- **Location:** Current working directory

### Expected Results

After implementing the fixes, you should observe:

1. **No Cross-Contamination:** Each camera window displays only its intended video feed
2. **Stable Layout:** Layout changes don't affect other camera views
3. **Smooth Transitions:** ExoPlayer → MediaPlayer fallbacks work without surface issues
4. **Consistent Performance:** No degradation in frame rate or memory usage
5. **Clean Logs:** Validation passes without contamination warnings

### Next Steps

If tests pass:
1. Conduct extended testing with various RTSP stream types
2. Test on different Android versions and devices
3. Validate performance under stress conditions

If tests fail:
1. Review logcat output for specific failure points
2. Check surface assignment and lifecycle management
3. Verify layout parameter isolation
4. Consider additional surface isolation measures
