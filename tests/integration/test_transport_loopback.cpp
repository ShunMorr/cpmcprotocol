#include "cpmcprotocol/codec/frame_decoder.hpp"
#include "cpmcprotocol/codec/frame_encoder.hpp"
#include "cpmcprotocol/device.hpp"
#include "cpmcprotocol/session_config.hpp"
#include "cpmcprotocol/transport.hpp"
#include "util/mock_slmp_server.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

int main() {
    using namespace cpmcprotocol;
    using cpmcprotocol::testutil::MockSlmpServer;

    SessionConfig config{};
    config.host = "127.0.0.1";
    config.port = 56000;
    config.network = 0x11;
    config.pc = 0x22;
    config.module_io = 0x1200;
    config.module_station = 0x33;
    config.timeout_250ms = 8;
    config.series = PlcSeries::IQ_R;

    codec::FrameEncoder encoder;
    codec::FrameDecoder decoder;
    DeviceRange range{DeviceAddress{"D100", DeviceType::Word}, 4};
    const auto request = encoder.makeBatchReadRequest(config, range);

    MockSlmpServer server;
    server.start(static_cast<std::uint16_t>(config.port), [=](const std::vector<std::uint8_t>& req) {
        if (!req.empty() && req[0] == 0xAA) {
            return std::vector<std::uint8_t>{
                0xD0, 0x00,
                config.network,
                config.pc,
                static_cast<std::uint8_t>(config.module_io & 0xFF),
                static_cast<std::uint8_t>((config.module_io >> 8) & 0xFF),
                config.module_station,
                0x04, 0x00,
                0x34, 0x12,
                0xDE, 0xAD
            };
        }

        std::vector<std::uint8_t> resp;
        resp.push_back(0xD0);
        resp.push_back(0x00);
        resp.push_back(req[2]);
        resp.push_back(req[3]);
        resp.push_back(req[4]);
        resp.push_back(req[5]);
        resp.push_back(req[6]);

        const std::vector<std::uint8_t> device_data = {0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE};
        const std::uint16_t data_length = static_cast<std::uint16_t>(2 + device_data.size());
        resp.push_back(static_cast<std::uint8_t>(data_length & 0xFF));
        resp.push_back(static_cast<std::uint8_t>((data_length >> 8) & 0xFF));
        resp.push_back(0x00);
        resp.push_back(0x00);
        resp.insert(resp.end(), device_data.begin(), device_data.end());
        return resp;
    });

    std::this_thread::sleep_for(50ms);

    TcpTransport transport;
    transport.connect(config);
    transport.sendAll(request);
    auto response = transport.receiveAll(9 + 2 + 8); // header + completion + data

    auto parsed = decoder.parseBatchReadResponse(response);
    assert(parsed.completion_code == 0x0000);
    assert(parsed.device_data.size() == 8);
    assert(parsed.device_data[0] == 0x10);
    assert(parsed.device_data[7] == 0xFE);

    // Test variable-length frame reception and diagnostic parsing
    const std::vector<std::uint8_t> diag_trigger = {0xAA};
    transport.sendAll(diag_trigger);
    auto diag_frame = transport.receiveFrame(9, [](const std::uint8_t* header, std::size_t) {
        return static_cast<std::size_t>(header[7] | (header[8] << 8));
    });
    auto diag_parsed = decoder.parseBatchReadResponse(diag_frame);
    assert(diag_parsed.completion_code == 0x1234);
    assert(diag_parsed.diagnostic_data.size() == 2);
    assert(diag_parsed.device_data.empty());

    transport.disconnect();
    server.stop();

    // Timeout handling test
    MockSlmpServer timeout_server;
    timeout_server.start(56001, [](const std::vector<std::uint8_t>&) {
        std::this_thread::sleep_for(200ms);
        return std::vector<std::uint8_t>{};
    });

    std::this_thread::sleep_for(50ms);

    SessionConfig timeout_config = config;
    timeout_config.port = 56001;
    timeout_config.timeout_250ms = 2; // 500ms timeout

    TcpTransport timeout_transport;
    timeout_transport.connect(timeout_config);

    bool timeout_thrown = false;
    try {
        timeout_transport.receiveAll(4);
    } catch (const TransportTimeoutError&) {
        timeout_thrown = true;
    }
    assert(timeout_thrown);

    timeout_transport.disconnect();
    timeout_server.stop();

    return 0;
}
