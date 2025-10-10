#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "cpmcprotocol/device.hpp"

namespace cpmcprotocol::codec {

struct BinaryDeviceCodeInfo {
    std::uint16_t code = 0;
    std::size_t code_width = 1; // bytes (1 for Q/L, 2 for iQ-R)
    int number_base = 10;
    std::size_t number_width = 0; // bytes (3 for Q/L, 4 for iQ-R)
};

struct AsciiDeviceCodeInfo {
    std::string code;
    int number_base = 10;
    std::size_t number_width = 0; // characters (6 for Q/L, 8 for iQ-R)
};

class DeviceCodeMap {
public:
    BinaryDeviceCodeInfo resolveBinary(PlcSeries series, const std::string& device_name) const;
    AsciiDeviceCodeInfo resolveAscii(PlcSeries series, const std::string& device_name) const;
};

} // namespace cpmcprotocol::codec
