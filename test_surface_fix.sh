#!/bin/bash

echo "🔍 Testing Multi-Camera Surface Rendering Fix"
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

# Monitor logs for surface rendering
echo "📊 Monitoring surface rendering logs..."
echo "Looking for:"
echo "  - Multi-camera setup messages"
echo "  - Surface creation and conflicts"
echo "  - MediaPlayer initialization order"
echo "  - Video visibility confirmations"
echo ""

adb logcat -s SurveillanceFragment | while read line; do
    timestamp=$(echo "$line" | cut -d' ' -f2)
    
    # Highlight important events
    if echo "$line" | grep -q "🎥 Starting multi-camera setup"; then
        echo "[$timestamp] 🎯 [SETUP] Multi-camera initialization started"
    elif echo "$line" | grep -q "🎬 Surface created for CameraView"; then
        echo "[$timestamp] 🎯 [SURFACE] Surface created for camera view"
    elif echo "$line" | grep -q "✅ No surface conflicts detected"; then
        echo "[$timestamp] ✅ [SUCCESS] No surface conflicts"
    elif echo "$line" | grep -q "🚨 SURFACE CONFLICT DETECTED"; then
        echo "[$timestamp] 🚨 [ERROR] Surface conflict detected!"
    elif echo "$line" | grep -q "🎬 MediaPlayer initialization order"; then
        echo "[$timestamp] ⏳ [ORDER] MediaPlayer sequential initialization"
    elif echo "$line" | grep -q "Video should now be visible"; then
        echo "[$timestamp] 📺 [SUCCESS] Video should be visible"
    elif echo "$line" | grep -q "✅ MediaPlayer initialization completed"; then
        echo "[$timestamp] ✅ [SUCCESS] MediaPlayer initialization completed"
    elif echo "$line" | grep -q "❌"; then
        echo "[$timestamp] ❌ [ERROR] $(echo "$line" | grep -o '❌.*')"
    fi
done
