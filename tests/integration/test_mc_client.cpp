#include "cpmcprotocol/access_option.hpp"
#include "cpmcprotocol/mc_client.hpp"
#include "cpmcprotocol/runtime_control.hpp"
#include "cpmcprotocol/session_config.hpp"
#include "cpmcprotocol/value_codec.hpp"
#include "util/mock_slmp_server.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

using namespace cpmcprotocol;

namespace {

std::vector<std::uint8_t> makeBinaryResponse(const std::vector<std::uint8_t>& request,
                                             const std::vector<std::uint8_t>& payload,
                                             std::uint16_t completion = 0x0000) {
    std::vector<std::uint8_t> response;
    response.reserve(11 + 2 + payload.size());
    response.push_back(0xD0);
    response.push_back(0x00);
    response.push_back(request[2]);
    response.push_back(request[3]);
    response.push_back(request[4]);
    response.push_back(request[5]);
    response.push_back(request[6]);
    const std::uint16_t data_length = static_cast<std::uint16_t>(2 + payload.size());
    response.push_back(static_cast<std::uint8_t>(data_length & 0xFF));
    response.push_back(static_cast<std::uint8_t>((data_length >> 8) & 0xFF));
    response.push_back(static_cast<std::uint8_t>(completion & 0xFF));
    response.push_back(static_cast<std::uint8_t>((completion >> 8) & 0xFF));
    response.insert(response.end(), payload.begin(), payload.end());
    return response;
}

} // namespace

int main() {
    using cpmcprotocol::testutil::MockSlmpServer;

    MockSlmpServer server;
    server.start(56002, [](const std::vector<std::uint8_t>& request) {
        if (request.size() < 15) {
            return std::vector<std::uint8_t>{};
        }
        const std::uint16_t command = static_cast<std::uint16_t>(request[11] | (request[12] << 8));
        const std::uint16_t subcommand = static_cast<std::uint16_t>(request[13] | (request[14] << 8));
        switch (command) {
        case 0x0401: { // sequential read
            if (subcommand == 0x0000 || subcommand == 0x0002) {
                // 2 ワード分のダミーデータ
                return makeBinaryResponse(request, {0x34, 0x12, 0x78, 0x56});
            }
            // ビット読み出し (3 点分)
            return makeBinaryResponse(request, {0x10, 0x10});
        }
        case 0x1401:
            // 書き込み応答は完了コードのみ
            return makeBinaryResponse(request, {});
        case 0x0403: { // ランダム読み出し
            // Check device counts from request
            std::uint8_t word_count = request.size() > 15 ? request[15] : 0;
            std::uint8_t dword_count = request.size() > 16 ? request[16] : 0;
            std::uint8_t lword_count = request.size() > 17 ? request[17] : 0;
            std::uint8_t bit_count = request.size() > 18 ? request[18] : 0;

            std::vector<std::uint8_t> payload;
            // WORD responses (2 bytes each)
            for (std::uint8_t i = 0; i < word_count; ++i) {
                payload.push_back(0x21);
                payload.push_back(0x43);
            }
            // DWORD responses (4 bytes each)
            for (std::uint8_t i = 0; i < dword_count; ++i) {
                payload.push_back(0xAB);
                payload.push_back(0x89);
                payload.push_back(0xEF);
                payload.push_back(0xCD);
            }
            // LWORD responses (8 bytes each)
            for (std::uint8_t i = 0; i < lword_count; ++i) {
                payload.push_back(0x12);
                payload.push_back(0x34);
                payload.push_back(0x56);
                payload.push_back(0x78);
                payload.push_back(0x9A);
                payload.push_back(0xBC);
                payload.push_back(0xDE);
                payload.push_back(0xF0);
            }
            // BIT responses (2 bytes each for iQ-R)
            for (std::uint8_t i = 0; i < bit_count; ++i) {
                payload.push_back(0x01);
                payload.push_back(0x00);
            }
            return makeBinaryResponse(request, payload);
        }
        case 0x1402:
            return makeBinaryResponse(request, {});
        case 0x0101: {
            std::string cpu_type = "QCPU" + std::string(12, ' ');
            std::vector<std::uint8_t> payload(cpu_type.begin(), cpu_type.end());
            payload.push_back(0x34);
            payload.push_back(0x12);
            return makeBinaryResponse(request, payload);
        }
        case 0x1001:
        case 0x1002:
        case 0x1003:
        case 0x1005:
        case 0x1006:
        case 0x1630:
        case 0x1631:
            return makeBinaryResponse(request, {});
        default:
            return std::vector<std::uint8_t>{};
        }
    });

    SessionConfig config{};
    config.host = "127.0.0.1";
    config.port = 56002;
    config.mode = CommunicationMode::Binary;
    config.network = 0x01;
    config.pc = 0x02;
    config.module_io = 0x1200;
    config.module_station = 0x03;
    config.timeout_250ms = 4;
    config.series = PlcSeries::IQ_R;

    McClient client;
    client.connect(config);

    AccessOption option{};
    option.mode = CommunicationMode::Binary;
    option.network = 0x01;
    option.pc = 0x02;
    option.module_io = 0x1200;
    option.module_station = 0x03;
    option.timeout_seconds = 1;
    client.setAccessOption(option);

    DeviceRange word_range{DeviceAddress{"D100", DeviceType::Word}, 2};
    auto word_values = client.readWords(word_range);
    assert(word_values.size() == 2);
    assert(word_values[0] == 0x1234);
    assert(word_values[1] == 0x5678);

    DeviceRange bit_range{DeviceAddress{"X10", DeviceType::Bit}, 3};
    auto bit_values = client.readBits(bit_range);
    assert(bit_values.size() == 3);
    assert(bit_values[0] == true && bit_values[1] == false && bit_values[2] == true);

    client.writeWords(word_range, {0x1111, 0x2222});
    client.writeBits(bit_range, {true, true, false});

    DeviceReadPlan read_plan{
        {DeviceAddress{"D200", DeviceType::Word}, ValueFormat::Int16()},
        {DeviceAddress{"D300", DeviceType::DoubleWord}, ValueFormat::Int32()}
    };
    auto random_values = client.randomRead(read_plan);
    assert(std::get<int16_t>(random_values[0]) == static_cast<int16_t>(0x4321));
    assert(std::get<int32_t>(random_values[1]) == static_cast<int32_t>(0xCDEF89AB));

    DeviceWritePlan write_plan{
        {DeviceAddress{"D200", DeviceType::Word}, ValueFormat::Int16(), static_cast<int16_t>(0x1111)},
        {DeviceAddress{"D300", DeviceType::DoubleWord}, ValueFormat::Int32(), static_cast<int32_t>(0x12345678)}
    };
    client.randomWrite(write_plan);

    // Test 64-bit random read
    DeviceReadPlan read_plan_64{
        {DeviceAddress{"D400", DeviceType::Word}, ValueFormat::Int64()},
        {DeviceAddress{"D500", DeviceType::Word}, ValueFormat::UInt64()}
    };
    auto random_values_64 = client.randomRead(read_plan_64);
    assert(std::get<int64_t>(random_values_64[0]) == static_cast<int64_t>(0xF0DEBC9A78563412LL));
    assert(std::get<uint64_t>(random_values_64[1]) == static_cast<uint64_t>(0xF0DEBC9A78563412ULL));

    // Test 64-bit random write
    DeviceWritePlan write_plan_64{
        {DeviceAddress{"D400", DeviceType::Word}, ValueFormat::Int64(), static_cast<int64_t>(0x1122334455667788LL)},
        {DeviceAddress{"D500", DeviceType::Word}, ValueFormat::UInt64(), static_cast<uint64_t>(0xAABBCCDDEEFF0011ULL)}
    };
    client.randomWrite(write_plan_64);

    // Test bit random read
    DeviceReadPlan read_plan_bit{
        {DeviceAddress{"X100", DeviceType::Bit}, ValueFormat::BitArray(1)},
        {DeviceAddress{"Y200", DeviceType::Bit}, ValueFormat::BitArray(1)}
    };
    auto random_values_bit = client.randomRead(read_plan_bit);
    auto bits1 = std::get<std::vector<bool>>(random_values_bit[0]);
    auto bits2 = std::get<std::vector<bool>>(random_values_bit[1]);
    assert(bits1.size() == 1 && bits1[0] == true);
    assert(bits2.size() == 1 && bits2[0] == true);

    // Test bit random write
    DeviceWritePlan write_plan_bit{
        {DeviceAddress{"X100", DeviceType::Bit}, ValueFormat::BitArray(1), std::vector<bool>{true}},
        {DeviceAddress{"Y200", DeviceType::Bit}, ValueFormat::BitArray(1), std::vector<bool>{false}}
    };
    client.randomWrite(write_plan_bit);

    auto cpu = client.readCpuType();
    assert(cpu.cpu_type == "QCPU");
    assert(cpu.cpu_code == "1234");

    RuntimeControl run_cmd{};
    run_cmd.type = RuntimeCommandType::Run;
    run_cmd.run_option = RuntimeRunOption{ClearMode::ClearAll, false};
    client.applyRuntimeControl(run_cmd);

    RuntimeControl stop_cmd{};
    stop_cmd.type = RuntimeCommandType::Stop;
    client.applyRuntimeControl(stop_cmd);

    RuntimeControl pause_cmd{};
    pause_cmd.type = RuntimeCommandType::Pause;
    pause_cmd.run_option = RuntimeRunOption{ClearMode::NoClear, true};
    client.applyRuntimeControl(pause_cmd);

    RuntimeControl latch_cmd{};
    latch_cmd.type = RuntimeCommandType::LatchClear;
    client.applyRuntimeControl(latch_cmd);

    RuntimeControl lock_cmd{};
    lock_cmd.type = RuntimeCommandType::Lock;
    RuntimeLockOption lock_option{};
    lock_option.password = std::string("123456");
    lock_cmd.lock_option = lock_option;
    client.applyRuntimeControl(lock_cmd);

    RuntimeControl unlock_cmd{};
    unlock_cmd.type = RuntimeCommandType::Unlock;
    RuntimeLockOption unlock_option{};
    unlock_option.password = std::string("123456");
    unlock_cmd.lock_option = unlock_option;
    client.applyRuntimeControl(unlock_cmd);

    client.disconnect();
    server.stop();

    return 0;
}
