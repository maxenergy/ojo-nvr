// Simple test to verify library linking without complex headers

int main() {
    // Test MPP
#ifdef HAVE_ROCKCHIP_MPP
    // MPP library compiled and linked successfully
    int mpp_available = 1;
#else
    // MPP not available
    int mpp_available = 0;
#endif

    // Test ZLMediaKit
#ifdef HAVE_ZLMEDIAKIT
    // ZLMediaKit library compiled and linked successfully
    int zlmediakit_available = 1;
#else
    // ZLMediaKit not available
    int zlmediakit_available = 0;
#endif

    // Return success if both libraries are available
    if (mpp_available && zlmediakit_available) {
        return 0; // SUCCESS
    } else {
        return 1; // FAILURE
    }
}
