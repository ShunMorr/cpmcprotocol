add_library(cpmcprotocol_test_support
    util/mock_slmp_server.cpp
)

target_include_directories(cpmcprotocol_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

enable_testing()

add_executable(test_frame_codec
    unit/test_frame_codec.cpp
)

target_link_libraries(test_frame_codec PRIVATE cpmcprotocol cpmcprotocol_test_support)

add_test(NAME FrameCodec COMMAND test_frame_codec)

add_executable(test_value_codec
    unit/test_value_codec.cpp
)

target_link_libraries(test_value_codec PRIVATE cpmcprotocol cpmcprotocol_test_support)

add_test(NAME ValueCodec COMMAND test_value_codec)

add_executable(test_transport_loopback
    integration/test_transport_loopback.cpp
)

target_link_libraries(test_transport_loopback PRIVATE cpmcprotocol cpmcprotocol_test_support)

add_test(NAME TransportLoopback COMMAND test_transport_loopback)
