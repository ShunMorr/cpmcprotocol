#include "cpmcprotocol/value_codec.hpp"

// ValueCodec は低レイヤフレームから得られたワード列を用途別の型へ変換する責務を持つ。

#include <cstring>
#include <sstream>
#include <stdexcept>

namespace cpmcprotocol {

namespace {

std::size_t wordsRequired(const ValueFormat& format) {
    switch (format.type) {
        case ValueType::Int16:
        case ValueType::UInt16:
            return 1;
        case ValueType::Int32:
        case ValueType::UInt32:
        case ValueType::Float32:
            return 2;
        case ValueType::Float64:
            return 4;
        case ValueType::AsciiString: {
            if (format.parameter == 0) {
                throw std::invalid_argument("AsciiString requires positive length");
            }
            return (format.parameter + 1) / 2;
        }
        case ValueType::RawWords:
            if (format.parameter == 0) {
                throw std::invalid_argument("RawWords requires positive word count");
            }
            return format.parameter;
        case ValueType::BitArray:
            if (format.parameter == 0) {
                throw std::invalid_argument("BitArray requires positive bit count");
            }
            return (format.parameter + 1) / 2;
    }
    throw std::invalid_argument("Unsupported ValueType");
}

std::string toHexPadded(std::uint32_t value, std::size_t width) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex;
    oss.width(static_cast<std::streamsize>(width));
    oss.fill('0');
    oss << value;
    return oss.str();
}

} // namespace

std::vector<DeviceValue> ValueCodec::decode(const DeviceReadPlan& plan, const std::vector<std::uint16_t>& words) const {
    std::vector<DeviceValue> result;
    result.reserve(plan.size());

    std::size_t offset = 0;
    for (const auto& entry : plan) {
        const std::size_t required = wordsRequired(entry.format);
        if (offset + required > words.size()) {
            throw std::invalid_argument("Insufficient word data for decode");
        }

        const auto* base = words.data() + offset;

        switch (entry.format.type) {
            case ValueType::Int16: {
                result.emplace_back(static_cast<int16_t>(base[0]));
                break;
            }
            case ValueType::UInt16: {
                result.emplace_back(static_cast<std::uint16_t>(base[0]));
                break;
            }
            case ValueType::Int32: {
                std::uint32_t value = static_cast<std::uint32_t>(base[0]) |
                                       (static_cast<std::uint32_t>(base[1]) << 16);
                result.emplace_back(static_cast<std::int32_t>(value));
                break;
            }
            case ValueType::UInt32: {
                std::uint32_t value = static_cast<std::uint32_t>(base[0]) |
                                       (static_cast<std::uint32_t>(base[1]) << 16);
                result.emplace_back(value);
                break;
            }
            case ValueType::Float32: {
                std::uint32_t raw = static_cast<std::uint32_t>(base[0]) |
                                     (static_cast<std::uint32_t>(base[1]) << 16);
                float value;
                std::memcpy(&value, &raw, sizeof(float));
                result.emplace_back(value);
                break;
            }
            case ValueType::Float64: {
                std::uint64_t raw = static_cast<std::uint64_t>(base[0]) |
                                    (static_cast<std::uint64_t>(base[1]) << 16) |
                                    (static_cast<std::uint64_t>(base[2]) << 32) |
                                    (static_cast<std::uint64_t>(base[3]) << 48);
                double value;
                std::memcpy(&value, &raw, sizeof(double));
                result.emplace_back(value);
                break;
            }
            case ValueType::AsciiString: {
                const std::size_t length = entry.format.parameter;
                std::string text;
                text.reserve(length);
                // pymcprotocol と同じくローバイト→ハイバイトの順で ASCII 文字を復元する。
                for (std::size_t i = 0; i < required && text.size() < length; ++i) {
                    char low = static_cast<char>(base[i] & 0xFF);
                    if (low == '\0') {
                        break;
                    }
                    text.push_back(low);
                    if (text.size() >= length) {
                        break;
                    }
                    char high = static_cast<char>((base[i] >> 8) & 0xFF);
                    if (high == '\0') {
                        break;
                    }
                    text.push_back(high);
                }
                result.emplace_back(std::move(text));
                break;
            }
            case ValueType::RawWords: {
                std::vector<std::uint16_t> segment(required);
                std::memcpy(segment.data(), base, required * sizeof(std::uint16_t));
                result.emplace_back(std::move(segment));
                break;
            }
            case ValueType::BitArray: {
                const std::size_t bit_count = entry.format.parameter;
                std::vector<bool> bits;
                bits.reserve(bit_count);
                for (std::size_t i = 0; i < required; ++i) {
                    const std::uint16_t word = base[i];
                    const std::uint8_t packed = static_cast<std::uint8_t>(word & 0xFF);
                    const std::size_t even_index = 2 * i;
                    // 上位ニブルが偶数番ビット、下位ニブルが奇数番ビットを表す。
                    if (even_index < bit_count) {
                        bits.push_back(((packed >> 4) & 0x1) != 0);
                    }
                    const std::size_t odd_index = even_index + 1;
                    if (odd_index < bit_count) {
                        bits.push_back((packed & 0x1) != 0);
                    }
                }
                result.emplace_back(std::move(bits));
                break;
            }
        }

        offset += required;
    }

    if (offset != words.size()) {
        throw std::invalid_argument("Unused word data remains after decode");
    }

    return result;
}

static const std::uint16_t* expectWordValue(const DeviceValue& value, std::uint16_t& storage) {
    if (auto ptr = std::get_if<std::uint16_t>(&value)) {
        return ptr;
    }
    if (auto ptr16 = std::get_if<std::int16_t>(&value)) {
        storage = static_cast<std::uint16_t>(*ptr16);
        return &storage;
    }
    return nullptr;
}

std::vector<std::uint16_t> ValueCodec::encode(const DeviceWritePlan& plan) const {
    std::vector<std::uint16_t> words;
    for (const auto& entry : plan) {
        const std::size_t required = wordsRequired(entry.format);
        switch (entry.format.type) {
            case ValueType::Int16:
            case ValueType::UInt16: {
                std::uint16_t storage = 0;
                const std::uint16_t* value = expectWordValue(entry.value, storage);
                if (!value) {
                    throw std::invalid_argument("DeviceValue does not match 16-bit format");
                }
                words.push_back(*value);
                break;
            }
            case ValueType::Int32: {
                if (!std::holds_alternative<std::int32_t>(entry.value)) {
                    throw std::invalid_argument("DeviceValue does not match Int32 format");
                }
                std::uint32_t raw = static_cast<std::uint32_t>(std::get<std::int32_t>(entry.value));
                words.push_back(static_cast<std::uint16_t>(raw & 0xFFFF));
                words.push_back(static_cast<std::uint16_t>((raw >> 16) & 0xFFFF));
                break;
            }
            case ValueType::UInt32: {
                if (!std::holds_alternative<std::uint32_t>(entry.value)) {
                    throw std::invalid_argument("DeviceValue does not match UInt32 format");
                }
                std::uint32_t raw = std::get<std::uint32_t>(entry.value);
                words.push_back(static_cast<std::uint16_t>(raw & 0xFFFF));
                words.push_back(static_cast<std::uint16_t>((raw >> 16) & 0xFFFF));
                break;
            }
            case ValueType::Float32: {
                if (!std::holds_alternative<float>(entry.value)) {
                    throw std::invalid_argument("DeviceValue does not match Float32 format");
                }
                float val = std::get<float>(entry.value);
                std::uint32_t raw;
                std::memcpy(&raw, &val, sizeof(float));
                words.push_back(static_cast<std::uint16_t>(raw & 0xFFFF));
                words.push_back(static_cast<std::uint16_t>((raw >> 16) & 0xFFFF));
                break;
            }
            case ValueType::Float64: {
                if (!std::holds_alternative<double>(entry.value)) {
                    throw std::invalid_argument("DeviceValue does not match Float64 format");
                }
                double val = std::get<double>(entry.value);
                std::uint64_t raw;
                std::memcpy(&raw, &val, sizeof(double));
                for (int i = 0; i < 4; ++i) {
                    words.push_back(static_cast<std::uint16_t>((raw >> (16 * i)) & 0xFFFF));
                }
                break;
            }
            case ValueType::AsciiString: {
                if (!std::holds_alternative<std::string>(entry.value)) {
                    throw std::invalid_argument("DeviceValue does not match AsciiString format");
                }
                const auto& text = std::get<std::string>(entry.value);
                const std::size_t length = entry.format.parameter;
                if (text.size() > length) {
                    throw std::invalid_argument("ASCII string exceeds specified length");
                }
                std::string padded = text;
                padded.resize(length, '\0');
                for (std::size_t i = 0; i < (length + 1) / 2; ++i) {
                    const char low = padded.size() > (2 * i) ? padded[2 * i] : '\0';
                    const char high = padded.size() > (2 * i + 1) ? padded[2 * i + 1] : '\0';
                    std::uint16_t word = static_cast<std::uint16_t>(static_cast<unsigned char>(low)) |
                                          (static_cast<std::uint16_t>(static_cast<unsigned char>(high)) << 8);
                    words.push_back(word);
                }
                break;
            }
            case ValueType::RawWords: {
                if (!std::holds_alternative<std::vector<std::uint16_t>>(entry.value)) {
                    throw std::invalid_argument("DeviceValue does not match RawWords format");
                }
                const auto& raw = std::get<std::vector<std::uint16_t>>(entry.value);
                if (raw.size() != required) {
                    throw std::invalid_argument("RawWords value count mismatch");
                }
                words.insert(words.end(), raw.begin(), raw.end());
                break;
            }
            case ValueType::BitArray: {
                if (!std::holds_alternative<std::vector<bool>>(entry.value)) {
                    throw std::invalid_argument("DeviceValue does not match BitArray format");
                }
                const auto& bits = std::get<std::vector<bool>>(entry.value);
                const std::size_t bit_count = entry.format.parameter;
                if (bits.size() != bit_count) {
                    throw std::invalid_argument("BitArray value count mismatch");
                }
                for (std::size_t i = 0; i < required; ++i) {
                    const std::size_t even_index = 2 * i;
                    const std::size_t odd_index = even_index + 1;
                    std::uint8_t byte = 0;
                    // 偶数番ビットは上位ニブル、奇数番ビットは下位ニブルに格納する。
                    if (even_index < bit_count && bits[even_index]) {
                        byte |= 0x10;
                    }
                    if (odd_index < bit_count && bits[odd_index]) {
                        byte |= 0x01;
                    }
                    // フレーム層では1バイトのビットデータを下位8bitに格納する
                    words.push_back(static_cast<std::uint16_t>(byte));
                }
                break;
            }
        }
    }

    return words;
}

std::vector<std::uint16_t> ValueCodec::fromBinaryBytes(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() % 2 != 0) {
        throw std::invalid_argument("Binary byte stream length must be even");
    }
    std::vector<std::uint16_t> words(bytes.size() / 2);
    for (std::size_t i = 0; i < words.size(); ++i) {
        words[i] = static_cast<std::uint16_t>(bytes[2 * i]) |
                   (static_cast<std::uint16_t>(bytes[2 * i + 1]) << 8);
    }
    return words;
}

std::vector<std::uint16_t> ValueCodec::fromAsciiWords(const std::vector<std::uint8_t>& ascii) {
    if (ascii.size() % 4 != 0) {
        throw std::invalid_argument("ASCII word stream must be a multiple of 4 characters");
    }
    std::vector<std::uint16_t> words(ascii.size() / 4);
    for (std::size_t i = 0; i < words.size(); ++i) {
        std::string hex(reinterpret_cast<const char*>(ascii.data() + 4 * i), 4);
        std::uint32_t value = static_cast<std::uint32_t>(std::stoul(hex, nullptr, 16));
        words[i] = static_cast<std::uint16_t>(value & 0xFFFF);
    }
    return words;
}

std::vector<std::uint8_t> ValueCodec::toBinaryBytes(const std::vector<std::uint16_t>& words) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(words.size() * 2);
    for (auto word : words) {
        bytes.push_back(static_cast<std::uint8_t>(word & 0xFF));
        bytes.push_back(static_cast<std::uint8_t>((word >> 8) & 0xFF));
    }
    return bytes;
}

std::vector<std::uint8_t> ValueCodec::toAsciiWords(const std::vector<std::uint16_t>& words) {
    std::vector<std::uint8_t> ascii;
    ascii.reserve(words.size() * 4);
    for (auto word : words) {
        std::string hex = toHexPadded(word, 4);
        ascii.insert(ascii.end(), hex.begin(), hex.end());
    }
    return ascii;
}

} // namespace cpmcprotocol
