#include "cpmcprotocol/mc_client.hpp"

#include "cpmcprotocol/access_option.hpp"
#include "cpmcprotocol/device.hpp"
#include "cpmcprotocol/runtime_control.hpp"
#include "cpmcprotocol/session_config.hpp"

#include <vector>

namespace cpmcprotocol {

struct McClient::Impl {
    // TODO: wire transport and codec layers once implemented
    bool connected = false;
};

McClient::McClient() : impl_(new Impl) {}
McClient::~McClient() = default;

void McClient::connect(const SessionConfig& /*config*/) {
    // TODO: initialize transport connection
    impl_->connected = true;
}

void McClient::disconnect() {
    // TODO: close transport connection
    impl_->connected = false;
}

bool McClient::isConnected() const noexcept {
    return impl_->connected;
}

void McClient::setAccessOption(const AccessOption& /*option*/) {
    // TODO: apply access options to transport/session
}

void McClient::readWords(const DeviceRange& /*range*/) {
    // TODO: delegate to codec + transport
}

void McClient::readBits(const DeviceRange& /*range*/) {
    // TODO
}

void McClient::writeWords(const DeviceRange& /*range*/, const std::vector<std::uint16_t>& /*values*/) {
    // TODO
}

void McClient::writeBits(const DeviceRange& /*range*/, const std::vector<bool>& /*values*/) {
    // TODO
}

void McClient::randomRead(const RandomDeviceRequest& /*request*/) {
    // TODO
}

void McClient::randomWrite(const RandomDeviceRequest& /*request*/) {
    // TODO
}

CpuInfo McClient::readCpuType() {
    // TODO: send MC command and parse response
    return {};
}

void McClient::applyRuntimeControl(const RuntimeControl& /*command*/) {
    // TODO: map runtime command to MC protocol message
}

} // namespace cpmcprotocol
