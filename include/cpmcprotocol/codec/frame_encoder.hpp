#pragma once

#include <cstdint>
#include <vector>

#include "cpmcprotocol/codec/device_code_map.hpp"
#include "cpmcprotocol/device.hpp"
#include "cpmcprotocol/session_config.hpp"

namespace cpmcprotocol::codec {

class FrameEncoder {
public:
    FrameEncoder();

    // Encode interfaces for MC protocol operations
    std::vector<std::uint8_t> makeBatchReadRequest(const SessionConfig& config, const DeviceRange& range) const;
    std::vector<std::uint8_t> makeBatchWriteRequest(const SessionConfig& config, const DeviceRange& range, const std::vector<std::uint16_t>& data) const;
    std::vector<std::uint8_t> makeRandomReadRequest(const SessionConfig& config, const RandomDeviceRequest& request) const;
    std::vector<std::uint8_t> makeRandomWriteRequest(const SessionConfig& config,
                                                     const RandomDeviceRequest& request,
                                                     const std::vector<std::uint16_t>& word_data,
                                                     const std::vector<std::uint32_t>& dword_data,
                                                     const std::vector<std::uint64_t>& lword_data,
                                                     const std::vector<bool>& bit_data) const;
    std::vector<std::uint8_t> makeSimpleCommand(const SessionConfig& config,
                                                std::uint16_t command,
                                                std::uint16_t subcommand,
                                                const std::vector<std::uint8_t>& binary_payload,
                                                const std::string& ascii_payload) const;

private:
    DeviceCodeMap device_code_map_;
};

} // namespace cpmcprotocol::codec
