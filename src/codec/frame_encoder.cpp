#include "cpmcprotocol/codec/frame_encoder.hpp"

// 低レイヤで構築したデバイス情報を 3E フレームへ変換するエンコーダ。

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace cpmcprotocol::codec {

namespace {

void appendLittleEndian(std::vector<std::uint8_t>& buffer, std::uint32_t value, std::size_t width) {
    for (std::size_t i = 0; i < width; ++i) {
        buffer.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF));
    }
}

std::size_t findDigitPosition(const std::string& device_name) {
    return device_name.find_first_of("0123456789");
}

std::uint32_t parseDeviceNumber(const std::string& device_name, int base) {
    const std::size_t pos = findDigitPosition(device_name);
    if (pos == std::string::npos) {
        throw std::invalid_argument("Device name missing numeric part: " + device_name);
    }
    std::string number_part = device_name.substr(pos);
    if (base == 16 && number_part.rfind("0x", 0) == 0) {
        number_part = number_part.substr(2);
    }
    return static_cast<std::uint32_t>(std::stoul(number_part, nullptr, base));
}

// ASCII モードでサブヘッダーや数値を固定桁で表現するためのヘルパ。
std::string toHex(std::uint32_t value, std::size_t width) {
    std::ostringstream oss;
    oss << std::uppercase << std::setw(static_cast<int>(width)) << std::setfill('0') << std::hex << value;
    return oss.str();
}

std::string toDecimalPadded(std::uint32_t value, std::size_t width) {
    std::string s = std::to_string(value);
    if (s.size() > width) {
        throw std::invalid_argument("Device number exceeds ASCII width");
    }
    if (s.size() < width) {
        s.insert(s.begin(), width - s.size(), '0');
    }
    return s;
}

std::vector<std::uint8_t> buildBinaryFrame(const SessionConfig& config, const std::vector<std::uint8_t>& request) {
    std::vector<std::uint8_t> frame;
    frame.reserve(11 + request.size());

    // 3E フレームの共通ヘッダーを組み立てる。
    frame.push_back(0x50);
    frame.push_back(0x00);
    frame.push_back(config.network);
    frame.push_back(config.pc);
    appendLittleEndian(frame, config.module_io, 2);
    frame.push_back(config.module_station);
    appendLittleEndian(frame, static_cast<std::uint16_t>(2 + request.size()), 2);
    appendLittleEndian(frame, config.timeout_250ms, 2);
    frame.insert(frame.end(), request.begin(), request.end());

    return frame;
}

std::vector<std::uint8_t> buildAsciiFrame(const SessionConfig& config, const std::string& request) {
    std::string frame;
    frame.reserve(4 + 2 + 2 + 4 + 2 + 4 + 4 + request.size());
    frame += "5000";
    frame += toHex(config.network, 2);
    frame += toHex(config.pc, 2);
    frame += toHex(config.module_io, 4);
    frame += toHex(config.module_station, 2);
    frame += toHex(static_cast<std::uint32_t>(4 + request.size()), 4);
    frame += toHex(config.timeout_250ms, 4);
    frame += request;

    return std::vector<std::uint8_t>(frame.begin(), frame.end());
}

std::uint16_t sequentialSubcommand(DeviceType type, PlcSeries series) {
    switch (type) {
        case DeviceType::Bit:
            return (series == PlcSeries::IQ_R) ? 0x0003 : 0x0001;
        case DeviceType::Word:
        case DeviceType::DoubleWord:
            return (series == PlcSeries::IQ_R) ? 0x0002 : 0x0000;
    }
    return 0x0000;
}

std::uint16_t randomWordSubcommand(PlcSeries series) {
    return (series == PlcSeries::IQ_R) ? 0x0002 : 0x0000;
}

void appendDeviceBinary(std::vector<std::uint8_t>& buffer, const BinaryDeviceCodeInfo& info, std::uint32_t number) {
    appendLittleEndian(buffer, number, info.number_width);
    appendLittleEndian(buffer, info.code, info.code_width);
}

void appendDeviceAscii(std::string& buffer, const AsciiDeviceCodeInfo& info, std::uint32_t number) {
    buffer += info.code;
    buffer += toDecimalPadded(number, info.number_width);
}

std::vector<std::uint8_t> packBitValuesBinary(const std::vector<std::uint16_t>& values, PlcSeries series, std::size_t length) {
    if (series == PlcSeries::IQ_R) {
        // iQ-R はビットでも 2 バイト幅で値を保持するため 16bit 毎に格納する。
        std::vector<std::uint8_t> bytes;
        bytes.reserve(length * 2);
        for (std::size_t i = 0; i < length; ++i) {
            appendLittleEndian(bytes, static_cast<std::uint16_t>(values[i] ? 1 : 0), 2);
        }
        return bytes;
    }

    // それ以外のシリーズは pymcprotocol と同様に 1 バイトに 2 ビット詰めで扱う。
    std::vector<std::uint8_t> packed((length + 1) / 2, 0);
    for (std::size_t idx = 0; idx < length; ++idx) {
        const std::size_t byte_index = idx / 2;
        const bool high_nibble = (idx % 2 == 0);
        const std::uint8_t bit_mask = high_nibble ? 0x10 : 0x01;
        if (values[idx]) {
            packed[byte_index] |= bit_mask;
        }
    }
    return packed;
}

std::string packBitValuesAscii(const std::vector<std::uint16_t>& values, PlcSeries series, std::size_t length) {
    std::string result;
    result.reserve(length * (series == PlcSeries::IQ_R ? 4 : 1));
    for (std::size_t i = 0; i < length; ++i) {
        if (series == PlcSeries::IQ_R) {
            result += toHex(static_cast<std::uint32_t>(values[i] ? 1 : 0), 4);
        } else {
            result += values[i] ? '1' : '0';
        }
    }
    return result;
}

} // namespace

FrameEncoder::FrameEncoder() = default;

std::vector<std::uint8_t> FrameEncoder::makeBatchReadRequest(const SessionConfig& config, const DeviceRange& range) const {
    if (range.length == 0) {
        throw std::invalid_argument("DeviceRange.length must be greater than zero");
    }

    const auto subcommand = sequentialSubcommand(range.head.type, config.series);

    if (config.mode == CommunicationMode::Ascii) {
        const auto info = device_code_map_.resolveAscii(config.series, range.head.name);
        const auto number = parseDeviceNumber(range.head.name, info.number_base);
        std::string request;
        request += toHex(0x0401, 4);
        request += toHex(subcommand, 4);
        appendDeviceAscii(request, info, number);
        request += toHex(range.length, 4);
        return buildAsciiFrame(config, request);
    }

    const auto info = device_code_map_.resolveBinary(config.series, range.head.name);
    const auto number = parseDeviceNumber(range.head.name, info.number_base);
    std::vector<std::uint8_t> request;
    appendLittleEndian(request, 0x0401, 2);
    appendLittleEndian(request, subcommand, 2);
    appendDeviceBinary(request, info, number);
    appendLittleEndian(request, range.length, 2);
    return buildBinaryFrame(config, request);
}

std::vector<std::uint8_t> FrameEncoder::makeBatchWriteRequest(const SessionConfig& config,
                                                              const DeviceRange& range,
                                                              const std::vector<std::uint16_t>& data) const {
    if (range.length == 0 || data.size() < range.length) {
        throw std::invalid_argument("Insufficient write data");
    }

    const auto subcommand = sequentialSubcommand(range.head.type, config.series);

    if (config.mode == CommunicationMode::Ascii) {
        const auto info = device_code_map_.resolveAscii(config.series, range.head.name);
        const auto number = parseDeviceNumber(range.head.name, info.number_base);
        std::string request;
        request += toHex(0x1401, 4);
        request += toHex(subcommand, 4);
        appendDeviceAscii(request, info, number);
        request += toHex(range.length, 4);
        if (range.head.type == DeviceType::Bit) {
            request += packBitValuesAscii(data, config.series, range.length);
        } else {
            for (std::size_t i = 0; i < range.length; ++i) {
                request += toHex(data[i], 4);
            }
        }
        return buildAsciiFrame(config, request);
    }

    const auto info = device_code_map_.resolveBinary(config.series, range.head.name);
    const auto number = parseDeviceNumber(range.head.name, info.number_base);
    std::vector<std::uint8_t> request;
    appendLittleEndian(request, 0x1401, 2);
    appendLittleEndian(request, subcommand, 2);
    appendDeviceBinary(request, info, number);
    appendLittleEndian(request, range.length, 2);
    if (range.head.type == DeviceType::Bit) {
        auto packed = packBitValuesBinary(data, config.series, range.length);
        request.insert(request.end(), packed.begin(), packed.end());
    } else {
        for (std::size_t i = 0; i < range.length; ++i) {
            appendLittleEndian(request, data[i], 2);
        }
    }
    return buildBinaryFrame(config, request);
}

std::vector<std::uint8_t> FrameEncoder::makeRandomReadRequest(const SessionConfig& config,
                                                              const RandomDeviceRequest& request) const {
    if (!request.bit_devices.empty()) {
        // pymcprotocol 互換の都合でランダムビットアクセスは未実装。
        throw std::invalid_argument("Random read for bit devices is not supported");
    }

    const auto subcommand = randomWordSubcommand(config.series);
    const std::uint16_t word_count = static_cast<std::uint16_t>(request.word_devices.size());
    const std::uint16_t dword_count = static_cast<std::uint16_t>(request.dword_devices.size());

    if (config.mode == CommunicationMode::Ascii) {
        std::string req;
        req += toHex(0x0403, 4);
        req += toHex(subcommand, 4);
        req += toHex(word_count, 2);
        req += toHex(dword_count, 2);
        for (const auto& device : request.word_devices) {
            const auto info = device_code_map_.resolveAscii(config.series, device.name);
            const auto number = parseDeviceNumber(device.name, info.number_base);
            appendDeviceAscii(req, info, number);
        }
        for (const auto& device : request.dword_devices) {
            const auto info = device_code_map_.resolveAscii(config.series, device.name);
            const auto number = parseDeviceNumber(device.name, info.number_base);
            appendDeviceAscii(req, info, number);
        }
        return buildAsciiFrame(config, req);
    }

    std::vector<std::uint8_t> req;
    appendLittleEndian(req, 0x0403, 2);
    appendLittleEndian(req, subcommand, 2);
    req.push_back(static_cast<std::uint8_t>(word_count));
    req.push_back(static_cast<std::uint8_t>(dword_count));
    for (const auto& device : request.word_devices) {
        const auto info = device_code_map_.resolveBinary(config.series, device.name);
        const auto number = parseDeviceNumber(device.name, info.number_base);
        appendDeviceBinary(req, info, number);
    }
    for (const auto& device : request.dword_devices) {
        const auto info = device_code_map_.resolveBinary(config.series, device.name);
        const auto number = parseDeviceNumber(device.name, info.number_base);
        appendDeviceBinary(req, info, number);
    }
    return buildBinaryFrame(config, req);
}

std::vector<std::uint8_t> FrameEncoder::makeRandomWriteRequest(const SessionConfig& config,
                                                               const RandomDeviceRequest& request,
                                                               const std::vector<std::uint16_t>& word_data,
                                                               const std::vector<std::uint32_t>& dword_data,
                                                               const std::vector<bool>& bit_data) const {
    if (!request.bit_devices.empty()) {
        // ランダムビット書き込みも現状サポートしていないことを明示する。
        throw std::invalid_argument("Random bit write is not implemented yet");
    }
    if (request.word_devices.size() != word_data.size()) {
        throw std::invalid_argument("word device/value count mismatch");
    }
    if (request.dword_devices.size() != dword_data.size()) {
        throw std::invalid_argument("dword device/value count mismatch");
    }

    const auto subcommand = randomWordSubcommand(config.series);
    const std::uint16_t word_count = static_cast<std::uint16_t>(request.word_devices.size());
    const std::uint16_t dword_count = static_cast<std::uint16_t>(request.dword_devices.size());

    if (config.mode == CommunicationMode::Ascii) {
        std::string req;
        req += toHex(0x1402, 4);
        req += toHex(subcommand, 4);
        req += toHex(word_count, 2);
        req += toHex(dword_count, 2);
        for (std::size_t i = 0; i < request.word_devices.size(); ++i) {
            const auto& device = request.word_devices[i];
            const auto info = device_code_map_.resolveAscii(config.series, device.name);
            const auto number = parseDeviceNumber(device.name, info.number_base);
            appendDeviceAscii(req, info, number);
            req += toHex(word_data[i], 4);
        }
        for (std::size_t i = 0; i < request.dword_devices.size(); ++i) {
            const auto& device = request.dword_devices[i];
            const auto info = device_code_map_.resolveAscii(config.series, device.name);
            const auto number = parseDeviceNumber(device.name, info.number_base);
            appendDeviceAscii(req, info, number);
            req += toHex(dword_data[i], 8);
        }
        return buildAsciiFrame(config, req);
    }

    std::vector<std::uint8_t> req;
    appendLittleEndian(req, 0x1402, 2);
    appendLittleEndian(req, subcommand, 2);
    req.push_back(static_cast<std::uint8_t>(word_count));
    req.push_back(static_cast<std::uint8_t>(dword_count));
    for (std::size_t i = 0; i < request.word_devices.size(); ++i) {
        const auto& device = request.word_devices[i];
        const auto info = device_code_map_.resolveBinary(config.series, device.name);
        const auto number = parseDeviceNumber(device.name, info.number_base);
        appendDeviceBinary(req, info, number);
        appendLittleEndian(req, word_data[i], 2);
    }
    for (std::size_t i = 0; i < request.dword_devices.size(); ++i) {
        const auto& device = request.dword_devices[i];
        const auto info = device_code_map_.resolveBinary(config.series, device.name);
        const auto number = parseDeviceNumber(device.name, info.number_base);
        appendDeviceBinary(req, info, number);
        appendLittleEndian(req, dword_data[i], 4);
    }
    return buildBinaryFrame(config, req);
}

} // namespace cpmcprotocol::codec
