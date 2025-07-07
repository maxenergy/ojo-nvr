#ifndef MPP_WRAPPER_H
#define MPP_WRAPPER_H

// MPP wrapper header to prevent conflicts with Android NDK
// This provides MPP type definitions without including problematic headers

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for MPP types to avoid header conflicts
typedef void* MppCtx;
typedef void* MppFrame;
typedef void* MppPacket;
typedef void* MppBuffer;
typedef void* MppBufferGroup;

// MPP return codes
typedef enum {
    MPP_OK                      = 0,
    MPP_NOK                     = -1,
    MPP_ERR_UNKNOW              = -2,
    MPP_ERR_NULL_PTR            = -3,
    MPP_ERR_MALLOC              = -4,
    MPP_ERR_OPEN_FILE           = -5,
    MPP_ERR_VALUE               = -6,
    MPP_ERR_READ_BIT            = -7,
    MPP_ERR_TIMEOUT             = -8,
    MPP_ERR_PERM                = -9,
    MPP_ERR_BASE                = -1000,

    // Stream processing errors
    MPP_ERR_LIST_STREAM         = MPP_ERR_BASE - 1,
    MPP_ERR_INIT                = MPP_ERR_BASE - 2,
    MPP_ERR_VPU_CODEC_INIT      = MPP_ERR_BASE - 3,
    MPP_ERR_STREAM              = MPP_ERR_BASE - 4,
    MPP_ERR_FATAL_THREAD        = MPP_ERR_BASE - 5,
    MPP_ERR_NOMEM               = MPP_ERR_BASE - 6,
    MPP_ERR_PROTOL              = MPP_ERR_BASE - 7,
    MPP_FAIL_SPLIT_FRAME        = MPP_ERR_BASE - 8,
    MPP_ERR_VPUHW               = MPP_ERR_BASE - 9,
    MPP_EOS_STREAM_REACHED      = MPP_ERR_BASE - 11,
    MPP_ERR_BUFFER_FULL         = MPP_ERR_BASE - 12,
    MPP_ERR_DISPLAY_FULL        = MPP_ERR_BASE - 13,
} MPP_RET;

// MPP coding types
typedef enum {
    MPP_VIDEO_CodingUnused,
    MPP_VIDEO_CodingAutoDetect,
    MPP_VIDEO_CodingMPEG2,
    MPP_VIDEO_CodingH263,
    MPP_VIDEO_CodingMPEG4,
    MPP_VIDEO_CodingWMV,
    MPP_VIDEO_CodingRV,
    MPP_VIDEO_CodingAVC,
    MPP_VIDEO_CodingMJPEG,
    MPP_VIDEO_CodingVP8,
    MPP_VIDEO_CodingVP9,
    MPP_VIDEO_CodingHEVC,
} MppCodingType;

// MPP task types
typedef enum {
    MPP_CTX_DEC,
    MPP_CTX_ENC,
    MPP_CTX_ISP,
    MPP_CTX_BUTT,
} MppCtxType;

// MPP decoder configuration type
typedef void* MppDecCfg;

// MPP frame format
typedef enum {
    MPP_FMT_YUV420SP = 0,
    MPP_FMT_YUV420P = 1,
    MPP_FMT_YUV422_YUYV = 2,
    MPP_FMT_YUV422_YVYU = 3,
    MPP_FMT_YUV422_UYVY = 4,
    MPP_FMT_YUV422_VYUY = 5,
    MPP_FMT_YUV422P = 6,
    MPP_FMT_YUV422SP = 7,
    MPP_FMT_YUV444P = 8,
    MPP_FMT_YUV444SP = 9,
    MPP_FMT_RGB565 = 10,
    MPP_FMT_BGR565 = 11,
    MPP_FMT_RGB555 = 12,
    MPP_FMT_BGR555 = 13,
    MPP_FMT_RGB444 = 14,
    MPP_FMT_BGR444 = 15,
    MPP_FMT_RGB888 = 16,
    MPP_FMT_BGR888 = 17,
    MPP_FMT_RGB101010 = 18,
    MPP_FMT_BGR101010 = 19,
    MPP_FMT_ARGB8888 = 20,
    MPP_FMT_ABGR8888 = 21,
    MPP_FMT_BGRA8888 = 22,
    MPP_FMT_RGBA8888 = 23,
} MppFrameFormat;

// MPP buffer type
typedef enum {
    MPP_BUFFER_TYPE_NORMAL = 0,
    MPP_BUFFER_TYPE_ION = 1,
    MPP_BUFFER_TYPE_DRM = 2,
    MPP_BUFFER_TYPE_EXT_DMA = 3,
} MppBufferType;

// MPP decoder control commands
typedef enum {
    MPP_DEC_GET_CFG = 0x100,
    MPP_DEC_SET_CFG = 0x101,
    MPP_DEC_SET_EXT_BUF_GROUP = 0x102,
    MPP_DEC_SET_INFO_CHANGE_READY = 0x103,
} MppDecCmd;

// MPP API structure (simplified for stub implementation)
typedef struct MppApiStruct {
    MPP_RET (*decode_put_packet)(MppCtx ctx, MppPacket packet);
    MPP_RET (*decode_get_frame)(MppCtx ctx, MppFrame *frame);
    MPP_RET (*control)(MppCtx ctx, int cmd, void *param);
    MPP_RET (*reset)(MppCtx ctx);
} *MppApi;

// Function declarations for MPP API (to be linked at runtime)
#ifdef HAVE_ROCKCHIP_MPP

// Core MPP functions
MPP_RET mpp_create(MppCtx *ctx, MppApi **mpi);
MPP_RET mpp_init(MppCtx ctx, MppCtxType type, MppCodingType coding);
MPP_RET mpp_destroy(MppCtx ctx);

// Frame and packet functions
MPP_RET mpp_frame_init(MppFrame *frame);
MPP_RET mpp_frame_deinit(MppFrame *frame);
MPP_RET mpp_packet_init(MppPacket *packet, void *data, size_t size);
MPP_RET mpp_packet_deinit(MppPacket *packet);

// Packet manipulation functions
MPP_RET mpp_packet_set_data(MppPacket packet, void *data);
MPP_RET mpp_packet_set_size(MppPacket packet, size_t size);
MPP_RET mpp_packet_set_pos(MppPacket packet, void *pos);
MPP_RET mpp_packet_set_length(MppPacket packet, size_t length);
MPP_RET mpp_packet_set_eos(MppPacket packet);

// Frame query functions
uint32_t mpp_frame_get_width(MppFrame frame);
uint32_t mpp_frame_get_height(MppFrame frame);
uint32_t mpp_frame_get_hor_stride(MppFrame frame);
uint32_t mpp_frame_get_ver_stride(MppFrame frame);
uint32_t mpp_frame_get_buf_size(MppFrame frame);
int64_t mpp_frame_get_pts(MppFrame frame);
int64_t mpp_frame_get_dts(MppFrame frame);
uint32_t mpp_frame_get_eos(MppFrame frame);
uint32_t mpp_frame_get_info_change(MppFrame frame);
uint32_t mpp_frame_get_errinfo(MppFrame frame);
uint32_t mpp_frame_get_discard(MppFrame frame);
MppFrameFormat mpp_frame_get_fmt(MppFrame frame);
MppBuffer mpp_frame_get_buffer(MppFrame frame);

// Buffer functions
MPP_RET mpp_buffer_group_get(MppBufferGroup *group, MppBufferType type);
MPP_RET mpp_buffer_group_put(MppBufferGroup group);
MPP_RET mpp_buffer_group_clear(MppBufferGroup group);
MPP_RET mpp_buffer_group_limit_config(MppBufferGroup group, size_t size, int count);
size_t mpp_buffer_group_usage(MppBufferGroup group);
MPP_RET mpp_buffer_get(MppBufferGroup group, MppBuffer *buffer, size_t size);
MPP_RET mpp_buffer_put(MppBuffer buffer);
void* mpp_buffer_get_ptr_with_caller(MppBuffer buffer, const char* caller);
int mpp_buffer_get_fd_with_caller(MppBuffer buffer, const char* caller);

// Convenience macros for buffer functions
#define mpp_buffer_get_ptr(buffer) mpp_buffer_get_ptr_with_caller(buffer, __FUNCTION__)
#define mpp_buffer_get_fd(buffer) mpp_buffer_get_fd_with_caller(buffer, __FUNCTION__)

// Decoder configuration functions
MPP_RET mpp_dec_cfg_init(MppDecCfg *cfg);
MPP_RET mpp_dec_cfg_deinit(MppDecCfg cfg);
MPP_RET mpp_dec_cfg_set_u32(MppDecCfg cfg, const char *name, uint32_t val);

#endif // HAVE_ROCKCHIP_MPP

#ifdef __cplusplus
}
#endif

#endif // MPP_WRAPPER_H
