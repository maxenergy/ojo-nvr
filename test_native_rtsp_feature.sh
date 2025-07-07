#!/bin/bash

echo "🔍 Testing Android Native RTSP Feature Branch"
echo "=============================================="

# Check device connection
echo "📱 Checking device connection..."
adb devices

# Clear previous logs
echo "🧹 Clearing previous logs..."
adb logcat -c

# Start Ojo app
echo "🚀 Starting Ojo app..."
adb shell am start -n it.danieleverducci.ojo/.ui.MainActivity

# Wait for app to start
echo "⏳ Waiting for app to start (5 seconds)..."
sleep 5

# Monitor logs for native RTSP functionality
echo "📊 Monitoring Native RTSP functionality..."
echo "Looking for:"
echo "  - ExoPlayer initialization and SDP parsing errors"
echo "  - Native MediaPlayer fallback activation"
echo "  - Surface management for MediaPlayer"
echo "  - Video rendering success"
echo ""

adb logcat -s SurveillanceFragment | while read line; do
    timestamp=$(echo "$line" | cut -d' ' -f2)
    
    # Highlight important events
    if echo "$line" | grep -q "ExoPlayer created"; then
        echo "[$timestamp] 🎯 [EXOPLAYER] ExoPlayer initialization"
    elif echo "$line" | grep -q "SDP parsing error detected"; then
        echo "[$timestamp] ⚠️  [SDP ERROR] SDP parsing error detected - triggering fallback"
    elif echo "$line" | grep -q "Switching to native MediaPlayer"; then
        echo "[$timestamp] 🔄 [FALLBACK] Switching to native MediaPlayer"
    elif echo "$line" | grep -q "Native MediaPlayer created"; then
        echo "[$timestamp] 🎮 [NATIVE] Native MediaPlayer created"
    elif echo "$line" | grep -q "Native MediaPlayer surface set successfully"; then
        echo "[$timestamp] 🎬 [SURFACE] Native MediaPlayer surface set successfully"
    elif echo "$line" | grep -q "Native MediaPlayer prepared"; then
        echo "[$timestamp] ✅ [PREPARED] Native MediaPlayer prepared"
    elif echo "$line" | grep -q "Native MediaPlayer started"; then
        echo "[$timestamp] ▶️  [STARTED] Native MediaPlayer started"
    elif echo "$line" | grep -q "Video should now be visible"; then
        echo "[$timestamp] 📺 [SUCCESS] Video should now be visible"
    elif echo "$line" | grep -q "MEDIA_ERROR_UNSUPPORTED"; then
        echo "[$timestamp] ❌ [ERROR] Media error unsupported"
    elif echo "$line" | grep -q "Alternative RTSP configuration"; then
        echo "[$timestamp] 🔧 [ALT CONFIG] Alternative RTSP configuration"
    elif echo "$line" | grep -q "MediaPlayer buffer"; then
        echo "[$timestamp] 📊 [BUFFER] MediaPlayer buffering status"
    elif echo "$line" | grep -q "ERROR"; then
        echo "[$timestamp] ❌ [ERROR] $(echo "$line" | grep -o 'ERROR.*')"
    fi
done
