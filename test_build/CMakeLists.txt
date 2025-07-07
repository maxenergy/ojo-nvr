cmake_minimum_required(VERSION 3.22.1)

project("library_test")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Android NDK libraries
find_library(log-lib log)
find_library(android-lib android)

# Third-party library paths
set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../app/src/main/cpp/3rdparty)
set(ZLMEDIAKIT_DIR ${THIRD_PARTY_DIR}/zlmediakit)
set(MPP_DIR ${THIRD_PARTY_DIR}/mpp)

# ZLMediaKit configuration
if(EXISTS ${ZLMEDIAKIT_DIR})
    include_directories(${ZLMEDIAKIT_DIR}/include)
    link_directories(${ZLMEDIAKIT_DIR}/lib/${ANDROID_ABI})
    set(ZLMEDIAKIT_LIBS
        mk_api
        zlmediakit
        zltoolkit
        ext-codec
        mpeg
        jsoncpp
    )
    add_definitions(-DHAVE_ZLMEDIAKIT)
    message(STATUS "ZLMediaKit found at: ${ZLMEDIAKIT_DIR}")
else()
    message(WARNING "ZLMediaKit not found at: ${ZLMEDIAKIT_DIR}")
    set(ZLMEDIAKIT_LIBS "")
endif()

# Rockchip MPP configuration
if(EXISTS ${MPP_DIR})
    include_directories(${MPP_DIR}/include)
    link_directories(${MPP_DIR}/lib/${ANDROID_ABI})
    set(MPP_LIBS
        rockchip_mpp
    )
    add_definitions(-DHAVE_ROCKCHIP_MPP)
    message(STATUS "Rockchip MPP found at: ${MPP_DIR}")
else()
    message(WARNING "Rockchip MPP not found at: ${MPP_DIR}")
    set(MPP_LIBS "")
endif()

# Create the test executable
add_executable(library_test test_library_integration.cpp)

# Link libraries
target_link_libraries(library_test
    # Android NDK libraries
    ${log-lib}
    ${android-lib}

    # Third-party libraries
    ${ZLMEDIAKIT_LIBS}
    ${MPP_LIBS}

    # System libraries
    dl
    m
)

# Print configuration summary
message(STATUS "=== Library Test Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Android ABI: ${ANDROID_ABI}")
message(STATUS "ZLMediaKit libs: ${ZLMEDIAKIT_LIBS}")
message(STATUS "MPP libs: ${MPP_LIBS}")
message(STATUS "==================================")
