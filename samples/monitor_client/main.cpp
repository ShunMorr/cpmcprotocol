#include "cpmcprotocol/mc_client.hpp"
#include "cpmcprotocol/session_config.hpp"

#include <iostream>

int main() {
    using namespace cpmcprotocol;

    SessionConfig config{};
    config.host = "127.0.0.1";
    config.port = 5000; // TODO: replace with PLC port configured for MC protocol

    McClient client;
    client.connect(config);

    std::cout << "Connected to PLC runtime stub (placeholder)." << std::endl;

    client.disconnect();
    return 0;
}
