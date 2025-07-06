# CMakeLists.txt for Ojo Native Player
# MPP + ZLMediaKit implementation for RK3588 platform

cmake_minimum_required(VERSION 3.18.1)

project("ojo_native")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Platform-specific settings for RK3588
if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a")
    add_definitions(-DRK3588_PLATFORM)
endif()

# Find required packages
find_package(PkgConfig REQUIRED)

# Android NDK libraries
find_library(log-lib log)
find_library(android-lib android)

# Define source files
set(OJO_NATIVE_SOURCES
    # Core utilities
    ojo_native/utils/ojo_types.h
    
    # RTSP client
    ojo_native/rtsp/ZLRTSPClient.h
    ojo_native/rtsp/ZLRTSPClient.cpp
    
    # MPP decoder
    ojo_native/decoder/MPPDecoder.h
    ojo_native/decoder/MPPDecoder.cpp
    
    # Surface renderer
    ojo_native/renderer/NativeSurfaceRenderer.h
    ojo_native/renderer/NativeSurfaceRenderer.cpp
    
    # JNI bridge
    ojo_native/jni/ojo_jni_bridge.h
    ojo_native/jni/ojo_jni_bridge.cpp
    
    # Thread pool and utilities
    ojo_native/utils/thread_pool.h
    ojo_native/utils/thread_pool.cpp
    ojo_native/utils/memory_pool.h
    ojo_native/utils/memory_pool.cpp
    ojo_native/utils/performance_monitor.h
    ojo_native/utils/performance_monitor.cpp
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ojo_native
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/include
)

# Third-party library paths
set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)
set(ZLMEDIAKIT_DIR ${THIRD_PARTY_DIR}/zlmediakit)
set(MPP_DIR ${THIRD_PARTY_DIR}/mpp)
set(RGA_DIR ${THIRD_PARTY_DIR}/rga)

# ZLMediaKit configuration
if(EXISTS ${ZLMEDIAKIT_DIR})
    include_directories(${ZLMEDIAKIT_DIR}/include)
    link_directories(${ZLMEDIAKIT_DIR}/lib/${ANDROID_ABI})
    set(ZLMEDIAKIT_LIBS
        zlmediakit
        zltoolkit
        ssl
        crypto
    )
    add_definitions(-DHAVE_ZLMEDIAKIT)
else()
    message(WARNING "ZLMediaKit not found, using placeholder implementation")
    set(ZLMEDIAKIT_LIBS "")
endif()

# Rockchip MPP configuration
if(EXISTS ${MPP_DIR})
    include_directories(${MPP_DIR}/include)
    link_directories(${MPP_DIR}/lib/${ANDROID_ABI})
    set(MPP_LIBS
        rockchip_mpp
        rockchip_vpu
    )
    add_definitions(-DHAVE_ROCKCHIP_MPP)
else()
    message(WARNING "Rockchip MPP not found, using placeholder implementation")
    set(MPP_LIBS "")
endif()

# RGA configuration
if(EXISTS ${RGA_DIR})
    include_directories(${RGA_DIR}/include)
    link_directories(${RGA_DIR}/lib/${ANDROID_ABI})
    set(RGA_LIBS
        rga
    )
    add_definitions(-DHAVE_RGA)
else()
    message(WARNING "RGA not found, using software rendering")
    set(RGA_LIBS "")
endif()

# Create the native library
add_library(ojo_native SHARED ${OJO_NATIVE_SOURCES})

# Link libraries
target_link_libraries(ojo_native
    # Android NDK libraries
    ${log-lib}
    ${android-lib}
    
    # Third-party libraries
    ${ZLMEDIAKIT_LIBS}
    ${MPP_LIBS}
    ${RGA_LIBS}
    
    # System libraries
    pthread
    dl
    m
)

# Compiler definitions
target_compile_definitions(ojo_native PRIVATE
    ANDROID_PLATFORM
    LOG_TAG="OjoNative"
)

# Debug configuration
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(ojo_native PRIVATE
        DEBUG_BUILD
        ENABLE_VERBOSE_LOGGING
    )
endif()

# Installation
install(TARGETS ojo_native
    LIBRARY DESTINATION lib/${ANDROID_ABI}
)

# Custom targets for development

# Format code
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i -style=Google ${OJO_NATIVE_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Formatting source code"
    )
endif()

# Static analysis
find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY} ${OJO_NATIVE_SOURCES} -- 
            -I${CMAKE_CURRENT_SOURCE_DIR}
            -I${CMAKE_CURRENT_SOURCE_DIR}/ojo_native
            -DANDROID_PLATFORM
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running static analysis"
    )
endif()

# Documentation
find_program(DOXYGEN doxygen)
if(DOXYGEN)
    add_custom_target(docs
        COMMAND ${DOXYGEN} Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating documentation"
    )
endif()

# Print configuration summary
message(STATUS "=== Ojo Native Player Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Android ABI: ${ANDROID_ABI}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "ZLMediaKit: ${ZLMEDIAKIT_DIR}")
message(STATUS "Rockchip MPP: ${MPP_DIR}")
message(STATUS "RGA: ${RGA_DIR}")
message(STATUS "=======================================")

# Validation
if(NOT ZLMEDIAKIT_LIBS AND NOT MPP_LIBS)
    message(FATAL_ERROR "Neither ZLMediaKit nor MPP found. Cannot build native player.")
endif()

if(ANDROID_ABI STREQUAL "arm64-v8a")
    message(STATUS "Building for RK3588 platform with hardware acceleration")
else()
    message(WARNING "Building for non-RK3588 platform. Hardware acceleration may not be available.")
endif()
