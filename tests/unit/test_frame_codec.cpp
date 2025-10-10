#include "cpmcprotocol/codec/device_code_map.hpp"
#include "cpmcprotocol/codec/frame_decoder.hpp"
#include "cpmcprotocol/codec/frame_encoder.hpp"
#include "cpmcprotocol/device.hpp"
#include "cpmcprotocol/session_config.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    using namespace cpmcprotocol;
    codec::FrameEncoder encoder;
    codec::FrameDecoder decoder;

    SessionConfig config{};
    config.network = 0x01;
    config.pc = 0x02;
    config.module_io = 0x1000;
    config.module_station = 0x03;
    config.timeout_250ms = 4;
    config.series = PlcSeries::IQ_R;

    DeviceRange range{DeviceAddress{"D123", DeviceType::Word}, 10};

    auto frame = encoder.makeBatchReadRequest(config, range);

    assert(frame.size() == 23);
    assert(frame[0] == 0x50 && frame[1] == 0x00);
    assert(frame[2] == config.network);
    assert(frame[3] == config.pc);

    const std::uint16_t data_length = static_cast<std::uint16_t>(frame[7] | (frame[8] << 8));
    assert(data_length == 14);

    // Build a mock response and validate decoder.
    std::vector<std::uint8_t> response;
    response.push_back(0xD0);
    response.push_back(0x00);
    response.push_back(config.network);
    response.push_back(config.pc);
    response.push_back(frame[4]);
    response.push_back(frame[5]);
    response.push_back(config.module_station);

    const std::vector<std::uint8_t> device_data = {0x11, 0x22, 0x33, 0x44};
    const std::uint16_t response_length = static_cast<std::uint16_t>(2 + device_data.size());
    response.push_back(static_cast<std::uint8_t>(response_length & 0xFF));
    response.push_back(static_cast<std::uint8_t>((response_length >> 8) & 0xFF));
    response.push_back(0x00); // Completion code LSB
    response.push_back(0x00); // Completion code MSB
    response.insert(response.end(), device_data.begin(), device_data.end());

    auto parsed = decoder.parseBatchReadResponse(response);
    assert(parsed.completion_code == 0x0000);
    assert(parsed.device_data == device_data);

    // Additional device code checks (ZR, RD)
    codec::DeviceCodeMap device_map;
    auto zrInfo = device_map.resolveBinary(config.series, "ZR10");
    assert(zrInfo.code == 0xB0);
    assert(zrInfo.number_base == 16);
    assert(zrInfo.number_width == 4);

    auto rdInfo = device_map.resolveBinary(config.series, "RD100");
    assert(rdInfo.code == 0x2C);
    assert(rdInfo.number_base == 10);
    assert(rdInfo.number_width == 4);

    SessionConfig asciiConfig = config;
    asciiConfig.mode = CommunicationMode::Ascii;

    auto asciiFrame = encoder.makeBatchReadRequest(asciiConfig, range);
    std::string asciiFrameStr(asciiFrame.begin(), asciiFrame.end());
    assert(asciiFrameStr.substr(0, 4) == "5000");
    assert(asciiFrameStr.substr(4, 2) == "01");
    assert(asciiFrameStr.substr(6, 2) == "02");
    assert(asciiFrameStr.substr(14, 4) == "001C");
    assert(asciiFrameStr.substr(22, 4) == "0401");
    assert(asciiFrameStr.substr(26, 4) == "0002");
    assert(asciiFrameStr.substr(30, 4) == "D***");
    assert(asciiFrameStr.substr(34, 8) == "00000123");
    assert(asciiFrameStr.substr(42, 4) == "000A");

    std::string asciiResponse = "D000";
    asciiResponse += "01";
    asciiResponse += "02";
    asciiResponse += "1000";
    asciiResponse += "03";
    asciiResponse += "000C";
    asciiResponse += "0000";
    asciiResponse += "1234ABCD";
    std::vector<std::uint8_t> asciiResponseBuf(asciiResponse.begin(), asciiResponse.end());
    auto asciiParsed = decoder.parseBatchReadResponse(asciiResponseBuf);
    assert(asciiParsed.completion_code == 0x0000);
    std::vector<std::uint8_t> expectedAsciiData = {'1','2','3','4','A','B','C','D'};
    assert(asciiParsed.device_data == expectedAsciiData);

    DeviceRange write_range{DeviceAddress{"D200", DeviceType::Word}, 2};
    std::vector<std::uint16_t> write_values{0x1234, 0x5678};
    auto write_frame = encoder.makeBatchWriteRequest(config, write_range, write_values);
    assert(write_frame.size() > frame.size());

    RandomDeviceRequest random_request{};
    random_request.word_devices = {DeviceAddress{"D300", DeviceType::Word}, DeviceAddress{"D500", DeviceType::Word}};
    random_request.dword_devices = {DeviceAddress{"D700", DeviceType::DoubleWord}};
    auto random_read_frame = encoder.makeRandomReadRequest(config, random_request);
    assert(random_read_frame.size() > frame.size());

    std::vector<std::uint16_t> random_word_values{0x1111, 0x2222};
    std::vector<std::uint32_t> random_dword_values{0x33334444};
    std::vector<bool> random_bit_values;
    auto random_write_frame = encoder.makeRandomWriteRequest(config, random_request, random_word_values, random_dword_values, random_bit_values);
    assert(random_write_frame.size() > random_read_frame.size());

    // Verify diagnostic data handling for binary frame
    std::vector<std::uint8_t> error_response{
        0xD0, 0x00, // subheader
        config.network,
        config.pc,
        0x00, 0x10,
        config.module_station,
        0x04, 0x00, // data length
        0x34, 0x12, // completion code (non-zero)
        0xDE, 0xAD  // diagnostic payload
    };
    auto errorParsed = decoder.parseBatchReadResponse(error_response);
    assert(errorParsed.completion_code == 0x1234);
    assert(errorParsed.device_data.empty());
    assert(errorParsed.diagnostic_data.size() == 2);
    assert(errorParsed.diagnostic_data[0] == 0xDE);

    // ASCII diagnostic frame
    std::string asciiError = "D000"; // subheader
    asciiError += "01"; // network
    asciiError += "02"; // pc
    asciiError += "1000"; // module IO
    asciiError += "03"; // station
    asciiError += "0008"; // data length (completion + payload)
    asciiError += "1234"; // completion code
    asciiError += "BEEF"; // diagnostic payload
    std::vector<std::uint8_t> asciiErrorBuf(asciiError.begin(), asciiError.end());
    auto asciiParsedError = decoder.parseBatchReadResponse(asciiErrorBuf);
    assert(asciiParsedError.completion_code == 0x1234);
    assert(asciiParsedError.device_data.empty());
    std::string diagAscii(asciiParsedError.diagnostic_data.begin(), asciiParsedError.diagnostic_data.end());
    assert(diagAscii == "BEEF");

    bool threw = false;
    try {
        SessionConfig qConfig = config;
        qConfig.series = PlcSeries::Q;
        device_map.resolveBinary(qConfig.series, "RD0");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw && "RD should not be allowed for Q series");

    return 0;
}
