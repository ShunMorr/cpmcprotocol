#include "cpmcprotocol/value_codec.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    using namespace cpmcprotocol;

    ValueCodec codec;

    DeviceWritePlan write_plan{
        {DeviceAddress{"D0", DeviceType::Word}, ValueFormat::Int16(), static_cast<int16_t>(-16)},
        {DeviceAddress{"D1", DeviceType::Word}, ValueFormat::UInt16(), static_cast<std::uint16_t>(0x1234)},
        {DeviceAddress{"D2", DeviceType::DoubleWord}, ValueFormat::Int32(), static_cast<int32_t>(0x9ABC5678)},
        {DeviceAddress{"D4", DeviceType::Word}, ValueFormat::Float32(), 1.0f},
        {DeviceAddress{"D6", DeviceType::DoubleWord}, ValueFormat::Float64(), 1.0},
        {DeviceAddress{"D10", DeviceType::Word}, ValueFormat::AsciiString(5), std::string("HELLO")},
        {DeviceAddress{"D20", DeviceType::Word}, ValueFormat::RawWords(2), std::vector<std::uint16_t>{0xAA55, 0x0F0F}},
        {DeviceAddress{"D30", DeviceType::Bit}, ValueFormat::BitArray(3), std::vector<bool>{true, false, true}}
    };

    auto encoded = codec.encode(write_plan);

    std::vector<std::uint16_t> expected_words = {
        0xFFF0,             // Int16
        0x1234,             // UInt16
        0x5678, 0x9ABC,     // Int32 (little-endian words)
        0x0000, 0x3F80,     // Float32 (1.0f)
        0x0000, 0x0000, 0x0000, 0x3FF0, // Float64 (1.0)
        0x4548, 0x4C4C, 0x004F,         // ASCII "HELLO"
        0xAA55, 0x0F0F,                 // Raw words
        0x0010, 0x0010                  // Bit array values packed per pymcprotocol
    };

    assert(encoded == expected_words);

    DeviceReadPlan plan{
        {DeviceAddress{"D0", DeviceType::Word}, ValueFormat::Int16()},
        {DeviceAddress{"D1", DeviceType::Word}, ValueFormat::UInt16()},
        {DeviceAddress{"D2", DeviceType::DoubleWord}, ValueFormat::Int32()},
        {DeviceAddress{"D4", DeviceType::Word}, ValueFormat::Float32()},
        {DeviceAddress{"D6", DeviceType::DoubleWord}, ValueFormat::Float64()},
        {DeviceAddress{"D10", DeviceType::Word}, ValueFormat::AsciiString(5)},
        {DeviceAddress{"D20", DeviceType::Word}, ValueFormat::RawWords(2)},
        {DeviceAddress{"D30", DeviceType::Bit}, ValueFormat::BitArray(3)}
    };

    auto decoded = codec.decode(plan, encoded);
    assert(std::get<int16_t>(decoded[0]) == static_cast<int16_t>(-16));
    assert(std::get<uint16_t>(decoded[1]) == 0x1234);
    assert(std::get<int32_t>(decoded[2]) == static_cast<int32_t>(0x9ABC5678));
    assert(std::fabs(std::get<float>(decoded[3]) - 1.0f) < 1e-6f);
    assert(std::fabs(std::get<double>(decoded[4]) - 1.0) < 1e-12);
    assert(std::get<std::string>(decoded[5]) == "HELLO");
    auto raw = std::get<std::vector<std::uint16_t>>(decoded[6]);
    assert(raw.size() == 2 && raw[0] == 0xAA55 && raw[1] == 0x0F0F);
    auto bits = std::get<std::vector<bool>>(decoded[7]);
    assert(bits.size() == 3 && bits[0] && !bits[1] && bits[2]);

    // Binary / ASCII helper verification
    auto binary_bytes = ValueCodec::toBinaryBytes({0x1234, 0xABCD});
    auto binary_words = ValueCodec::fromBinaryBytes(binary_bytes);
    assert(binary_words.size() == 2 && binary_words[0] == 0x1234 && binary_words[1] == 0xABCD);

    auto ascii_bytes = ValueCodec::toAsciiWords({0x5678});
    auto ascii_words = ValueCodec::fromAsciiWords(ascii_bytes);
    assert(ascii_words.size() == 1 && ascii_words[0] == 0x5678);

    // Error cases
    bool threw = false;
    try {
        DeviceReadPlan bad_plan{{DeviceAddress{"D0", DeviceType::Word}, ValueFormat::Int32()}};
        codec.decode(bad_plan, {0x0001});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        DeviceWritePlan wrong_value{{DeviceAddress{"D0", DeviceType::Word}, ValueFormat::Int16(), std::string("oops")}};
        codec.encode(wrong_value);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    return 0;
}
