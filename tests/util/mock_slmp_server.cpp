#include "util/mock_slmp_server.hpp"

#include <chrono>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace cpmcprotocol::testutil {

namespace {

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;

void closeSocket(SocketHandle s) {
    if (s != kInvalidSocket) {
        closesocket(s);
    }
}
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;

void closeSocket(SocketHandle s) {
    if (s != kInvalidSocket) {
        ::close(s);
    }
}
#endif

} // namespace

MockSlmpServer::MockSlmpServer() = default;
MockSlmpServer::~MockSlmpServer() { stop(); }

void MockSlmpServer::start(std::uint16_t port, Handler handler) {
    if (running_.exchange(true)) {
        throw std::runtime_error("MockSlmpServer already running");
    }
    port_.store(port, std::memory_order_relaxed);
    thread_ = std::thread(&MockSlmpServer::run, this, port, std::move(handler));
}

void MockSlmpServer::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    wakeListener();
    if (thread_.joinable()) {
        thread_.join();
    }
    port_.store(0, std::memory_order_relaxed);
}

bool MockSlmpServer::isRunning() const { return running_.load(); }

void MockSlmpServer::run(std::uint16_t port, Handler handler) {
#ifdef _WIN32
    WSADATA data{};
    WSAStartup(MAKEWORD(2, 2), &data);
#endif

    SocketHandle listen_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket == kInvalidSocket) {
        running_ = false;
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    int reuse = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR,
#ifdef _WIN32
               reinterpret_cast<const char*>(&reuse),
#else
               &reuse,
#endif
               sizeof(reuse));

    if (::bind(listen_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        closeSocket(listen_socket);
        running_ = false;
        return;
    }

    if (::listen(listen_socket, 1) < 0) {
        closeSocket(listen_socket);
        running_ = false;
        return;
    }

    while (running_.load()) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listen_socket, &readfds);

        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms

        int ready =
#ifdef _WIN32
            ::select(0, &readfds, nullptr, nullptr, &tv);
#else
            ::select(listen_socket + 1, &readfds, nullptr, nullptr, &tv);
#endif

        if (!running_.load()) {
            break;
        }

        if (ready < 0) {
#ifdef _WIN32
            const int err = WSAGetLastError();
            if (err == WSAEINTR) {
                continue;
            }
#else
            if (errno == EINTR) {
                continue;
            }
#endif
            break;
        }

        if (ready == 0) {
            continue; // timeout
        }

        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        SocketHandle client =
            ::accept(listen_socket, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client == kInvalidSocket) {
            continue;
        }

        while (running_.load()) {
            std::vector<std::uint8_t> buffer(2048);
            const int received =
                ::recv(client, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
            if (received <= 0) {
                break;
            }
            buffer.resize(static_cast<std::size_t>(received));
            auto response = handler(buffer);
            if (!response.empty()) {
                ::send(client, reinterpret_cast<const char*>(response.data()),
                        static_cast<int>(response.size()), 0);
            }
        }

        closeSocket(client);
    }

    running_ = false;

    closeSocket(listen_socket);

#ifdef _WIN32
    WSACleanup();
#endif
}

void MockSlmpServer::wakeListener() {
    const auto port = port_.load(std::memory_order_relaxed);
    if (port == 0) {
        return;
    }

    SocketHandle s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == kInvalidSocket) {
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    ::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    closeSocket(s);
}

} // namespace cpmcprotocol::testutil
