#!/usr/bin/env python3
"""
ExoPlayer 迁移验证脚本
验证从 libvlc 到 ExoPlayer 的迁移是否完整和正确
"""

import os
import re
import sys
from pathlib import Path

class MigrationVerifier:
    def __init__(self, project_root):
        self.project_root = Path(project_root)
        self.errors = []
        self.warnings = []
        self.success_count = 0
        
    def log_error(self, message):
        self.errors.append(f"❌ ERROR: {message}")
        
    def log_warning(self, message):
        self.warnings.append(f"⚠️  WARNING: {message}")
        
    def log_success(self, message):
        self.success_count += 1
        print(f"✅ {message}")
        
    def check_dependencies(self):
        """检查 build.gradle 中的依赖项"""
        print("\n🔍 检查依赖项配置...")
        
        build_gradle = self.project_root / "app" / "build.gradle"
        if not build_gradle.exists():
            self.log_error("build.gradle 文件不存在")
            return
            
        content = build_gradle.read_text()
        
        # 检查是否移除了 VLC 依赖
        if "libvlc-android" in content:
            self.log_error("仍然包含 libvlc-android 依赖")
        else:
            self.log_success("已移除 libvlc-android 依赖")
            
        # 检查是否添加了 ExoPlayer 依赖
        if "com.google.android.exoplayer:exoplayer:" in content:
            self.log_success("已添加 ExoPlayer 核心依赖")
        else:
            self.log_error("缺少 ExoPlayer 核心依赖")
            
        if "com.google.android.exoplayer:exoplayer-rtsp:" in content:
            self.log_success("已添加 ExoPlayer RTSP 依赖")
        else:
            self.log_error("缺少 ExoPlayer RTSP 依赖")
            
        # 检查最低 API 版本
        if "minSdkVersion 16" in content:
            self.log_success("最低 API 版本已更新为 16")
        elif "minSdkVersion 15" in content:
            self.log_warning("最低 API 版本仍为 15，ExoPlayer 需要 API 16+")
        else:
            self.log_warning("无法确定最低 API 版本")
    
    def check_imports(self):
        """检查 Java 文件中的 import 语句"""
        print("\n🔍 检查 import 语句...")
        
        java_files = list(self.project_root.rglob("*.java"))
        vlc_imports_found = False
        exoplayer_imports_found = False
        
        for java_file in java_files:
            content = java_file.read_text()
            
            # 检查 VLC imports
            vlc_patterns = [
                r"import org\.videolan\.libvlc",
                r"import.*LibVLC",
                r"import.*MediaPlayer",
                r"import.*IVLCVout"
            ]
            
            for pattern in vlc_patterns:
                if re.search(pattern, content):
                    vlc_imports_found = True
                    self.log_error(f"在 {java_file.relative_to(self.project_root)} 中发现 VLC import")
                    
            # 检查 ExoPlayer imports
            exoplayer_patterns = [
                r"import com\.google\.android\.exoplayer2",
                r"import.*ExoPlayer",
                r"import.*RtspMediaSource"
            ]
            
            for pattern in exoplayer_patterns:
                if re.search(pattern, content):
                    exoplayer_imports_found = True
                    
        if not vlc_imports_found:
            self.log_success("未发现残留的 VLC import 语句")
            
        if exoplayer_imports_found:
            self.log_success("发现 ExoPlayer import 语句")
        else:
            self.log_error("未发现 ExoPlayer import 语句")
    
    def check_class_usage(self):
        """检查类的使用情况"""
        print("\n🔍 检查类使用情况...")
        
        surveillance_fragment = self.project_root / "app" / "src" / "main" / "java" / "it" / "danieleverducci" / "ojo" / "ui" / "SurveillanceFragment.java"
        
        if not surveillance_fragment.exists():
            self.log_error("SurveillanceFragment.java 文件不存在")
            return
            
        content = surveillance_fragment.read_text()
        
        # 检查 VLC 类使用
        vlc_classes = ["LibVLC", "MediaPlayer", "IVLCVout", "Media"]
        for vlc_class in vlc_classes:
            if re.search(rf"\b{vlc_class}\b", content):
                self.log_error(f"在 SurveillanceFragment 中仍在使用 VLC 类: {vlc_class}")
            else:
                self.log_success(f"已移除 VLC 类使用: {vlc_class}")
                
        # 检查 ExoPlayer 类使用
        exoplayer_classes = ["ExoPlayer", "MediaSource", "RtspMediaSource"]
        for exo_class in exoplayer_classes:
            if re.search(rf"\b{exo_class}\b", content):
                self.log_success(f"正在使用 ExoPlayer 类: {exo_class}")
            else:
                self.log_warning(f"未发现 ExoPlayer 类使用: {exo_class}")
    
    def check_error_handling(self):
        """检查错误处理实现"""
        print("\n🔍 检查错误处理实现...")
        
        surveillance_fragment = self.project_root / "app" / "src" / "main" / "java" / "it" / "danieleverducci" / "ojo" / "ui" / "SurveillanceFragment.java"
        
        if not surveillance_fragment.exists():
            return
            
        content = surveillance_fragment.read_text()
        
        # 检查错误处理相关代码
        error_handling_patterns = [
            (r"onPlayerError", "播放器错误处理"),
            (r"PlaybackException", "播放异常处理"),
            (r"retryCount", "重试计数机制"),
            (r"MAX_RETRY_ATTEMPTS", "最大重试次数"),
            (r"scheduleRestart", "重启调度机制"),
            (r"PlayerState", "播放器状态管理")
        ]
        
        for pattern, description in error_handling_patterns:
            if re.search(pattern, content):
                self.log_success(f"已实现{description}")
            else:
                self.log_warning(f"可能缺少{description}")
    
    def check_lifecycle_management(self):
        """检查生命周期管理"""
        print("\n🔍 检查生命周期管理...")
        
        surveillance_fragment = self.project_root / "app" / "src" / "main" / "java" / "it" / "danieleverducci" / "ojo" / "ui" / "SurveillanceFragment.java"
        
        if not surveillance_fragment.exists():
            return
            
        content = surveillance_fragment.read_text()
        
        # 检查生命周期方法
        lifecycle_patterns = [
            (r"onResume\(\)", "onResume 方法"),
            (r"onPause\(\)", "onPause 方法"),
            (r"startPlayback\(\)", "开始播放方法"),
            (r"pausePlayback\(\)", "暂停播放方法"),
            (r"destroy\(\)", "销毁方法"),
            (r"isDestroyed", "销毁状态检查")
        ]
        
        for pattern, description in lifecycle_patterns:
            if re.search(pattern, content):
                self.log_success(f"已实现{description}")
            else:
                self.log_warning(f"可能缺少{description}")
    
    def check_configuration(self):
        """检查配置参数"""
        print("\n🔍 检查配置参数...")
        
        surveillance_fragment = self.project_root / "app" / "src" / "main" / "java" / "it" / "danieleverducci" / "ojo" / "ui" / "SurveillanceFragment.java"
        
        if not surveillance_fragment.exists():
            return
            
        content = surveillance_fragment.read_text()
        
        # 检查配置常量
        config_patterns = [
            (r"RTSP_TIMEOUT_MS", "RTSP 超时配置"),
            (r"MAX_RETRY_ATTEMPTS", "最大重试次数配置"),
            (r"RETRY_DELAY_MS", "重试延迟配置")
        ]
        
        for pattern, description in config_patterns:
            if re.search(pattern, content):
                self.log_success(f"已配置{description}")
            else:
                self.log_warning(f"可能缺少{description}")
    
    def run_verification(self):
        """运行完整的验证流程"""
        print("🚀 开始 ExoPlayer 迁移验证...")
        print("=" * 50)
        
        self.check_dependencies()
        self.check_imports()
        self.check_class_usage()
        self.check_error_handling()
        self.check_lifecycle_management()
        self.check_configuration()
        
        print("\n" + "=" * 50)
        print("📊 验证结果汇总:")
        print(f"✅ 成功检查: {self.success_count}")
        print(f"⚠️  警告数量: {len(self.warnings)}")
        print(f"❌ 错误数量: {len(self.errors)}")
        
        if self.warnings:
            print("\n⚠️  警告详情:")
            for warning in self.warnings:
                print(f"  {warning}")
                
        if self.errors:
            print("\n❌ 错误详情:")
            for error in self.errors:
                print(f"  {error}")
            return False
        else:
            print("\n🎉 迁移验证通过！")
            return True

def main():
    if len(sys.argv) > 1:
        project_root = sys.argv[1]
    else:
        project_root = "."
        
    verifier = MigrationVerifier(project_root)
    success = verifier.run_verification()
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
