cmake_minimum_required(VERSION 3.18)
project(cpmcprotocol LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(cpmcprotocol
    src/mc_client.cpp
    src/transport.cpp
    src/runtime_control.cpp
    src/device_catalog.cpp
    src/codec/frame_encoder.cpp
    src/codec/frame_decoder.cpp
    src/codec/device_code_map.cpp
    src/value_codec.cpp
)

target_include_directories(cpmcprotocol
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

add_subdirectory(samples/monitor_client)
add_subdirectory(tests)
