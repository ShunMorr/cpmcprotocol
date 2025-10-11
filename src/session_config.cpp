#include "cpmcprotocol/session_config.hpp"

#include <sstream>

namespace cpmcprotocol {

std::vector<std::string> SessionConfig::getValidationErrors() const {
    std::vector<std::string> errors;

    // Host validation
    if (host.empty()) {
        errors.push_back("Host address is empty");
    }

    // Port validation
    if (port == 0) {
        errors.push_back("Port must be non-zero");
    }

    // Network number validation (0-255 by type, but ensure reasonable range)
    // Network 0 means local network, 1-239 are valid network numbers
    if (network > 239) {
        errors.push_back("Network number must be 0-239 (actual: " + std::to_string(network) + ")");
    }

    // PC number validation
    // 0xFF means direct connection, 1-120 are typical station numbers
    // 0 is reserved but sometimes used
    if (pc > 0 && pc < 0xFF && pc > 120) {
        errors.push_back("PC number should be 0, 1-120, or 0xFF for direct (actual: " + std::to_string(pc) + ")");
    }

    // Module I/O validation
    // 0x03FF (1023) means direct connection
    // Valid range: 0x0000-0x03FF for Q series, 0x0000-0x0FFF for iQ-R
    if (series == PlcSeries::IQ_R && module_io > 0x0FFF) {
        errors.push_back("Module I/O number for iQ-R must be 0x0000-0x0FFF (actual: 0x" +
                        std::to_string(module_io) + ")");
    } else if (series != PlcSeries::IQ_R && module_io > 0x03FF) {
        errors.push_back("Module I/O number must be 0x0000-0x03FF (actual: 0x" +
                        std::to_string(module_io) + ")");
    }

    // Module station validation (0-255 by type, but typical range is smaller)
    if (module_station > 16 && module_station != 0) {
        errors.push_back("Module station number typically 0-16 (actual: " +
                        std::to_string(module_station) + ")");
    }

    // Timeout validation
    if (timeout_250ms == 0) {
        errors.push_back("Timeout must be at least 1 (250ms)");
    }
    // Maximum timeout is typically 65535 * 250ms = ~16000 seconds
    // Warn if unreasonably large (> 1 minute = 240 units)
    if (timeout_250ms > 240) {
        std::ostringstream oss;
        oss << "Timeout is very large: " << timeout_250ms
            << " units (" << (timeout_250ms * 250 / 1000.0) << " seconds)";
        errors.push_back(oss.str());
    }

    return errors;
}

bool SessionConfig::isValid(std::string* error_message) const {
    auto errors = getValidationErrors();
    if (errors.empty()) {
        return true;
    }

    if (error_message) {
        std::ostringstream oss;
        for (std::size_t i = 0; i < errors.size(); ++i) {
            if (i > 0) {
                oss << "; ";
            }
            oss << errors[i];
        }
        *error_message = oss.str();
    }

    return false;
}

void SessionConfig::validate() const {
    std::string error_message;
    if (!isValid(&error_message)) {
        throw std::invalid_argument("SessionConfig validation failed: " + error_message);
    }
}

} // namespace cpmcprotocol
