#ifndef OJO_RGA_UTILS_H
#define OJO_RGA_UTILS_H

#include "ojo_types.h"
#include <memory>

#ifdef HAVE_RGA
#include "im2d.h"
#include "rga.h"
#include "RgaUtils.h"
#include "im2d.hpp"
#endif

namespace ojo {

/**
 * RGA (Rockchip Graphics Accelerator) utility class
 * Provides hardware-accelerated image processing operations
 */
class RGAUtils {
public:
    RGAUtils();
    ~RGAUtils();

    // Initialization and cleanup
    bool initialize();
    void cleanup();
    bool isAvailable() const;

    // Format conversion
    OjoError convertFormat(const VideoFrame& srcFrame, VideoFrame& dstFrame, 
                          VideoFormat dstFormat);
    
    // Scaling operations
    OjoError scaleFrame(const VideoFrame& srcFrame, VideoFrame& dstFrame,
                       int dstWidth, int dstHeight);
    
    // Combined format conversion and scaling
    OjoError convertAndScale(const VideoFrame& srcFrame, VideoFrame& dstFrame,
                            int dstWidth, int dstHeight, VideoFormat dstFormat);
    
    // Color space conversion (NV12 to RGB)
    OjoError nv12ToRgb(const VideoFrame& srcFrame, VideoFrame& dstFrame);
    
    // Rotation operations
    OjoError rotateFrame(const VideoFrame& srcFrame, VideoFrame& dstFrame,
                        int rotationDegrees);

    // Direct buffer operations (for performance)
    OjoError processBuffer(const void* srcBuffer, int srcWidth, int srcHeight, RGAFormat srcFormat,
                          void* dstBuffer, int dstWidth, int dstHeight, RGAFormat dstFormat);

    // Utility functions
    static RGAFormat videoFormatToRGA(VideoFormat format);
    static VideoFormat rgaFormatToVideo(RGAFormat format);
    static size_t calculateBufferSize(int width, int height, RGAFormat format);

private:
    std::unique_ptr<RGAContext> context_;
    
    // Internal helper methods
    bool initializeRGA();
    void cleanupRGA();
    
#ifdef HAVE_RGA
    // RGA-specific helper methods
    int mapVideoFormatToRGA(VideoFormat format);
    bool setupRGABuffers(const VideoFrame& srcFrame, const VideoFrame& dstFrame,
                        rga_buffer_t& srcImg, rga_buffer_t& dstImg);
    void releaseRGABuffers(rga_buffer_handle_t srcHandle, rga_buffer_handle_t dstHandle);
#endif
};

} // namespace ojo

#endif // OJO_RGA_UTILS_H
