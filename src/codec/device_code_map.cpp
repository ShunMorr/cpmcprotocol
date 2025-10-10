#include "cpmcprotocol/codec/device_code_map.hpp"

// デバイス名から MC プロトコル用のコード値を導出するテーブル実装。

#include <stdexcept>
#include <string>

namespace cpmcprotocol::codec {

namespace {

enum class SeriesMask : std::uint8_t {
    None = 0,
    Q = 1 << 0,
    L = 1 << 1,
    QnA = 1 << 2,
    IQ_L = 1 << 3,
    IQ_R = 1 << 4
};

constexpr SeriesMask operator|(SeriesMask lhs, SeriesMask rhs) {
    return static_cast<SeriesMask>(static_cast<std::uint8_t>(lhs) |
                                   static_cast<std::uint8_t>(rhs));
}

// シリーズ毎に利用可能なデバイスか判定するヘルパ。
constexpr bool isSeriesSupported(SeriesMask mask, PlcSeries series) {
    const auto bit = static_cast<std::uint8_t>(mask);
    switch (series) {
        case PlcSeries::Q:
            return (bit & static_cast<std::uint8_t>(SeriesMask::Q)) != 0;
        case PlcSeries::L:
            return (bit & static_cast<std::uint8_t>(SeriesMask::L)) != 0;
        case PlcSeries::QnA:
            return (bit & static_cast<std::uint8_t>(SeriesMask::QnA)) != 0;
        case PlcSeries::IQ_L:
            return (bit & static_cast<std::uint8_t>(SeriesMask::IQ_L)) != 0;
        case PlcSeries::IQ_R:
            return (bit & static_cast<std::uint8_t>(SeriesMask::IQ_R)) != 0;
    }
    return false;
}

struct DeviceEntry {
    const char* prefix;
    std::uint16_t binary_code;
    int base;
    SeriesMask supported;
};

const DeviceEntry* lookupDevice(const std::string& name) {
    static const DeviceEntry table[] = {
        {"ZR", 0xB0, 16, SeriesMask::Q | SeriesMask::L | SeriesMask::QnA | SeriesMask::IQ_L | SeriesMask::IQ_R},
        {"RD", 0x2C, 10, SeriesMask::IQ_R},
        {"X", 0x9C, 16, SeriesMask::Q | SeriesMask::L | SeriesMask::QnA | SeriesMask::IQ_L | SeriesMask::IQ_R},
        {"Y", 0x9D, 16, SeriesMask::Q | SeriesMask::L | SeriesMask::QnA | SeriesMask::IQ_L | SeriesMask::IQ_R},
        {"M", 0x90, 10, SeriesMask::Q | SeriesMask::L | SeriesMask::QnA | SeriesMask::IQ_L | SeriesMask::IQ_R},
        {"D", 0xA8, 10, SeriesMask::Q | SeriesMask::L | SeriesMask::QnA | SeriesMask::IQ_L | SeriesMask::IQ_R},
        {"W", 0xB4, 16, SeriesMask::Q | SeriesMask::L | SeriesMask::QnA | SeriesMask::IQ_L | SeriesMask::IQ_R},
        {"L", 0x92, 10, SeriesMask::Q | SeriesMask::L | SeriesMask::QnA | SeriesMask::IQ_L | SeriesMask::IQ_R},
        {"F", 0x93, 10, SeriesMask::Q | SeriesMask::L | SeriesMask::QnA | SeriesMask::IQ_L | SeriesMask::IQ_R},
        {"R", 0xAF, 10, SeriesMask::Q | SeriesMask::L | SeriesMask::QnA | SeriesMask::IQ_L | SeriesMask::IQ_R},
        {"Z", 0xCC, 10, SeriesMask::Q | SeriesMask::L | SeriesMask::QnA | SeriesMask::IQ_L | SeriesMask::IQ_R},
        {"B", 0xA0, 16, SeriesMask::Q | SeriesMask::L | SeriesMask::QnA | SeriesMask::IQ_L | SeriesMask::IQ_R},
        {"T", 0xC2, 10, SeriesMask::Q | SeriesMask::L | SeriesMask::QnA | SeriesMask::IQ_L | SeriesMask::IQ_R},
        {"C", 0xC5, 10, SeriesMask::Q | SeriesMask::L | SeriesMask::QnA | SeriesMask::IQ_L | SeriesMask::IQ_R},
    };

    for (const auto& entry : table) {
        if (name.rfind(entry.prefix, 0) == 0) {
            return &entry;
        }
    }

    return nullptr;
}

} // namespace

BinaryDeviceCodeInfo DeviceCodeMap::resolveBinary(PlcSeries series, const std::string& device_name) const {
    const auto* entry = lookupDevice(device_name);
    if (!entry) {
        throw std::invalid_argument("Unsupported device name: " + device_name);
    }

    if (!isSeriesSupported(entry->supported, series)) {
        throw std::invalid_argument("Device " + device_name + " is not supported by selected PLC series");
    }

    BinaryDeviceCodeInfo info{};
    info.code = entry->binary_code;
    info.code_width = (series == PlcSeries::IQ_R) ? 2 : 1;
    info.number_base = entry->base;
    info.number_width = (series == PlcSeries::IQ_R) ? 4 : 3;
    return info;
}

AsciiDeviceCodeInfo DeviceCodeMap::resolveAscii(PlcSeries series, const std::string& device_name) const {
    const auto* entry = lookupDevice(device_name);
    if (!entry) {
        throw std::invalid_argument("Unsupported device name: " + device_name);
    }

    if (!isSeriesSupported(entry->supported, series)) {
        throw std::invalid_argument("Device " + device_name + " is not supported by selected PLC series");
    }

    AsciiDeviceCodeInfo info{};
    const bool is_iqr = (series == PlcSeries::IQ_R);
    const std::size_t code_width = is_iqr ? 4 : 2;

    std::string code(entry->prefix);
    if (code.size() > code_width) {
        throw std::invalid_argument("Device prefix length exceeds ASCII code width: " + device_name);
    }
    if (code.size() < code_width) {
        code.append(code_width - code.size(), '*');
    }
    info.code = std::move(code);
    info.number_base = entry->base;
    info.number_width = is_iqr ? 8 : 6;
    return info;
}

} // namespace cpmcprotocol::codec
