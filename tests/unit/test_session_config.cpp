#include "cpmcprotocol/session_config.hpp"

#include <cassert>
#include <string>

int main() {
    using namespace cpmcprotocol;

    // Test 1: Valid configuration
    {
        SessionConfig config{};
        config.host = "192.168.1.10";
        config.port = 5000;
        config.network = 0;
        config.pc = 0xFF;
        config.module_io = 0x03FF;
        config.module_station = 0;
        config.timeout_250ms = 4;
        config.series = PlcSeries::IQ_R;

        assert(config.isValid());

        // Should not throw
        config.validate();
    }

    // Test 2: Empty host
    {
        SessionConfig config{};
        config.host = "";
        config.port = 5000;

        std::string error;
        assert(!config.isValid(&error));
        assert(error.find("Host") != std::string::npos);

        bool threw = false;
        try {
            config.validate();
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);
    }

    // Test 3: Zero port
    {
        SessionConfig config{};
        config.host = "localhost";
        config.port = 0;

        assert(!config.isValid());

        auto errors = config.getValidationErrors();
        bool found_port_error = false;
        for (const auto& err : errors) {
            if (err.find("Port") != std::string::npos) {
                found_port_error = true;
                break;
            }
        }
        assert(found_port_error);
    }

    // Test 4: Invalid network number
    {
        SessionConfig config{};
        config.host = "localhost";
        config.port = 5000;
        config.network = 250;  // Invalid: > 239

        assert(!config.isValid());
    }

    // Test 5: Invalid module I/O for non-iQ-R
    {
        SessionConfig config{};
        config.host = "localhost";
        config.port = 5000;
        config.series = PlcSeries::Q;
        config.module_io = 0x0500;  // Invalid: > 0x03FF for Q series

        assert(!config.isValid());
    }

    // Test 6: Valid module I/O for iQ-R
    {
        SessionConfig config{};
        config.host = "localhost";
        config.port = 5000;
        config.series = PlcSeries::IQ_R;
        config.module_io = 0x0500;  // Valid for iQ-R

        // Should have no module_io related errors
        auto errors = config.getValidationErrors();
        for (const auto& err : errors) {
            assert(err.find("Module I/O") == std::string::npos);
        }
    }

    // Test 7: Zero timeout
    {
        SessionConfig config{};
        config.host = "localhost";
        config.port = 5000;
        config.timeout_250ms = 0;

        assert(!config.isValid());
    }

    // Test 8: Very large timeout (warning)
    {
        SessionConfig config{};
        config.host = "localhost";
        config.port = 5000;
        config.timeout_250ms = 500;  // > 240 (60 seconds)

        auto errors = config.getValidationErrors();
        bool found_timeout_warning = false;
        for (const auto& err : errors) {
            if (err.find("Timeout") != std::string::npos &&
                err.find("large") != std::string::npos) {
                found_timeout_warning = true;
                break;
            }
        }
        assert(found_timeout_warning);
    }

    // Test 9: Multiple validation errors
    {
        SessionConfig config{};
        config.host = "";
        config.port = 0;
        config.network = 250;

        auto errors = config.getValidationErrors();
        assert(errors.size() >= 3);  // At least 3 errors

        std::string combined_error;
        assert(!config.isValid(&combined_error));
        assert(!combined_error.empty());
        assert(combined_error.find(";") != std::string::npos);  // Multiple errors joined
    }

    return 0;
}
