#pragma once

#include "cpmcprotocol/session_config.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace cpmcprotocol {

class TransportError : public std::runtime_error {
public:
    explicit TransportError(const std::string& message);
};

class TransportTimeoutError : public TransportError {
public:
    explicit TransportTimeoutError(const std::string& message);
};

class TcpTransport {
public:
    TcpTransport();
    ~TcpTransport();

    TcpTransport(const TcpTransport&) = delete;
    TcpTransport& operator=(const TcpTransport&) = delete;
    TcpTransport(TcpTransport&&) noexcept;
    TcpTransport& operator=(TcpTransport&&) noexcept;

    void connect(const SessionConfig& config);
    void disconnect() noexcept;
    bool isConnected() const noexcept;

    void setTimeout(std::chrono::milliseconds send_timeout,
                    std::chrono::milliseconds recv_timeout);

    void sendAll(const std::uint8_t* data, std::size_t size);
    void sendAll(const std::vector<std::uint8_t>& data);

    std::size_t receiveSome(std::uint8_t* buffer, std::size_t capacity);
    void receiveAll(std::uint8_t* buffer, std::size_t expected);
    std::vector<std::uint8_t> receiveAll(std::size_t expected);
    std::vector<std::uint8_t> receiveFrame(std::size_t header_size,
                                           const std::function<std::size_t(const std::uint8_t*, std::size_t)>& length_extractor);

private:
    void ensureConnected() const;
    void applySocketOptions();
    bool isTimeoutError(int error_code) const;
    void markDisconnected() noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cpmcprotocol
