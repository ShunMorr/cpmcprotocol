#pragma once

#include <cstdint>
#include <vector>

namespace cpmcprotocol::codec {

struct BatchReadResponse {
    std::uint16_t completion_code = 0;
    std::vector<std::uint8_t> device_data;
    std::vector<std::uint8_t> diagnostic_data;
};

struct BatchWriteResponse {
    std::uint16_t completion_code = 0;
    std::vector<std::uint8_t> diagnostic_data;
};

struct RandomReadResponse {
    std::uint16_t completion_code = 0;
    std::vector<std::uint8_t> device_data;
    std::vector<std::uint8_t> diagnostic_data;
};

struct RandomWriteResponse {
    std::uint16_t completion_code = 0;
    std::vector<std::uint8_t> diagnostic_data;
};

class FrameDecoder {
public:
    FrameDecoder();

    // TODO: parse response payload and extract values per spec Section 9
    BatchReadResponse parseBatchReadResponse(const std::vector<std::uint8_t>& frame) const;
    BatchWriteResponse parseBatchWriteResponse(const std::vector<std::uint8_t>& frame) const;
    RandomReadResponse parseRandomReadResponse(const std::vector<std::uint8_t>& frame) const;
    RandomWriteResponse parseRandomWriteResponse(const std::vector<std::uint8_t>& frame) const;
};

} // namespace cpmcprotocol::codec
