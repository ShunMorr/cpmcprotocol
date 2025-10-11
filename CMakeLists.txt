cmake_minimum_required(VERSION 3.18)
project(cpmcprotocol LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(cpmcprotocol
    src/mc_client.cpp
    src/transport.cpp
    src/runtime_control.cpp
    src/session_config.cpp
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

# Install library
install(TARGETS cpmcprotocol
    EXPORT cpmcprotocolTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Install header files
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

# Install CMake package configuration files
include(CMakePackageConfigHelpers)

# Generate the config file that includes the exports
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cpmcprotocolConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cpmcprotocolConfig.cmake
    INSTALL_DESTINATION lib/cmake/cpmcprotocol
)

# Generate the version file for the config file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cpmcprotocolConfigVersion.cmake
    VERSION 0.1.0
    COMPATIBILITY AnyNewerVersion
)

# Install the configuration files
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cpmcprotocolConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cpmcprotocolConfigVersion.cmake
    DESTINATION lib/cmake/cpmcprotocol
)

# Install the export set for use with the install-tree
install(EXPORT cpmcprotocolTargets
    FILE cpmcprotocolTargets.cmake
    NAMESPACE cpmcprotocol::
    DESTINATION lib/cmake/cpmcprotocol
)

add_subdirectory(samples/monitor_client)
add_subdirectory(tests)
