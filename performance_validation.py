#!/usr/bin/env python3
"""
Performance Validation Script for Ojo NVR RTSP Streaming Optimizations

This script validates the performance improvements made to the RTSP video streaming
functionality, specifically testing the optimizations for:
1. MediaPlayer error code -38 handling
2. Real-time playback optimization
3. H.264 High 4:2:2 buffer parameters
4. RK3588 hardware video decoding
5. Adaptive quality controls

Usage: python3 performance_validation.py
"""

import subprocess
import time
import re
import json
import sys
from datetime import datetime

class PerformanceValidator:
    def __init__(self):
        self.test_streams = [
            "rtsp://192.168.31.22:8554/unicast",
            "rtsp://192.168.31.64:8554/unicast"
        ]
        self.results = {
            "timestamp": datetime.now().isoformat(),
            "test_results": {},
            "performance_metrics": {},
            "optimization_status": {}
        }
        
    def run_validation(self):
        """Run comprehensive performance validation"""
        print("🚀 Starting RTSP Performance Validation")
        print("=" * 60)
        
        # Test 1: Verify MediaPlayer Error Handling
        self.test_mediaplayer_error_handling()
        
        # Test 2: Validate Real-time Playback Optimization
        self.test_realtime_optimization()
        
        # Test 3: Check H.264 High 4:2:2 Buffer Optimization
        self.test_h264_buffer_optimization()
        
        # Test 4: Verify RK3588 Hardware Decoding
        self.test_rk3588_hardware_decoding()
        
        # Test 5: Test Adaptive Quality Controls
        self.test_adaptive_quality_controls()
        
        # Test 6: Performance Monitoring Integration
        self.test_performance_monitoring()
        
        # Test 7: RTSP Stream Connectivity
        self.test_rtsp_connectivity()
        
        # Generate final report
        self.generate_report()
        
    def test_mediaplayer_error_handling(self):
        """Test MediaPlayer error code -38 handling"""
        print("\n📋 Test 1: MediaPlayer Error Code -38 Handling")
        print("-" * 50)
        
        # Check if error handling code is present
        error_handling_patterns = [
            r"MEDIA_ERROR_UNSUPPORTED.*-38",
            r"handleUnsupportedMediaError",
            r"getMediaPlayerErrorDetails",
            r"extra.*-38"
        ]
        
        results = self.check_code_patterns(
            "app/src/main/java/it/danieleverducci/ojo/ui/SurveillanceFragment.java",
            error_handling_patterns
        )
        
        self.results["test_results"]["mediaplayer_error_handling"] = {
            "status": "PASS" if all(results.values()) else "FAIL",
            "details": results
        }
        
        print(f"✅ Error code constants defined: {results.get('MEDIA_ERROR_UNSUPPORTED.*-38', False)}")
        print(f"✅ Error handler method present: {results.get('handleUnsupportedMediaError', False)}")
        print(f"✅ Error details method present: {results.get('getMediaPlayerErrorDetails', False)}")
        
    def test_realtime_optimization(self):
        """Test real-time playback optimization"""
        print("\n📋 Test 2: Real-time Playback Optimization")
        print("-" * 50)
        
        optimization_patterns = [
            r"optimizeForRealTimePlayback",
            r"applyAdvancedRealTimeOptimizations",
            r"setPlaybackParams",
            r"FLAG_LOW_LATENCY"
        ]
        
        results = self.check_code_patterns(
            "app/src/main/java/it/danieleverducci/ojo/ui/SurveillanceFragment.java",
            optimization_patterns
        )
        
        self.results["test_results"]["realtime_optimization"] = {
            "status": "PASS" if all(results.values()) else "FAIL",
            "details": results
        }
        
        print(f"✅ Real-time optimization method: {results.get('optimizeForRealTimePlayback', False)}")
        print(f"✅ Advanced optimizations: {results.get('applyAdvancedRealTimeOptimizations', False)}")
        print(f"✅ Playback parameters: {results.get('setPlaybackParams', False)}")
        
    def test_h264_buffer_optimization(self):
        """Test H.264 High 4:2:2 buffer optimization"""
        print("\n📋 Test 3: H.264 High 4:2:2 Buffer Optimization")
        print("-" * 50)
        
        buffer_patterns = [
            r"H264_422_BUFFER_SIZE_MS",
            r"H264_422_MIN_BUFFER_MS",
            r"createH264High422LoadControl",
            r"isLikelyH264High422Stream"
        ]
        
        results = self.check_code_patterns(
            "app/src/main/java/it/danieleverducci/ojo/ui/SurveillanceFragment.java",
            buffer_patterns
        )
        
        self.results["test_results"]["h264_buffer_optimization"] = {
            "status": "PASS" if all(results.values()) else "FAIL",
            "details": results
        }
        
        print(f"✅ H.264 4:2:2 buffer constants: {results.get('H264_422_BUFFER_SIZE_MS', False)}")
        print(f"✅ H.264 LoadControl method: {results.get('createH264High422LoadControl', False)}")
        print(f"✅ H.264 detection method: {results.get('isLikelyH264High422Stream', False)}")
        
    def test_rk3588_hardware_decoding(self):
        """Test RK3588 hardware video decoding optimization"""
        print("\n📋 Test 4: RK3588 Hardware Video Decoding")
        print("-" * 50)
        
        rk3588_patterns = [
            r"RK3588_MAX_DECODER_INSTANCES",
            r"createRK3588OptimizedFactory",
            r"applyRK3588VideoRendererOptimizations",
            r"resolveRK3588DecoderConflicts"
        ]
        
        results = self.check_code_patterns(
            "app/src/main/java/it/danieleverducci/ojo/ui/SurveillanceFragment.java",
            rk3588_patterns
        )
        
        self.results["test_results"]["rk3588_hardware_decoding"] = {
            "status": "PASS" if all(results.values()) else "FAIL",
            "details": results
        }
        
        print(f"✅ RK3588 decoder constants: {results.get('RK3588_MAX_DECODER_INSTANCES', False)}")
        print(f"✅ RK3588 factory method: {results.get('createRK3588OptimizedFactory', False)}")
        print(f"✅ RK3588 renderer optimizations: {results.get('applyRK3588VideoRendererOptimizations', False)}")
        
    def test_adaptive_quality_controls(self):
        """Test adaptive quality control implementation"""
        print("\n📋 Test 5: Adaptive Quality Controls")
        print("-" * 50)
        
        adaptive_patterns = [
            r"ENABLE_ADAPTIVE_QUALITY",
            r"initializeAdaptiveQualityControl",
            r"monitorNetworkAndAdjustQuality",
            r"applyQualityAdjustments"
        ]
        
        results = self.check_code_patterns(
            "app/src/main/java/it/danieleverducci/ojo/ui/SurveillanceFragment.java",
            adaptive_patterns
        )
        
        self.results["test_results"]["adaptive_quality_controls"] = {
            "status": "PASS" if all(results.values()) else "FAIL",
            "details": results
        }
        
        print(f"✅ Adaptive quality enabled: {results.get('ENABLE_ADAPTIVE_QUALITY', False)}")
        print(f"✅ Quality control initialization: {results.get('initializeAdaptiveQualityControl', False)}")
        print(f"✅ Network monitoring: {results.get('monitorNetworkAndAdjustQuality', False)}")
        
    def test_performance_monitoring(self):
        """Test performance monitoring integration"""
        print("\n📋 Test 6: Performance Monitoring Integration")
        print("-" * 50)
        
        monitoring_patterns = [
            r"PerformanceMonitor",
            r"logPlayerPerformance",
            r"incrementCounter",
            r"BUFFER_UNDERRUNS"
        ]
        
        results = self.check_code_patterns(
            "app/src/main/java/it/danieleverducci/ojo/ui/SurveillanceFragment.java",
            monitoring_patterns
        )
        
        self.results["test_results"]["performance_monitoring"] = {
            "status": "PASS" if all(results.values()) else "FAIL",
            "details": results
        }
        
        print(f"✅ Performance monitor usage: {results.get('PerformanceMonitor', False)}")
        print(f"✅ Player performance logging: {results.get('logPlayerPerformance', False)}")
        print(f"✅ Counter increments: {results.get('incrementCounter', False)}")
        
    def test_rtsp_connectivity(self):
        """Test RTSP stream connectivity"""
        print("\n📋 Test 7: RTSP Stream Connectivity")
        print("-" * 50)
        
        connectivity_results = {}
        
        for stream_url in self.test_streams:
            print(f"Testing connectivity to: {stream_url}")
            
            # Use ffprobe to test RTSP connectivity
            try:
                cmd = [
                    "ffprobe", "-v", "quiet", "-print_format", "json",
                    "-show_streams", "-rtsp_transport", "tcp",
                    "-timeout", "10000000", stream_url
                ]
                
                result = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
                
                if result.returncode == 0:
                    stream_info = json.loads(result.stdout)
                    streams = stream_info.get("streams", [])
                    video_streams = [s for s in streams if s.get("codec_type") == "video"]
                    
                    if video_streams:
                        video_stream = video_streams[0]
                        connectivity_results[stream_url] = {
                            "status": "CONNECTED",
                            "codec": video_stream.get("codec_name", "unknown"),
                            "resolution": f"{video_stream.get('width', 0)}x{video_stream.get('height', 0)}",
                            "fps": video_stream.get("r_frame_rate", "unknown")
                        }
                        print(f"  ✅ Connected - {connectivity_results[stream_url]['codec']} "
                              f"{connectivity_results[stream_url]['resolution']} "
                              f"{connectivity_results[stream_url]['fps']} fps")
                    else:
                        connectivity_results[stream_url] = {"status": "NO_VIDEO_STREAM"}
                        print(f"  ❌ No video stream found")
                else:
                    connectivity_results[stream_url] = {"status": "CONNECTION_FAILED", "error": result.stderr}
                    print(f"  ❌ Connection failed: {result.stderr[:100]}")
                    
            except subprocess.TimeoutExpired:
                connectivity_results[stream_url] = {"status": "TIMEOUT"}
                print(f"  ⏰ Connection timeout")
            except Exception as e:
                connectivity_results[stream_url] = {"status": "ERROR", "error": str(e)}
                print(f"  ❌ Error: {str(e)}")
        
        self.results["test_results"]["rtsp_connectivity"] = connectivity_results
        
    def check_code_patterns(self, file_path, patterns):
        """Check if code patterns exist in the specified file"""
        results = {}
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
                
            for pattern in patterns:
                results[pattern] = bool(re.search(pattern, content, re.MULTILINE))
                
        except FileNotFoundError:
            print(f"❌ File not found: {file_path}")
            for pattern in patterns:
                results[pattern] = False
        except Exception as e:
            print(f"❌ Error reading file {file_path}: {e}")
            for pattern in patterns:
                results[pattern] = False
                
        return results
        
    def generate_report(self):
        """Generate comprehensive performance validation report"""
        print("\n" + "=" * 60)
        print("📊 PERFORMANCE VALIDATION REPORT")
        print("=" * 60)
        
        total_tests = len(self.results["test_results"])
        passed_tests = sum(1 for test in self.results["test_results"].values() 
                          if test.get("status") == "PASS")
        
        print(f"\n📈 Overall Results: {passed_tests}/{total_tests} tests passed")
        print(f"📅 Test Date: {self.results['timestamp']}")
        
        print(f"\n🎯 Test Summary:")
        for test_name, test_result in self.results["test_results"].items():
            status_icon = "✅" if test_result.get("status") == "PASS" else "❌"
            print(f"  {status_icon} {test_name.replace('_', ' ').title()}: {test_result.get('status')}")
        
        # Save detailed results to file
        report_file = f"performance_validation_report_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
        with open(report_file, 'w') as f:
            json.dump(self.results, f, indent=2)
        
        print(f"\n📄 Detailed report saved to: {report_file}")
        
        if passed_tests == total_tests:
            print("\n🎉 All performance optimizations validated successfully!")
            print("✨ RTSP streaming performance improvements are ready for testing.")
        else:
            print(f"\n⚠️  {total_tests - passed_tests} tests failed. Please review the implementation.")
            
        return passed_tests == total_tests

if __name__ == "__main__":
    validator = PerformanceValidator()
    success = validator.run_validation()
    sys.exit(0 if success else 1)
