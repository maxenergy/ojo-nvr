#!/bin/bash

# Build script for RockChip MPP with Android NDK cross-compilation
# Target: RK3588 platform with arm64-v8a and armeabi-v7a support

set -e

# Configuration
ANDROID_NDK_ROOT="/home/rogers/Android/Sdk/ndk/27.0.12077973"
ANDROID_API_LEVEL="21"
PROJECT_ROOT="/home/rogers/source/rockchip/aibox_android/ojo"
MPP_SOURCE_DIR="/tmp/mpp_source"
ZLMEDIAKIT_SOURCE_DIR="/tmp/zlmediakit_source"
MPP_INSTALL_DIR="${PROJECT_ROOT}/app/src/main/cpp/3rdparty/mpp"
ZLMEDIAKIT_INSTALL_DIR="${PROJECT_ROOT}/app/src/main/cpp/3rdparty/zlmediakit"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Verify prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    if [ ! -d "$ANDROID_NDK_ROOT" ]; then
        log_error "Android NDK not found at: $ANDROID_NDK_ROOT"
        exit 1
    fi
    
    if [ ! -d "$MPP_SOURCE_DIR" ]; then
        log_error "MPP source not found at: $MPP_SOURCE_DIR"
        exit 1
    fi
    
    if ! command -v cmake &> /dev/null; then
        log_error "CMake not found. Please install CMake 3.22.1 or later"
        exit 1
    fi
    
    CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
    log_info "Using CMake version: $CMAKE_VERSION"
    log_info "Using Android NDK: $ANDROID_NDK_ROOT"
}

# Build function for specific architecture
build_mpp_arch() {
    local ARCH=$1
    local ABI=$2
    local TOOLCHAIN_PREFIX=$3
    
    log_info "Building MPP for architecture: $ARCH ($ABI)"
    
    # Set up build directory
    BUILD_DIR="/tmp/mpp_build_${ABI}"
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Set up Android toolchain variables
    export ANDROID_NDK="$ANDROID_NDK_ROOT"
    export ANDROID_ABI="$ABI"
    export ANDROID_PLATFORM="android-${ANDROID_API_LEVEL}"
    export ANDROID_TOOLCHAIN="clang"
    
    # Toolchain paths
    TOOLCHAIN_DIR="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64"
    export CC="${TOOLCHAIN_DIR}/bin/${TOOLCHAIN_PREFIX}${ANDROID_API_LEVEL}-clang"
    export CXX="${TOOLCHAIN_DIR}/bin/${TOOLCHAIN_PREFIX}${ANDROID_API_LEVEL}-clang++"
    export AR="${TOOLCHAIN_DIR}/bin/llvm-ar"
    export STRIP="${TOOLCHAIN_DIR}/bin/llvm-strip"
    export RANLIB="${TOOLCHAIN_DIR}/bin/llvm-ranlib"
    
    # CMake configuration
    CMAKE_ARGS=(
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake"
        -DANDROID_ABI="$ABI"
        -DANDROID_PLATFORM="android-${ANDROID_API_LEVEL}"
        -DANDROID_NDK="$ANDROID_NDK_ROOT"
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"
        -DCMAKE_CXX_STANDARD=17
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
        -DHAVE_DRM=ON
        -DRKPLATFORM=ON
        -DHAVE_AVSD=OFF
        -DHAVE_H263D=OFF
        -DHAVE_H264D=ON
        -DHAVE_H265D=ON
        -DHAVE_MPEG2D=OFF
        -DHAVE_MPEG4D=OFF
        -DHAVE_VP8D=ON
        -DHAVE_VP9D=ON
        -DHAVE_JPEGD=ON
        -DHAVE_H264E=ON
        -DHAVE_H265E=ON
        -DHAVE_JPEGE=ON
        -DHAVE_VP8E=OFF
        -DHAVE_VP9E=OFF
    )
    
    # Add architecture-specific flags
    if [ "$ABI" = "arm64-v8a" ]; then
        CMAKE_ARGS+=(
            -DCMAKE_CXX_FLAGS="-march=armv8-a -mtune=cortex-a76 -O3 -DNDEBUG -DRK3588_PLATFORM"
            -DCMAKE_C_FLAGS="-march=armv8-a -mtune=cortex-a76 -O3 -DNDEBUG -DRK3588_PLATFORM"
        )
    else
        CMAKE_ARGS+=(
            -DCMAKE_CXX_FLAGS="-march=armv7-a -mfpu=neon -O3 -DNDEBUG"
            -DCMAKE_C_FLAGS="-march=armv7-a -mfpu=neon -O3 -DNDEBUG"
        )
    fi
    
    log_info "Configuring MPP build..."
    cmake "${CMAKE_ARGS[@]}" "$MPP_SOURCE_DIR"
    
    log_info "Building MPP..."
    make -j$(nproc)
    
    log_info "Installing MPP libraries for $ABI..."
    # Create architecture-specific lib directory
    mkdir -p "$INSTALL_DIR/lib/$ABI"
    
    # Copy libraries
    find . -name "*.so" -exec cp {} "$INSTALL_DIR/lib/$ABI/" \;
    find . -name "*.a" -exec cp {} "$INSTALL_DIR/lib/$ABI/" \;
    
    log_info "MPP build completed for $ABI"
}

# Copy headers
copy_headers() {
    log_info "Copying MPP headers..."
    
    # Copy main headers
    cp -r "$MPP_SOURCE_DIR/inc/"* "$INSTALL_DIR/include/"
    
    # Copy additional headers from mpp directory
    find "$MPP_SOURCE_DIR/mpp" -name "*.h" -exec cp {} "$INSTALL_DIR/include/" \;
    
    # Copy osal headers
    find "$MPP_SOURCE_DIR/osal" -name "*.h" -exec cp {} "$INSTALL_DIR/include/" \;
    
    log_info "Headers copied successfully"
}

# Main build process
main() {
    log_info "Starting MPP build process for Android..."
    
    check_prerequisites
    
    # Build for arm64-v8a (primary target for RK3588)
    build_mpp_arch "aarch64" "arm64-v8a" "aarch64-linux-android"
    
    # Build for armeabi-v7a (fallback)
    build_mpp_arch "arm" "armeabi-v7a" "armv7a-linux-androideabi"
    
    # Copy headers
    copy_headers
    
    # Create pkg-config files
    create_pkgconfig_files
    
    log_info "MPP build process completed successfully!"
    log_info "Libraries installed to: $INSTALL_DIR"
    
    # Show summary
    show_build_summary
}

# Create pkg-config files
create_pkgconfig_files() {
    log_info "Creating pkg-config files..."
    
    mkdir -p "$INSTALL_DIR/lib/pkgconfig"
    
    cat > "$INSTALL_DIR/lib/pkgconfig/rockchip_mpp.pc" << EOF
prefix=$INSTALL_DIR
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: rockchip_mpp
Description: Rockchip Media Process Platform
Version: 1.0.0
Libs: -L\${libdir} -lrockchip_mpp
Cflags: -I\${includedir}
EOF
}

# Show build summary
show_build_summary() {
    log_info "Build Summary:"
    echo "----------------------------------------"
    echo "Install Directory: $INSTALL_DIR"
    echo "Architectures: arm64-v8a, armeabi-v7a"
    echo ""
    echo "Libraries built:"
    find "$INSTALL_DIR/lib" -name "*.so" -o -name "*.a" | sort
    echo ""
    echo "Headers installed:"
    find "$INSTALL_DIR/include" -name "*.h" | wc -l
    echo " header files copied"
}

# Run main function
main "$@"
