#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

namespace cpmcprotocol::testutil {

class MockSlmpServer {
public:
    using Handler = std::function<std::vector<std::uint8_t>(const std::vector<std::uint8_t>&)>;

    MockSlmpServer();
    ~MockSlmpServer();

    MockSlmpServer(const MockSlmpServer&) = delete;
    MockSlmpServer& operator=(const MockSlmpServer&) = delete;

    void start(std::uint16_t port, Handler handler);
    void stop();
    bool isRunning() const;

private:
    void run(std::uint16_t port, Handler handler);
    void wakeListener();

    std::atomic<bool> running_{false};
    std::atomic<std::uint16_t> port_{0};
    std::thread thread_;
};

} // namespace cpmcprotocol::testutil
