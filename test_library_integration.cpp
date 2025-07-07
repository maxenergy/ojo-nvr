#include <iostream>
#include <android/log.h>

// Test MPP integration
#ifdef HAVE_ROCKCHIP_MPP
#include "rk_mpi.h"
#include "mpp_api.h"
#endif

// Test ZLMediaKit integration  
#ifdef HAVE_ZLMEDIAKIT
#include "mk_common.h"
#include "mk_player.h"
#endif

#define LOG_TAG "LibraryTest"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

int main() {
    LOGI("Starting library integration test...");
    
    bool mpp_available = false;
    bool zlmediakit_available = false;
    
    // Test MPP
#ifdef HAVE_ROCKCHIP_MPP
    LOGI("Testing Rockchip MPP...");
    MppCtx ctx = nullptr;
    MppApi *mpi = nullptr;
    
    MPP_RET ret = mpp_create(&ctx, &mpi);
    if (ret == MPP_OK) {
        LOGI("MPP context created successfully");
        mpp_available = true;
        mpp_destroy(ctx);
    } else {
        LOGE("Failed to create MPP context: %d", ret);
    }
#else
    LOGI("MPP not available (not compiled with HAVE_ROCKCHIP_MPP)");
#endif

    // Test ZLMediaKit
#ifdef HAVE_ZLMEDIAKIT
    LOGI("Testing ZLMediaKit...");
    
    // Initialize ZLMediaKit
    mk_config config = {0};
    config.ini = nullptr;
    config.ini_is_path = 0;
    config.log_level = 0;
    config.log_mask = LOG_CONSOLE;
    config.ssl_file = nullptr;
    config.ssl_pwd = nullptr;
    
    mk_env_init(&config);
    LOGI("ZLMediaKit initialized successfully");
    zlmediakit_available = true;
    
    // Test player creation
    mk_player player = mk_player_create();
    if (player) {
        LOGI("ZLMediaKit player created successfully");
        mk_player_release(player);
    } else {
        LOGE("Failed to create ZLMediaKit player");
        zlmediakit_available = false;
    }
    
    mk_env_uninit();
#else
    LOGI("ZLMediaKit not available (not compiled with HAVE_ZLMEDIAKIT)");
#endif

    // Summary
    LOGI("=== Library Integration Test Results ===");
    LOGI("MPP Available: %s", mpp_available ? "YES" : "NO");
    LOGI("ZLMediaKit Available: %s", zlmediakit_available ? "YES" : "NO");
    
    if (mpp_available && zlmediakit_available) {
        LOGI("SUCCESS: All libraries integrated successfully!");
        return 0;
    } else {
        LOGE("FAILURE: Some libraries failed to integrate");
        return 1;
    }
}
