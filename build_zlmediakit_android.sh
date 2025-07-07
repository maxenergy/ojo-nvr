#!/bin/bash

# Build script for ZLMediaKit with Android NDK cross-compilation
# Target: RK3588 platform with arm64-v8a and armeabi-v7a support
# Features: RTSP client support, minimal build for mobile

set -e

# Configuration
ANDROID_NDK_ROOT="/home/rogers/Android/Sdk/ndk/27.0.12077973"
ANDROID_API_LEVEL="21"
PROJECT_ROOT="/home/rogers/source/rockchip/aibox_android/ojo"
ZLMEDIAKIT_SOURCE_DIR="/tmp/zlmediakit_source"
INSTALL_DIR="${PROJECT_ROOT}/app/src/main/cpp/3rdparty/zlmediakit"

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
    log_info "Checking prerequisites for ZLMediaKit..."
    
    if [ ! -d "$ANDROID_NDK_ROOT" ]; then
        log_error "Android NDK not found at: $ANDROID_NDK_ROOT"
        exit 1
    fi
    
    if [ ! -d "$ZLMEDIAKIT_SOURCE_DIR" ]; then
        log_error "ZLMediaKit source not found at: $ZLMEDIAKIT_SOURCE_DIR"
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
build_zlmediakit_arch() {
    local ARCH=$1
    local ABI=$2
    local TOOLCHAIN_PREFIX=$3
    
    log_info "Building ZLMediaKit for architecture: $ARCH ($ABI)"
    
    # Set up build directory
    BUILD_DIR="/tmp/zlmediakit_build_${ABI}"
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
    
    # CMake configuration for ZLMediaKit
    CMAKE_ARGS=(
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake"
        -DANDROID_ABI="$ABI"
        -DANDROID_PLATFORM="android-${ANDROID_API_LEVEL}"
        -DANDROID_NDK="$ANDROID_NDK_ROOT"
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"
        -DCMAKE_CXX_STANDARD=17
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
        -DCMAKE_POLICY_DEFAULT_CMP0057=NEW
        
        # ZLMediaKit specific options - minimal build for RTSP client
        -DENABLE_API=ON
        -DENABLE_API_STATIC_LIB=ON
        -DENABLE_CXX_API=OFF
        -DENABLE_PLAYER=ON
        -DENABLE_RTSP=ON
        -DENABLE_RTMP=OFF
        -DENABLE_HLS=OFF
        -DENABLE_MP4=OFF
        -DENABLE_WEBRTC=OFF
        -DENABLE_SRT=OFF
        -DENABLE_FFMPEG=OFF
        -DENABLE_FAAC=OFF
        -DENABLE_X264=OFF
        -DENABLE_MYSQL=OFF
        -DENABLE_OPENSSL=ON
        -DENABLE_JEMALLOC_STATIC=OFF
        -DENABLE_MEM_DEBUG=OFF
        -DENABLE_ASAN=OFF
        
        # Disable server components
        -DENABLE_SERVER=OFF
        -DENABLE_TESTS=OFF
        
        # Mobile optimizations
        -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -ffunction-sections -fdata-sections"
        -DCMAKE_C_FLAGS_RELEASE="-O3 -DNDEBUG -ffunction-sections -fdata-sections"
        -DCMAKE_EXE_LINKER_FLAGS="-Wl,--gc-sections"
        -DCMAKE_SHARED_LINKER_FLAGS="-Wl,--gc-sections"
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
    
    log_info "Configuring ZLMediaKit build..."
    cmake "${CMAKE_ARGS[@]}" "$ZLMEDIAKIT_SOURCE_DIR"
    
    log_info "Building ZLMediaKit..."
    make -j$(nproc)
    
    log_info "Installing ZLMediaKit libraries for $ABI..."
    # Create architecture-specific lib directory
    mkdir -p "$INSTALL_DIR/lib/$ABI"
    
    # Copy libraries (focus on client libraries)
    find . -name "libzlmediakit.so" -exec cp {} "$INSTALL_DIR/lib/$ABI/" \;
    find . -name "libzlmediakit.a" -exec cp {} "$INSTALL_DIR/lib/$ABI/" \;
    find . -name "libzltoolkit.so" -exec cp {} "$INSTALL_DIR/lib/$ABI/" \;
    find . -name "libzltoolkit.a" -exec cp {} "$INSTALL_DIR/lib/$ABI/" \;
    find . -name "libmk_api.so" -exec cp {} "$INSTALL_DIR/lib/$ABI/" \;
    find . -name "libmk_api.a" -exec cp {} "$INSTALL_DIR/lib/$ABI/" \;
    
    log_info "ZLMediaKit build completed for $ABI"
}

# Copy headers
copy_headers() {
    log_info "Copying ZLMediaKit headers..."
    
    # Copy API headers
    cp -r "$ZLMEDIAKIT_SOURCE_DIR/api/include/"* "$INSTALL_DIR/include/" 2>/dev/null || true
    
    # Copy source headers needed for client
    find "$ZLMEDIAKIT_SOURCE_DIR/src" -name "*.h" -path "*/Client/*" -exec cp {} "$INSTALL_DIR/include/" \; 2>/dev/null || true
    find "$ZLMEDIAKIT_SOURCE_DIR/src" -name "*.h" -path "*/Common/*" -exec cp {} "$INSTALL_DIR/include/" \; 2>/dev/null || true
    find "$ZLMEDIAKIT_SOURCE_DIR/src" -name "*.h" -path "*/Network/*" -exec cp {} "$INSTALL_DIR/include/" \; 2>/dev/null || true
    find "$ZLMEDIAKIT_SOURCE_DIR/src" -name "*.h" -path "*/Util/*" -exec cp {} "$INSTALL_DIR/include/" \; 2>/dev/null || true
    find "$ZLMEDIAKIT_SOURCE_DIR/src" -name "*.h" -path "*/Rtsp/*" -exec cp {} "$INSTALL_DIR/include/" \; 2>/dev/null || true
    
    # Copy ZLToolKit headers
    find "$ZLMEDIAKIT_SOURCE_DIR/3rdpart/ZLToolKit/src" -name "*.h" -exec cp {} "$INSTALL_DIR/include/" \; 2>/dev/null || true
    
    log_info "Headers copied successfully"
}

# Create pkg-config files
create_pkgconfig_files() {
    log_info "Creating pkg-config files..."
    
    mkdir -p "$INSTALL_DIR/lib/pkgconfig"
    
    cat > "$INSTALL_DIR/lib/pkgconfig/zlmediakit.pc" << EOF
prefix=$INSTALL_DIR
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: zlmediakit
Description: ZLMediaKit RTSP Client Library
Version: 1.0.0
Libs: -L\${libdir} -lzlmediakit -lzltoolkit -lmk_api
Cflags: -I\${includedir}
EOF
}

# Show build summary
show_build_summary() {
    log_info "ZLMediaKit Build Summary:"
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

# Main build process
main() {
    log_info "Starting ZLMediaKit build process for Android..."
    
    check_prerequisites
    
    # Build for arm64-v8a (primary target for RK3588)
    build_zlmediakit_arch "aarch64" "arm64-v8a" "aarch64-linux-android"
    
    # Build for armeabi-v7a (fallback)
    build_zlmediakit_arch "arm" "armeabi-v7a" "armv7a-linux-androideabi"
    
    # Copy headers
    copy_headers
    
    # Create pkg-config files
    create_pkgconfig_files
    
    log_info "ZLMediaKit build process completed successfully!"
    log_info "Libraries installed to: $INSTALL_DIR"
    
    # Show summary
    show_build_summary
}

# Run main function
main "$@"
