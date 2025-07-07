# RK3588 HDMI 交叉污染修复验证指南

## 🎯 验证目标

验证RK3588设备上的HDMI交叉污染修复是否成功解决了视频分屏区域的内容混乱问题。

## 📋 验证前准备

### 硬件要求
- ✅ RK3588设备（已安装修复版本APK）
- ✅ HDMI显示器或电视
- ✅ HDMI线缆
- ✅ 网络连接（用于RTSP摄像头访问）

### 软件要求
- ✅ 已安装的Ojo RTSP监控应用（修复版本）
- ✅ ADB调试工具（用于日志监控）
- ✅ scrcpy（用于对比测试）

## 🧪 验证步骤

### 第一阶段：基础功能验证

#### 1. **应用启动验证**
```bash
# 启动应用
adb shell am start -n it.danieleverducci.ojo/.ui.MainActivity

# 检查应用是否正常运行
adb shell pidof it.danieleverducci.ojo
```

**预期结果**: 应用正常启动并显示摄像头分屏界面

#### 2. **HDMI修复日志验证**
```bash
# 监控HDMI特定修复日志
adb logcat | grep -E "(HDMI.*safe|Applied HDMI|ExoPlayer.*HDMI)"
```

**预期日志**:
```
D SurveillanceFragment: Selecting HDMI-safe decoder for camera: ch1, MIME: video/avc
D SurveillanceFragment: HDMI-safe: Selected RK3588 hardware decoder c2.rk.avc.decoder for camera: ch1
D SurveillanceFragment: Applied HDMI surface isolation for camera: ch1 (Surface tag: camera_surface_ch1)
D SurveillanceFragment: ExoPlayer video surface attached with HDMI isolation for camera: ch1
```

### 第二阶段：HDMI交叉污染测试

#### 3. **HDMI显示验证**
**操作步骤**:
1. 确保RK3588设备连接到HDMI显示器
2. 观察4个分屏区域的视频内容
3. 记录每个区域显示的摄像头内容
4. 持续观察5-10分钟，注意是否有内容切换

**验证标准**:
- ✅ **区域1**: 始终显示ch1摄像头内容
- ✅ **区域2**: 始终显示ch2摄像头内容  
- ✅ **区域3**: 始终显示ch3摄像头内容
- ✅ **区域4**: 始终显示ch4摄像头内容
- ❌ **交叉污染**: 任何区域显示错误的摄像头内容

#### 4. **scrcpy对比验证**
**操作步骤**:
```bash
# 启动scrcpy进行远程显示
scrcpy
```

1. 同时观察HDMI显示器和scrcpy窗口
2. 验证两者显示的内容是否一致
3. 确认scrcpy显示正常（作为基准）

**验证标准**:
- ✅ **一致性**: HDMI显示应与scrcpy显示完全一致
- ✅ **稳定性**: 两种显示方式都应该稳定无交叉污染

### 第三阶段：表面管理验证

#### 5. **表面隔离验证**
```bash
# 检查表面验证日志
adb logcat | grep -E "(Surface.*validation.*PASSED|Surface.*isolation)"
```

**预期日志**:
```
I SurveillanceFragment: ✓ Surface management validation PASSED - No cross-contamination detected
I SurfaceTestingUtils: Surface isolation validation PASSED - 4 surfaces tracked, 4 cameras
```

#### 6. **硬件解码器分配验证**
```bash
# 监控解码器分配
adb logcat | grep -E "(decoder.*complete|RK3588.*decoder)"
```

**预期结果**: 每个摄像头都应该有独立的解码器实例分配

### 第四阶段：性能和稳定性测试

#### 7. **内存使用监控**
```bash
# 检查内存使用
adb shell dumpsys meminfo it.danieleverducci.ojo | head -10
```

**验证标准**: 内存使用应保持在合理范围（<100MB for 4 streams）

#### 8. **长期稳定性测试**
**操作步骤**:
1. 让应用在HDMI显示器上连续运行30分钟
2. 每5分钟检查一次视频内容分配
3. 监控是否出现交叉污染或其他异常

**验证标准**:
- ✅ **稳定性**: 30分钟内无交叉污染
- ✅ **性能**: 无明显性能下降
- ✅ **内存**: 内存使用保持稳定

## 📊 验证结果记录

### 测试结果表格
| 测试项目 | 预期结果 | 实际结果 | 状态 | 备注 |
|----------|----------|----------|------|------|
| 应用启动 | 正常启动 | | ⏳ | |
| HDMI修复日志 | 显示修复日志 | | ⏳ | |
| 区域1内容 | 显示ch1 | | ⏳ | |
| 区域2内容 | 显示ch2 | | ⏳ | |
| 区域3内容 | 显示ch3 | | ⏳ | |
| 区域4内容 | 显示ch4 | | ⏳ | |
| scrcpy一致性 | 与HDMI一致 | | ⏳ | |
| 表面隔离 | 验证通过 | | ⏳ | |
| 长期稳定性 | 30分钟无问题 | | ⏳ | |

### 问题记录
如果发现问题，请记录：
- **问题描述**: 
- **出现时间**: 
- **复现步骤**: 
- **相关日志**: 
- **影响程度**: 

## 🔧 故障排除

### 常见问题和解决方案

#### 问题1: HDMI修复日志未出现
**可能原因**: 
- 应用未正确加载修复代码
- 日志级别设置问题

**解决方案**:
```bash
# 重新安装APK
adb install -r app-debug.apk

# 清除日志并重新启动
adb logcat -c
adb shell am force-stop it.danieleverducci.ojo
adb shell am start -n it.danieleverducci.ojo/.ui.MainActivity
```

#### 问题2: 仍然出现交叉污染
**可能原因**:
- HDMI修复未完全生效
- 硬件解码器分配问题

**诊断步骤**:
```bash
# 检查解码器选择日志
adb logcat | grep -E "(decoder.*selection|MediaCodec)"

# 检查表面分配
adb logcat | grep -E "(surface.*allocation|Surface.*tag)"
```

#### 问题3: 性能下降
**可能原因**:
- 表面隔离增加了开销
- 解码器选择不当

**监控命令**:
```bash
# 监控CPU使用
adb shell top -p $(adb shell pidof it.danieleverducci.ojo)

# 监控内存使用
adb shell dumpsys meminfo it.danieleverducci.ojo
```

## 📞 支持和反馈

### 验证完成后
请提供以下信息：
1. **验证结果表格** (填写完整)
2. **关键日志片段** (特别是HDMI修复相关)
3. **问题记录** (如果有)
4. **性能数据** (内存使用、CPU使用)
5. **用户体验反馈** (主观感受)

### 成功标准
修复被认为成功当且仅当：
- ✅ **零交叉污染**: HDMI显示器上无任何交叉污染现象
- ✅ **scrcpy一致性**: HDMI显示与scrcpy显示完全一致
- ✅ **稳定性**: 长期运行无问题
- ✅ **性能**: 无明显性能回归

### 下一步行动
根据验证结果：
- **成功**: 可以部署到生产环境
- **部分成功**: 需要进一步调优
- **失败**: 需要重新分析和修复

---

**验证指南版本**: 1.0  
**创建时间**: 2025年7月8日 00:10  
**适用版本**: RK3588 HDMI修复版本
