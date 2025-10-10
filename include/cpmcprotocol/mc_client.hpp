#pragma once

#include <memory>
#include <string>
#include <vector>

namespace cpmcprotocol {

class SessionConfig;
class AccessOption;
class DeviceRange;
class RandomDeviceRequest;
class RuntimeControl;
struct CpuInfo;

class McClient {
public:
    McClient();
    ~McClient();

    // TODO: define ctor overloads for dependency injection (transport, codec layers)
    void connect(const SessionConfig& config);
    void disconnect();
    bool isConnected() const noexcept;

    // TODO: flesh out read/write APIs based on spec.md Section 8
    void setAccessOption(const AccessOption& option);

    // Placeholders for batch operations
    void readWords(const DeviceRange& range);
    void readBits(const DeviceRange& range);
    void writeWords(const DeviceRange& range, const std::vector<std::uint16_t>& values);
    void writeBits(const DeviceRange& range, const std::vector<bool>& values);

    void randomRead(const RandomDeviceRequest& request);
    void randomWrite(const RandomDeviceRequest& request);

    CpuInfo readCpuType();
    void applyRuntimeControl(const RuntimeControl& command);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cpmcprotocol
