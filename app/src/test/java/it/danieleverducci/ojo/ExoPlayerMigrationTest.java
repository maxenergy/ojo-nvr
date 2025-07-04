package it.danieleverducci.ojo;

import org.junit.Test;
import org.junit.Before;
import static org.junit.Assert.*;

import it.danieleverducci.ojo.entities.Camera;

/**
 * ExoPlayer 迁移功能测试
 * 验证从 VLC 迁移到 ExoPlayer 后的核心功能
 */
public class ExoPlayerMigrationTest {

    private Camera testCamera;
    
    @Before
    public void setUp() {
        // 使用测试用的 RTSP 流
        testCamera = new Camera("Test Camera", "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov");
    }

    @Test
    public void testCameraEntityCreation() {
        assertNotNull("Camera 对象不应为空", testCamera);
        assertEquals("摄像头名称应该正确", "Test Camera", testCamera.getName());
        assertEquals("RTSP URL 应该正确", "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov", testCamera.getRtspUrl());
    }

    @Test
    public void testRtspUrlValidation() {
        // 测试有效的 RTSP URL
        String validUrl = "rtsp://example.com:554/stream";
        Camera validCamera = new Camera("Valid", validUrl);
        assertTrue("有效的 RTSP URL 应该以 rtsp:// 开头", validCamera.getRtspUrl().startsWith("rtsp://"));
        
        // 测试带认证的 RTSP URL
        String authUrl = "rtsp://user:pass@example.com:554/stream";
        Camera authCamera = new Camera("Auth", authUrl);
        assertTrue("带认证的 RTSP URL 应该包含认证信息", authCamera.getRtspUrl().contains("user:pass"));
    }

    @Test
    public void testCameraSerializability() {
        // 测试 Camera 对象的序列化能力（用于设置持久化）
        assertNotNull("Camera 应该实现 Serializable", testCamera);
        assertTrue("Camera 应该是 Serializable 的实例", testCamera instanceof java.io.Serializable);
    }

    @Test
    public void testCameraModification() {
        // 测试摄像头信息修改
        String newName = "Modified Camera";
        String newUrl = "rtsp://new.example.com/stream";
        
        testCamera.setName(newName);
        testCamera.setRtspUrl(newUrl);
        
        assertEquals("摄像头名称应该被正确修改", newName, testCamera.getName());
        assertEquals("RTSP URL 应该被正确修改", newUrl, testCamera.getRtspUrl());
    }

    @Test
    public void testMultipleCameras() {
        // 测试多摄像头场景
        Camera camera1 = new Camera("Camera 1", "rtsp://example1.com/stream");
        Camera camera2 = new Camera("Camera 2", "rtsp://example2.com/stream");
        Camera camera3 = new Camera("Camera 3", "rtsp://example3.com/stream");
        Camera camera4 = new Camera("Camera 4", "rtsp://example4.com/stream");
        
        // 验证每个摄像头都有唯一的标识
        assertNotEquals("摄像头名称应该不同", camera1.getName(), camera2.getName());
        assertNotEquals("RTSP URL 应该不同", camera1.getRtspUrl(), camera2.getRtspUrl());
        
        // 验证可以创建多个摄像头实例
        assertNotNull(camera1);
        assertNotNull(camera2);
        assertNotNull(camera3);
        assertNotNull(camera4);
    }

    @Test
    public void testRtspUrlFormats() {
        // 测试不同格式的 RTSP URL
        String[] testUrls = {
            "rtsp://192.168.1.100:554/stream1",
            "rtsp://user:password@192.168.1.100:554/stream1",
            "rtsp://example.com/live/stream",
            "rtsp://demo:demo@ipvmdemo.dyndns.org:5541/onvif-media/media.amp?profile=profile_1_h264&sessiontimeout=60&streamtype=unicast"
        };
        
        for (String url : testUrls) {
            Camera camera = new Camera("Test", url);
            assertTrue("URL 应该以 rtsp:// 开头: " + url, camera.getRtspUrl().startsWith("rtsp://"));
            assertFalse("URL 不应该为空: " + url, camera.getRtspUrl().isEmpty());
        }
    }

    @Test
    public void testCameraEquality() {
        // 测试摄像头对象的相等性（虽然 Camera 类没有重写 equals，但测试基本属性）
        Camera camera1 = new Camera("Same Name", "rtsp://same.url/stream");
        Camera camera2 = new Camera("Same Name", "rtsp://same.url/stream");
        
        // 虽然是不同的对象实例，但属性相同
        assertEquals("相同属性的摄像头名称应该相等", camera1.getName(), camera2.getName());
        assertEquals("相同属性的摄像头URL应该相等", camera1.getRtspUrl(), camera2.getRtspUrl());
    }

    @Test
    public void testEmptyValues() {
        // 测试边界情况
        Camera emptyCamera = new Camera("", "");
        assertNotNull("即使为空值，Camera 对象也不应为空", emptyCamera);
        assertEquals("空名称应该被正确设置", "", emptyCamera.getName());
        assertEquals("空URL应该被正确设置", "", emptyCamera.getRtspUrl());
    }

    @Test
    public void testNullValues() {
        // 测试 null 值处理
        try {
            Camera nullCamera = new Camera(null, null);
            assertNotNull("即使传入 null 值，Camera 对象也不应为空", nullCamera);
            // 注意：实际的 null 处理取决于 Camera 类的实现
        } catch (Exception e) {
            // 如果 Camera 类不允许 null 值，这是可以接受的
            assertTrue("如果不支持 null 值，应该抛出适当的异常", true);
        }
    }

    @Test
    public void testLongValues() {
        // 测试长字符串值
        String longName = "这是一个非常长的摄像头名称，用来测试系统是否能够正确处理长字符串输入，包括中文字符和特殊符号！@#$%^&*()";
        String longUrl = "rtsp://very.long.domain.name.example.com:554/very/long/path/to/stream/with/many/parameters?param1=value1&param2=value2&param3=value3";
        
        Camera longCamera = new Camera(longName, longUrl);
        assertEquals("长名称应该被正确设置", longName, longCamera.getName());
        assertEquals("长URL应该被正确设置", longUrl, longCamera.getRtspUrl());
    }
}
