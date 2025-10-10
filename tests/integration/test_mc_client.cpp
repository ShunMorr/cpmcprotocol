#include "cpmcprotocol/mc_client.hpp"
#include "cpmcprotocol/session_config.hpp"

#include <iostream>

// TODO: add TcpTransport loopback verification against a mock SLMP server.

int main() {
    using namespace cpmcprotocol;
    McClient client;
    SessionConfig config{};

    client.connect(config);
    std::cout << "Client connected: " << client.isConnected() << '\n';
    client.disconnect();
    std::cout << "Client connected: " << client.isConnected() << '\n';

    // TODO: replace with real integration tests against stub server
    return 0;
}
