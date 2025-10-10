#pragma once

#include <cstdint>

#include "cpmcprotocol/communication_mode.hpp"

namespace cpmcprotocol {

struct AccessOption {
    CommunicationMode mode = CommunicationMode::Binary;
    std::uint8_t network = 0;
    std::uint8_t pc = 0xFF;
    std::uint16_t module_io = 0x03FF;
    std::uint8_t module_station = 0;
    std::uint16_t timeout_seconds = 1; // TODO: convert to 250ms units in transport implementation
};

} // namespace cpmcprotocol
