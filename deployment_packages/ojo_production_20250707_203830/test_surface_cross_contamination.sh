#!/bin/bash

# Test script for validating video cross-contamination fixes in Ojo RTSP surveillance app
# This script monitors logcat for surface management validation and cross-contamination issues

echo "=== Ojo RTSP Surveillance - Surface Cross-Contamination Test ==="
echo "Testing fixes for video feed cross-contamination in split window views"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
PACKAGE_NAME="it.danieleverducci.ojo"
TEST_DURATION=60  # seconds
LOGCAT_BUFFER_SIZE=10000

# Function to print colored output
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${NC}"
}

# Function to check if app is running
check_app_running() {
    local pid=$(adb shell pidof $PACKAGE_NAME 2>/dev/null)
    if [ -n "$pid" ]; then
        return 0
    else
        return 1
    fi
}

# Function to start monitoring
start_monitoring() {
    print_status $BLUE "Starting logcat monitoring for surface management validation..."
    
    # Clear logcat buffer
    adb logcat -c
    
    # Start logcat monitoring in background
    adb logcat -v time -s SurveillanceFragment:D SurfaceTestingUtils:I | while read line; do
        echo "$(date '+%H:%M:%S') | $line"
        
        # Check for validation results
        if echo "$line" | grep -q "Surface management validation PASSED"; then
            print_status $GREEN "✓ VALIDATION PASSED: No cross-contamination detected"
        elif echo "$line" | grep -q "Surface management validation FAILED"; then
            print_status $RED "✗ VALIDATION FAILED: Cross-contamination risk detected"
        elif echo "$line" | grep -q "SURFACE CONTAMINATION DETECTED"; then
            print_status $RED "✗ CRITICAL: Surface contamination detected!"
        elif echo "$line" | grep -q "LAYOUT PARAMETER SHARING DETECTED"; then
            print_status $RED "✗ CRITICAL: Layout parameter sharing detected!"
        elif echo "$line" | grep -q "individual layout params"; then
            print_status $GREEN "✓ Using individual layout parameters"
        elif echo "$line" | grep -q "surface tag"; then
            print_status $BLUE "ℹ Surface tracking: $(echo "$line" | grep -o 'surface tag: [^,]*')"
        fi
    done &
    
    LOGCAT_PID=$!
    echo "Logcat monitoring started (PID: $LOGCAT_PID)"
}

# Function to stop monitoring
stop_monitoring() {
    if [ -n "$LOGCAT_PID" ]; then
        kill $LOGCAT_PID 2>/dev/null
        print_status $BLUE "Logcat monitoring stopped"
    fi
}

# Function to run specific test scenarios
run_test_scenario() {
    local scenario=$1
    print_status $YELLOW "=== Testing Scenario: $scenario ==="
    
    case $scenario in
        "multi_stream")
            print_status $BLUE "Testing multiple concurrent RTSP streams..."
            print_status $BLUE "Please open 3 RTSP streams in the app and observe for cross-contamination"
            ;;
        "player_fallback")
            print_status $BLUE "Testing ExoPlayer to MediaPlayer fallback..."
            print_status $BLUE "Monitor for proper surface recreation during player transitions"
            ;;
        "layout_toggle")
            print_status $BLUE "Testing fullscreen/multi-view toggle..."
            print_status $BLUE "Please toggle between fullscreen and multi-view modes"
            ;;
        "surface_recreation")
            print_status $BLUE "Testing surface recreation during player switch..."
            print_status $BLUE "Monitor for proper surface isolation during transitions"
            ;;
    esac
}

# Function to analyze test results
analyze_results() {
    print_status $YELLOW "=== Analyzing Test Results ==="
    
    # Get recent logs for analysis
    local recent_logs=$(adb logcat -d -v time -s SurveillanceFragment:D SurfaceTestingUtils:I | tail -100)
    
    # Count validation results
    local pass_count=$(echo "$recent_logs" | grep -c "validation PASSED" || echo "0")
    local fail_count=$(echo "$recent_logs" | grep -c "validation FAILED" || echo "0")
    local contamination_count=$(echo "$recent_logs" | grep -c "CONTAMINATION DETECTED" || echo "0")
    
    print_status $BLUE "Validation Results:"
    print_status $GREEN "  - Passed validations: $pass_count"
    print_status $RED "  - Failed validations: $fail_count"
    print_status $RED "  - Contamination events: $contamination_count"
    
    # Overall assessment
    if [ $contamination_count -eq 0 ] && [ $fail_count -eq 0 ] && [ $pass_count -gt 0 ]; then
        print_status $GREEN "✓ OVERALL RESULT: Cross-contamination fixes appear to be working correctly"
    elif [ $contamination_count -gt 0 ] || [ $fail_count -gt 0 ]; then
        print_status $RED "✗ OVERALL RESULT: Cross-contamination issues detected - further investigation needed"
    else
        print_status $YELLOW "? OVERALL RESULT: Insufficient data - please run longer test or trigger more events"
    fi
}

# Function to generate detailed report
generate_report() {
    local report_file="surface_test_report_$(date +%Y%m%d_%H%M%S).log"
    
    print_status $BLUE "Generating detailed test report: $report_file"
    
    {
        echo "=== Ojo RTSP Surveillance - Surface Cross-Contamination Test Report ==="
        echo "Generated: $(date)"
        echo "Test Duration: $TEST_DURATION seconds"
        echo ""
        
        echo "=== Surface Management Logs ==="
        adb logcat -d -v time -s SurveillanceFragment:D SurfaceTestingUtils:I
        
        echo ""
        echo "=== Error Logs ==="
        adb logcat -d -v time "*:E" | grep -E "(SurveillanceFragment|SurfaceTestingUtils|surface|contamination)"
        
    } > "$report_file"
    
    print_status $GREEN "Report saved to: $report_file"
}

# Main test execution
main() {
    print_status $BLUE "Checking device connection..."
    if ! adb devices | grep -q "device$"; then
        print_status $RED "Error: No Android device connected"
        exit 1
    fi
    
    print_status $BLUE "Checking if Ojo app is running..."
    if ! check_app_running; then
        print_status $YELLOW "Warning: Ojo app is not running. Please start the app and navigate to surveillance view."
        print_status $BLUE "Waiting for app to start..."
        
        # Wait for app to start
        local wait_count=0
        while ! check_app_running && [ $wait_count -lt 30 ]; do
            sleep 1
            wait_count=$((wait_count + 1))
        done
        
        if ! check_app_running; then
            print_status $RED "Error: App did not start within 30 seconds"
            exit 1
        fi
    fi
    
    print_status $GREEN "Ojo app is running"
    
    # Start monitoring
    start_monitoring
    
    # Run test scenarios
    run_test_scenario "multi_stream"
    sleep 15
    
    run_test_scenario "player_fallback"
    sleep 15
    
    run_test_scenario "layout_toggle"
    sleep 15
    
    run_test_scenario "surface_recreation"
    sleep 15
    
    print_status $BLUE "Test monitoring will continue for $TEST_DURATION seconds..."
    print_status $BLUE "Please interact with the app to trigger various scenarios"
    
    # Wait for test duration
    sleep $TEST_DURATION
    
    # Stop monitoring and analyze
    stop_monitoring
    analyze_results
    generate_report
    
    print_status $GREEN "Test completed successfully!"
}

# Trap to ensure cleanup on exit
trap 'stop_monitoring; exit' INT TERM

# Run main function
main "$@"
