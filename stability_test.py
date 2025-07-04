#!/usr/bin/env python3
"""
ExoPlayer RTSP 稳定性测试脚本
用于长时间运行测试和性能监控
"""

import subprocess
import time
import json
import sys
import re
from datetime import datetime, timedelta

class StabilityTester:
    def __init__(self, package_name="it.danieleverducci.ojo"):
        self.package_name = package_name
        self.test_start_time = datetime.now()
        self.metrics = {
            "memory_samples": [],
            "cpu_samples": [],
            "crashes": 0,
            "anr_count": 0,
            "network_errors": 0,
            "player_errors": 0,
            "successful_connections": 0,
            "failed_connections": 0
        }
        
    def run_adb_command(self, command):
        """执行 ADB 命令并返回输出"""
        try:
            result = subprocess.run(f"adb {command}", shell=True, 
                                  capture_output=True, text=True, timeout=30)
            return result.stdout.strip() if result.returncode == 0 else None
        except subprocess.TimeoutExpired:
            print(f"⚠️  ADB command timeout: {command}")
            return None
        except Exception as e:
            print(f"❌ ADB command error: {e}")
            return None
    
    def check_device_connected(self):
        """检查设备是否连接"""
        devices = self.run_adb_command("devices")
        if devices and "device" in devices:
            print("✅ Android device connected")
            return True
        else:
            print("❌ No Android device connected")
            return False
    
    def install_app(self, apk_path):
        """安装应用"""
        print(f"📱 Installing app: {apk_path}")
        result = self.run_adb_command(f"install -r {apk_path}")
        if result is not None:
            print("✅ App installed successfully")
            return True
        else:
            print("❌ App installation failed")
            return False
    
    def start_app(self):
        """启动应用"""
        print("🚀 Starting app...")
        result = self.run_adb_command(f"shell am start -n {self.package_name}/.ui.MainActivity")
        if result is not None:
            print("✅ App started")
            time.sleep(5)  # Wait for app to fully start
            return True
        else:
            print("❌ Failed to start app")
            return False
    
    def get_memory_usage(self):
        """获取内存使用情况"""
        meminfo = self.run_adb_command(f"shell dumpsys meminfo {self.package_name}")
        if meminfo:
            # 解析内存信息
            lines = meminfo.split('\n')
            for line in lines:
                if "TOTAL" in line and "kB" in line:
                    # 提取总内存使用量
                    match = re.search(r'(\d+)', line)
                    if match:
                        memory_kb = int(match.group(1))
                        return memory_kb / 1024  # 转换为 MB
        return None
    
    def get_cpu_usage(self):
        """获取 CPU 使用率"""
        top_output = self.run_adb_command(f"shell top -n 1 | grep {self.package_name}")
        if top_output:
            # 解析 CPU 使用率
            match = re.search(r'(\d+\.?\d*)%', top_output)
            if match:
                return float(match.group(1))
        return None
    
    def check_for_crashes(self):
        """检查应用崩溃"""
        logcat = self.run_adb_command("logcat -d -s AndroidRuntime:E")
        if logcat and self.package_name in logcat:
            crash_count = logcat.count("FATAL EXCEPTION")
            return crash_count
        return 0
    
    def check_for_anr(self):
        """检查 ANR (Application Not Responding)"""
        logcat = self.run_adb_command("logcat -d -s ActivityManager:E")
        if logcat and "ANR" in logcat and self.package_name in logcat:
            anr_count = logcat.count("ANR in")
            return anr_count
        return 0
    
    def check_player_errors(self):
        """检查播放器错误"""
        logcat = self.run_adb_command("logcat -d -s SurveillanceFragment:E")
        if logcat:
            error_count = logcat.count("ExoPlayer error")
            return error_count
        return 0
    
    def check_network_errors(self):
        """检查网络错误"""
        logcat = self.run_adb_command("logcat -d | grep -i 'network\\|connection\\|timeout'")
        if logcat and self.package_name in logcat:
            error_count = logcat.count("error") + logcat.count("failed")
            return error_count
        return 0
    
    def collect_metrics(self):
        """收集性能指标"""
        timestamp = datetime.now()
        
        # 内存使用
        memory_mb = self.get_memory_usage()
        if memory_mb:
            self.metrics["memory_samples"].append({
                "timestamp": timestamp.isoformat(),
                "memory_mb": memory_mb
            })
        
        # CPU 使用
        cpu_percent = self.get_cpu_usage()
        if cpu_percent:
            self.metrics["cpu_samples"].append({
                "timestamp": timestamp.isoformat(),
                "cpu_percent": cpu_percent
            })
        
        # 错误统计
        self.metrics["crashes"] = self.check_for_crashes()
        self.metrics["anr_count"] = self.check_for_anr()
        self.metrics["player_errors"] = self.check_player_errors()
        self.metrics["network_errors"] = self.check_network_errors()
        
        return {
            "timestamp": timestamp.isoformat(),
            "memory_mb": memory_mb,
            "cpu_percent": cpu_percent,
            "crashes": self.metrics["crashes"],
            "anr_count": self.metrics["anr_count"],
            "player_errors": self.metrics["player_errors"],
            "network_errors": self.metrics["network_errors"]
        }
    
    def print_current_status(self, metrics):
        """打印当前状态"""
        runtime = datetime.now() - self.test_start_time
        print(f"\n📊 Runtime: {runtime}")
        print(f"💾 Memory: {metrics['memory_mb']:.1f}MB" if metrics['memory_mb'] else "💾 Memory: N/A")
        print(f"⚡ CPU: {metrics['cpu_percent']:.1f}%" if metrics['cpu_percent'] else "⚡ CPU: N/A")
        print(f"💥 Crashes: {metrics['crashes']}")
        print(f"⏰ ANRs: {metrics['anr_count']}")
        print(f"🎥 Player Errors: {metrics['player_errors']}")
        print(f"🌐 Network Errors: {metrics['network_errors']}")
    
    def generate_report(self):
        """生成测试报告"""
        runtime = datetime.now() - self.test_start_time
        
        # 计算平均值
        avg_memory = sum(s["memory_mb"] for s in self.metrics["memory_samples"]) / len(self.metrics["memory_samples"]) if self.metrics["memory_samples"] else 0
        avg_cpu = sum(s["cpu_percent"] for s in self.metrics["cpu_samples"]) / len(self.metrics["cpu_samples"]) if self.metrics["cpu_samples"] else 0
        
        # 计算最大值
        max_memory = max(s["memory_mb"] for s in self.metrics["memory_samples"]) if self.metrics["memory_samples"] else 0
        max_cpu = max(s["cpu_percent"] for s in self.metrics["cpu_samples"]) if self.metrics["cpu_samples"] else 0
        
        report = {
            "test_duration": str(runtime),
            "test_start": self.test_start_time.isoformat(),
            "test_end": datetime.now().isoformat(),
            "performance": {
                "memory": {
                    "average_mb": round(avg_memory, 2),
                    "max_mb": round(max_memory, 2),
                    "samples": len(self.metrics["memory_samples"])
                },
                "cpu": {
                    "average_percent": round(avg_cpu, 2),
                    "max_percent": round(max_cpu, 2),
                    "samples": len(self.metrics["cpu_samples"])
                }
            },
            "stability": {
                "crashes": self.metrics["crashes"],
                "anr_count": self.metrics["anr_count"],
                "player_errors": self.metrics["player_errors"],
                "network_errors": self.metrics["network_errors"]
            },
            "raw_metrics": self.metrics
        }
        
        return report
    
    def save_report(self, report, filename=None):
        """保存测试报告"""
        if not filename:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"stability_test_report_{timestamp}.json"
        
        with open(filename, 'w') as f:
            json.dump(report, f, indent=2)
        
        print(f"📄 Report saved to: {filename}")
    
    def run_stability_test(self, duration_hours=1, sample_interval=60):
        """运行稳定性测试"""
        print(f"🧪 Starting {duration_hours}h stability test...")
        print(f"📊 Sample interval: {sample_interval}s")
        
        if not self.check_device_connected():
            return False
        
        if not self.start_app():
            return False
        
        end_time = datetime.now() + timedelta(hours=duration_hours)
        sample_count = 0
        
        try:
            while datetime.now() < end_time:
                sample_count += 1
                print(f"\n📈 Sample #{sample_count}")
                
                metrics = self.collect_metrics()
                self.print_current_status(metrics)
                
                # 检查严重问题
                if metrics["crashes"] > 0:
                    print("🚨 CRITICAL: App crashed!")
                
                if metrics["anr_count"] > 0:
                    print("🚨 WARNING: ANR detected!")
                
                # 等待下一次采样
                time.sleep(sample_interval)
                
        except KeyboardInterrupt:
            print("\n⏹️  Test interrupted by user")
        
        # 生成并保存报告
        report = self.generate_report()
        self.save_report(report)
        
        # 打印总结
        print("\n" + "="*50)
        print("📋 STABILITY TEST SUMMARY")
        print("="*50)
        print(f"⏱️  Duration: {report['test_duration']}")
        print(f"💾 Avg Memory: {report['performance']['memory']['average_mb']:.1f}MB")
        print(f"💾 Max Memory: {report['performance']['memory']['max_mb']:.1f}MB")
        print(f"⚡ Avg CPU: {report['performance']['cpu']['average_percent']:.1f}%")
        print(f"⚡ Max CPU: {report['performance']['cpu']['max_percent']:.1f}%")
        print(f"💥 Total Crashes: {report['stability']['crashes']}")
        print(f"⏰ Total ANRs: {report['stability']['anr_count']}")
        print(f"🎥 Player Errors: {report['stability']['player_errors']}")
        print(f"🌐 Network Errors: {report['stability']['network_errors']}")
        
        # 评估结果
        if report['stability']['crashes'] == 0 and report['stability']['anr_count'] == 0:
            print("✅ RESULT: STABLE")
        elif report['stability']['crashes'] > 0:
            print("❌ RESULT: UNSTABLE (Crashes detected)")
        else:
            print("⚠️  RESULT: PARTIALLY STABLE (Minor issues)")
        
        return True

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 stability_test.py <duration_hours> [sample_interval_seconds]")
        print("Example: python3 stability_test.py 2 30")
        sys.exit(1)
    
    duration_hours = float(sys.argv[1])
    sample_interval = int(sys.argv[2]) if len(sys.argv) > 2 else 60
    
    tester = StabilityTester()
    tester.run_stability_test(duration_hours, sample_interval)

if __name__ == "__main__":
    main()
