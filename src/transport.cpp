#include "cpmcprotocol/transport.hpp"

// TCP/3E 通信のソケットラッパ。タイムアウトや切断制御を一箇所で扱う。

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <cerrno>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace cpmcprotocol {

namespace {

#ifdef _WIN32
// Windows ソケット API 周りの薄いラッパ。POSIX と共通の呼び出し口を保つ。
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;

std::string lastSocketErrorMessage(int code) {
    if (code == 0) {
        code = WSAGetLastError();
    }
    LPSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    const DWORD result = FormatMessageA(flags, nullptr, static_cast<DWORD>(code), lang,
                                        reinterpret_cast<LPSTR>(&buffer), 0, nullptr);
    std::string message;
    if (result == 0) {
        message = "Unknown WSA error " + std::to_string(code);
    } else {
        message.assign(buffer, buffer + result);
    }
    if (buffer) {
        LocalFree(buffer);
    }
    return "WSA error " + std::to_string(code) + ": " + message;
}

std::string addrInfoErrorMessage(int code) {
    char buf[512] = {};
    if (gai_strerror_s(buf, sizeof(buf), code) != 0) {
        return "getaddrinfo failed with code " + std::to_string(code);
    }
    return std::string(buf);
}

void closeSocket(SocketHandle socket) {
    if (socket != kInvalidSocket) {
        closesocket(socket);
    }
}

void ensureWinsock() {
    static std::once_flag once;
    std::call_once(once, []() {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw TransportError("WSAStartup failed");
        }
    });
}

#else

using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;

// POSIX 系の errno を文字列に変換するユーティリティ。
std::string lastSocketErrorMessage(int code) {
    if (code == 0) {
        code = errno;
    }
    return std::string(std::strerror(code));
}

std::string addrInfoErrorMessage(int code) {
    return gai_strerror(code);
}

void closeSocket(SocketHandle socket) {
    if (socket != kInvalidSocket) {
        ::close(socket);
    }
}

void ensureWinsock() {
    // No-op on POSIX systems.
}

#endif

std::chrono::milliseconds deriveTimeout(const SessionConfig& config) {
    constexpr std::uint16_t kMinTicks = 1;
    auto ticks = std::max(kMinTicks, config.timeout_250ms);
    return std::chrono::milliseconds(ticks * 250);
}

} // namespace

TransportError::TransportError(const std::string& message)
    : std::runtime_error(message) {}

TransportTimeoutError::TransportTimeoutError(const std::string& message)
    : TransportError(message) {}

// PIMPL に実際のソケットやタイムアウト設定をまとめる。
struct TcpTransport::Impl {
    SocketHandle socket = kInvalidSocket;
    SessionConfig config{};
    std::chrono::milliseconds send_timeout{std::chrono::milliseconds{0}};
    std::chrono::milliseconds recv_timeout{std::chrono::milliseconds{0}};
};

TcpTransport::TcpTransport()
    : impl_(std::make_unique<Impl>()) {}

TcpTransport::~TcpTransport() {
    disconnect();
}

TcpTransport::TcpTransport(TcpTransport&& other) noexcept = default;
TcpTransport& TcpTransport::operator=(TcpTransport&& other) noexcept = default;

void TcpTransport::connect(const SessionConfig& config) {
    if (config.host.empty()) {
        throw TransportError("SessionConfig.host must not be empty");
    }
    if (config.port == 0) {
        throw TransportError("SessionConfig.port must be non-zero");
    }

    ensureWinsock();

    disconnect();

    impl_->config = config;
    impl_->send_timeout = deriveTimeout(config);
    impl_->recv_timeout = deriveTimeout(config);

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* results = nullptr;
    const std::string port_str = std::to_string(config.port);
    const int gai_result = ::getaddrinfo(config.host.c_str(), port_str.c_str(), &hints, &results);
    if (gai_result != 0) {
        throw TransportError(addrInfoErrorMessage(gai_result));
    }

    SocketHandle socket = kInvalidSocket;
    for (addrinfo* rp = results; rp != nullptr; rp = rp->ai_next) {
        socket = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (socket == kInvalidSocket) {
            continue;
        }

        const int connect_result = ::connect(socket, rp->ai_addr, static_cast<int>(rp->ai_addrlen));
        if (connect_result == 0) {
            impl_->socket = socket;
            break;
        }

        closeSocket(socket);
        socket = kInvalidSocket;
    }

    ::freeaddrinfo(results);

    if (impl_->socket == kInvalidSocket) {
        std::ostringstream oss;
        oss << "Failed to connect to " << config.host << ":" << config.port;
        throw TransportError(oss.str());
    }

    applySocketOptions();
}

void TcpTransport::disconnect() noexcept {
    markDisconnected();
}

bool TcpTransport::isConnected() const noexcept {
    return impl_ && impl_->socket != kInvalidSocket;
}

void TcpTransport::setTimeout(std::chrono::milliseconds send_timeout,
                              std::chrono::milliseconds recv_timeout) {
    impl_->send_timeout = send_timeout;
    impl_->recv_timeout = recv_timeout;
    if (isConnected()) {
        applySocketOptions();
    }
}

void TcpTransport::sendAll(const std::uint8_t* data, std::size_t size) {
    ensureConnected();
    if (size == 0) {
        return;
    }

    std::size_t total_sent = 0;
    while (total_sent < size) {
        const std::size_t remaining = size - total_sent;
        const std::size_t chunk_size =
            std::min<std::size_t>(remaining,
                                  static_cast<std::size_t>(std::numeric_limits<int>::max()));
#ifdef _WIN32
        const int sent = ::send(impl_->socket,
                                reinterpret_cast<const char*>(data + total_sent),
                                static_cast<int>(chunk_size), 0);
        if (sent == SOCKET_ERROR) {
            const int err = WSAGetLastError();
            if (isTimeoutError(err)) {
                throw TransportTimeoutError(lastSocketErrorMessage(err));
            }
            markDisconnected();
            throw TransportError(lastSocketErrorMessage(err));
        }
#else
        const auto sent =
            ::send(impl_->socket, data + total_sent, static_cast<int>(chunk_size), 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (isTimeoutError(errno)) {
                throw TransportTimeoutError(lastSocketErrorMessage(errno));
            }
            markDisconnected();
            throw TransportError(lastSocketErrorMessage(errno));
        }
#endif
        if (sent == 0) {
            markDisconnected();
            throw TransportError("Socket closed while sending");
        }
        total_sent += static_cast<std::size_t>(sent);
    }
}

void TcpTransport::sendAll(const std::vector<std::uint8_t>& data) {
    sendAll(data.data(), data.size());
}

std::size_t TcpTransport::receiveSome(std::uint8_t* buffer, std::size_t capacity) {
    ensureConnected();
    if (capacity == 0) {
        return 0;
    }

    const std::size_t chunk_size =
        std::min<std::size_t>(capacity,
                              static_cast<std::size_t>(std::numeric_limits<int>::max()));
#ifdef _WIN32
    const int received =
        ::recv(impl_->socket, reinterpret_cast<char*>(buffer), static_cast<int>(chunk_size), 0);
    if (received == 0) {
        markDisconnected();
        throw TransportError("Remote host closed the connection");
    }
    if (received == SOCKET_ERROR) {
        const int code = WSAGetLastError();
        if (code == WSAEINTR) {
            return receiveSome(buffer, capacity);
        }
        if (isTimeoutError(code)) {
            throw TransportTimeoutError(lastSocketErrorMessage(code));
        }
        markDisconnected();
        throw TransportError(lastSocketErrorMessage(code));
    }
#else
    int received = -1;
    while (true) {
        received = ::recv(impl_->socket, buffer, static_cast<int>(chunk_size), 0);
        if (received >= 0) {
            break;
        }
        if (errno == EINTR) {
            // Interrupted by signal, retry.
            continue;
        }
        if (isTimeoutError(errno)) {
            throw TransportTimeoutError(lastSocketErrorMessage(errno));
        }
        markDisconnected();
        throw TransportError(lastSocketErrorMessage(errno));
    }
    if (received == 0) {
        markDisconnected();
        throw TransportError("Remote host closed the connection");
    }
#endif
    return static_cast<std::size_t>(received);
}

void TcpTransport::receiveAll(std::uint8_t* buffer, std::size_t expected) {
    std::size_t total = 0;
    while (total < expected) {
        const std::size_t received = receiveSome(buffer + total, expected - total);
        if (received == 0) {
            throw TransportError("Timed out while receiving data");
        }
        total += received;
    }
}

std::vector<std::uint8_t> TcpTransport::receiveAll(std::size_t expected) {
    std::vector<std::uint8_t> buffer(expected);
    receiveAll(buffer.data(), expected);
    return buffer;
}

std::vector<std::uint8_t> TcpTransport::receiveFrame(std::size_t header_size,
                                                     const std::function<std::size_t(const std::uint8_t*, std::size_t)>& length_extractor) {
    if (header_size == 0) {
        throw TransportError("Header size must be greater than zero");
    }
    std::vector<std::uint8_t> header(header_size);
    try {
        receiveAll(header.data(), header_size);
    } catch (...) {
        markDisconnected();
        throw;
    }

    std::size_t body_size = 0;
    try {
        body_size = length_extractor(header.data(), header.size());
    } catch (...) {
        markDisconnected();
        throw;
    }

    if (body_size == 0) {
        markDisconnected();
        throw TransportError("Frame body length reported as zero");
    }

    std::vector<std::uint8_t> frame;
    frame.reserve(header_size + body_size);
    frame.insert(frame.end(), header.begin(), header.end());
    if (body_size > 0) {
        std::vector<std::uint8_t> body;
        // 本体部分の受信中に例外が出た場合も呼び出し側で再接続できるように切断しておく。
        try {
            body = receiveAll(body_size);
        } catch (...) {
            markDisconnected();
            throw;
        }
        frame.insert(frame.end(), body.begin(), body.end());
    }
    return frame;
}

void TcpTransport::ensureConnected() const {
    if (!isConnected()) {
        throw TransportError("Transport is not connected");
    }
}

void TcpTransport::applySocketOptions() {
    ensureConnected();

    SocketHandle socket = impl_->socket;

    // Enable keepalive by default.
    const int enable = 1;
    ::setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE,
#ifdef _WIN32
                 reinterpret_cast<const char*>(&enable),
#else
                 &enable,
#endif
                 sizeof(enable));

    // Disable Nagle to reduce latency.
    ::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY,
#ifdef _WIN32
                 reinterpret_cast<const char*>(&enable),
#else
                 &enable,
#endif
                 sizeof(enable));

    // Apply send timeout.
    if (impl_->send_timeout.count() > 0) {
#ifdef _WIN32
        DWORD timeout_ms = static_cast<DWORD>(impl_->send_timeout.count());
        ::setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,
                     reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
        timeval tv{};
        tv.tv_sec = static_cast<long>(impl_->send_timeout.count() / 1000);
        tv.tv_usec =
            static_cast<long>((impl_->send_timeout.count() % 1000) * 1000);
        ::setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
    }

    // Apply receive timeout.
    if (impl_->recv_timeout.count() > 0) {
#ifdef _WIN32
        DWORD timeout_ms = static_cast<DWORD>(impl_->recv_timeout.count());
        ::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
                     reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
        timeval tv{};
        tv.tv_sec = static_cast<long>(impl_->recv_timeout.count() / 1000);
        tv.tv_usec =
            static_cast<long>((impl_->recv_timeout.count() % 1000) * 1000);
        ::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    }
}

bool TcpTransport::isTimeoutError(int error_code) const {
#ifdef _WIN32
    return error_code == WSAEWOULDBLOCK || error_code == WSAETIMEDOUT;
#else
    return error_code == EWOULDBLOCK || error_code == EAGAIN || error_code == ETIMEDOUT;
#endif
}

void TcpTransport::markDisconnected() noexcept {
    if (!impl_) {
        return;
    }
    if (impl_->socket != kInvalidSocket) {
        // 以降の受信で再利用しないため、即座にソケットを閉じて無効化する。
        closeSocket(impl_->socket);
        impl_->socket = kInvalidSocket;
    }
}

} // namespace cpmcprotocol
