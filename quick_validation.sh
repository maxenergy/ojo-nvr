#!/bin/bash

# Quick validation script for surface cross-contamination fixes
# This script performs basic validation of the implemented changes

echo "=== Ojo RTSP Surveillance - Quick Validation ==="
echo "Validating surface cross-contamination fixes..."
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${NC}"
}

# Check if device is connected
check_device() {
    print_status $BLUE "Checking device connection..."
    if ! adb devices | grep -q "device$"; then
        print_status $RED "❌ No Android device connected"
        return 1
    fi
    print_status $GREEN "✅ Device connected"
    return 0
}

# Install the app
install_app() {
    print_status $BLUE "Installing Ojo app..."
    if adb install -r app/build/outputs/apk/debug/app-debug.apk > /dev/null 2>&1; then
        print_status $GREEN "✅ App installed successfully"
        return 0
    else
        print_status $RED "❌ App installation failed"
        return 1
    fi
}

# Start the app
start_app() {
    print_status $BLUE "Starting Ojo app..."
    adb shell am start -n it.danieleverducci.ojo/.MainActivity > /dev/null 2>&1
    sleep 3
    
    local pid=$(adb shell pidof it.danieleverducci.ojo 2>/dev/null)
    if [ -n "$pid" ]; then
        print_status $GREEN "✅ App started (PID: $pid)"
        return 0
    else
        print_status $RED "❌ App failed to start"
        return 1
    fi
}

# Monitor for validation messages
monitor_validation() {
    print_status $BLUE "Monitoring for surface validation (30 seconds)..."
    print_status $YELLOW "Please navigate to the surveillance view in the app..."
    
    # Clear logcat and start monitoring
    adb logcat -c
    
    local validation_found=false
    local contamination_found=false
    local timeout=30
    local count=0
    
    while [ $count -lt $timeout ]; do
        # Check for validation messages in recent logs
        local recent_logs=$(adb logcat -d -s SurveillanceFragment:D SurfaceTestingUtils:I 2>/dev/null)
        
        if echo "$recent_logs" | grep -q "Surface management validation PASSED"; then
            print_status $GREEN "✅ Surface validation PASSED - No cross-contamination detected"
            validation_found=true
        fi
        
        if echo "$recent_logs" | grep -q "individual layout params"; then
            print_status $GREEN "✅ Individual layout parameters detected"
        fi
        
        if echo "$recent_logs" | grep -q "surface tag:"; then
            print_status $GREEN "✅ Surface tagging working"
        fi
        
        if echo "$recent_logs" | grep -q "CONTAMINATION DETECTED"; then
            print_status $RED "❌ Surface contamination detected!"
            contamination_found=true
        fi
        
        if echo "$recent_logs" | grep -q "LAYOUT PARAMETER SHARING"; then
            print_status $RED "❌ Layout parameter sharing detected!"
            contamination_found=true
        fi
        
        sleep 1
        count=$((count + 1))
        
        # Show progress
        if [ $((count % 5)) -eq 0 ]; then
            print_status $BLUE "Monitoring... ${count}/${timeout}s"
        fi
    done
    
    if [ "$validation_found" = true ] && [ "$contamination_found" = false ]; then
        print_status $GREEN "✅ Validation monitoring completed successfully"
        return 0
    elif [ "$contamination_found" = true ]; then
        print_status $RED "❌ Contamination issues detected"
        return 1
    else
        print_status $YELLOW "⚠️  No validation messages detected - app may not be in surveillance view"
        return 2
    fi
}

# Check for specific log patterns
check_log_patterns() {
    print_status $BLUE "Checking for expected log patterns..."
    
    local logs=$(adb logcat -d -s SurveillanceFragment:D SurfaceTestingUtils:I 2>/dev/null)
    local patterns_found=0
    
    # Check for individual layout parameters
    if echo "$logs" | grep -q "individual layout params"; then
        print_status $GREEN "✅ Individual layout parameters pattern found"
        patterns_found=$((patterns_found + 1))
    fi
    
    # Check for surface tagging
    if echo "$logs" | grep -q "surface tag:"; then
        print_status $GREEN "✅ Surface tagging pattern found"
        patterns_found=$((patterns_found + 1))
    fi
    
    # Check for surface registration
    if echo "$logs" | grep -q "Registered surface"; then
        print_status $GREEN "✅ Surface registration pattern found"
        patterns_found=$((patterns_found + 1))
    fi
    
    # Check for validation execution
    if echo "$logs" | grep -q "VALIDATING SURFACE MANAGEMENT"; then
        print_status $GREEN "✅ Surface validation execution found"
        patterns_found=$((patterns_found + 1))
    fi
    
    if [ $patterns_found -gt 0 ]; then
        print_status $GREEN "✅ Found $patterns_found expected log patterns"
        return 0
    else
        print_status $YELLOW "⚠️  No expected log patterns found - ensure app is in surveillance view"
        return 1
    fi
}

# Generate summary report
generate_summary() {
    print_status $BLUE "Generating validation summary..."
    
    local logs=$(adb logcat -d -s SurveillanceFragment:D SurfaceTestingUtils:I 2>/dev/null)
    local error_logs=$(adb logcat -d "*:E" 2>/dev/null | grep -E "(SurveillanceFragment|SurfaceTestingUtils)" || echo "")
    
    echo ""
    print_status $YELLOW "=== VALIDATION SUMMARY ==="
    
    # Count key events
    local surface_registrations=$(echo "$logs" | grep -c "Registered surface" || echo "0")
    local validations_passed=$(echo "$logs" | grep -c "validation PASSED" || echo "0")
    local validations_failed=$(echo "$logs" | grep -c "validation FAILED" || echo "0")
    local contaminations=$(echo "$logs" | grep -c "CONTAMINATION DETECTED" || echo "0")
    
    echo "Surface registrations: $surface_registrations"
    echo "Validations passed: $validations_passed"
    echo "Validations failed: $validations_failed"
    echo "Contamination events: $contaminations"
    
    if [ -n "$error_logs" ]; then
        echo ""
        print_status $RED "Errors detected:"
        echo "$error_logs" | head -5
    fi
    
    echo ""
    if [ $contaminations -eq 0 ] && [ $validations_failed -eq 0 ] && [ $validations_passed -gt 0 ]; then
        print_status $GREEN "🎉 OVERALL RESULT: Validation SUCCESSFUL - Fixes appear to be working"
    elif [ $contaminations -gt 0 ] || [ $validations_failed -gt 0 ]; then
        print_status $RED "❌ OVERALL RESULT: Issues detected - Further investigation needed"
    else
        print_status $YELLOW "⚠️  OVERALL RESULT: Inconclusive - Please ensure app is in surveillance view"
    fi
}

# Main execution
main() {
    echo "Starting quick validation of surface cross-contamination fixes..."
    echo ""
    
    # Check prerequisites
    if ! check_device; then
        exit 1
    fi
    
    # Install and start app
    if ! install_app; then
        exit 1
    fi
    
    if ! start_app; then
        exit 1
    fi
    
    # Monitor for validation
    monitor_validation
    local monitor_result=$?
    
    # Check log patterns
    check_log_patterns
    local pattern_result=$?
    
    # Generate summary
    generate_summary
    
    echo ""
    print_status $BLUE "Quick validation completed!"
    print_status $BLUE "For comprehensive testing, run: ./test_surface_cross_contamination.sh"
    
    # Return appropriate exit code
    if [ $monitor_result -eq 0 ] && [ $pattern_result -eq 0 ]; then
        exit 0
    else
        exit 1
    fi
}

# Run main function
main "$@"
