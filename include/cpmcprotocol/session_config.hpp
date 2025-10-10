#pragma once

#include <cstdint>
#include <string>

#include "cpmcprotocol/communication_mode.hpp"
#include "cpmcprotocol/device.hpp"

namespace cpmcprotocol {

struct SessionConfig {
    std::string host;
    std::uint16_t port = 0;
    std::uint8_t network = 0;
    std::uint8_t pc = 0xFF;
    std::uint16_t module_io = 0x03FF;
    std::uint8_t module_station = 0;
    std::uint16_t timeout_250ms = 4; // Default 1 second
    PlcSeries series = PlcSeries::IQ_R;
    CommunicationMode mode = CommunicationMode::Binary;

    // TODO: add validation helpers (e.g., to check ranges before connect)
};

} // namespace cpmcprotocol
