#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cpmcprotocol {

enum class PlcSeries {
    Q,
    L,
    QnA,
    IQ_L,
    IQ_R
};

enum class DeviceType {
    Word,
    Bit,
    DoubleWord
};

struct DeviceAddress {
    std::string name;        // e.g., "D1000"
    DeviceType type = DeviceType::Word;
};

struct DeviceRange {
    DeviceAddress head;
    std::uint16_t length = 0;
};

struct RandomDeviceRequest {
    std::vector<DeviceAddress> word_devices;
    std::vector<DeviceAddress> bit_devices;
    std::vector<DeviceAddress> dword_devices;
    // TODO: extend with dword devices per spec Section 10
};

} // namespace cpmcprotocol
