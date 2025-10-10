#pragma once

#include "cpmcprotocol/device.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace cpmcprotocol {

enum class ValueType {
    Int16,
    UInt16,
    Int32,
    UInt32,
    Float32,
    Float64,
    AsciiString,
    RawWords,
    BitArray
};

struct ValueFormat {
    ValueType type;
    std::size_t parameter = 0; // length / word count / bit count depending on type

    static ValueFormat Int16() { return {ValueType::Int16, 0}; }
    static ValueFormat UInt16() { return {ValueType::UInt16, 0}; }
    static ValueFormat Int32() { return {ValueType::Int32, 0}; }
    static ValueFormat UInt32() { return {ValueType::UInt32, 0}; }
    static ValueFormat Float32() { return {ValueType::Float32, 0}; }
    static ValueFormat Float64() { return {ValueType::Float64, 0}; }
    static ValueFormat AsciiString(std::size_t length) { return {ValueType::AsciiString, length}; }
    static ValueFormat RawWords(std::size_t words) { return {ValueType::RawWords, words}; }
    static ValueFormat BitArray(std::size_t bit_count) { return {ValueType::BitArray, bit_count}; }
};

using DeviceValue = std::variant<int16_t,
                                 uint16_t,
                                 int32_t,
                                 uint32_t,
                                 float,
                                 double,
                                 std::string,
                                 std::vector<std::uint16_t>,
                                 std::vector<bool>>;

struct DeviceReadPlanEntry {
    DeviceAddress address;
    ValueFormat format;
};

using DeviceReadPlan = std::vector<DeviceReadPlanEntry>;

struct DeviceWritePlanEntry {
    DeviceAddress address;
    ValueFormat format;
    DeviceValue value;
};

using DeviceWritePlan = std::vector<DeviceWritePlanEntry>;

class ValueCodec {
public:
    std::vector<DeviceValue> decode(const DeviceReadPlan& plan, const std::vector<std::uint16_t>& words) const;
    std::vector<std::uint16_t> encode(const DeviceWritePlan& plan) const;

    static std::vector<std::uint16_t> fromBinaryBytes(const std::vector<std::uint8_t>& bytes);
    static std::vector<std::uint16_t> fromAsciiWords(const std::vector<std::uint8_t>& ascii);
    static std::vector<std::uint8_t> toBinaryBytes(const std::vector<std::uint16_t>& words);
    static std::vector<std::uint8_t> toAsciiWords(const std::vector<std::uint16_t>& words);
};

} // namespace cpmcprotocol

