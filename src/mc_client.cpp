#include "cpmcprotocol/mc_client.hpp"

#include "cpmcprotocol/access_option.hpp"
#include "cpmcprotocol/device.hpp"
#include "cpmcprotocol/runtime_control.hpp"
#include "cpmcprotocol/session_config.hpp"
#include "cpmcprotocol/transport.hpp"
#include "cpmcprotocol/codec/frame_decoder.hpp"
#include "cpmcprotocol/codec/frame_encoder.hpp"
#include "cpmcprotocol/value_codec.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace cpmcprotocol {

namespace {

std::uint16_t secondsToTicks(std::uint16_t seconds) {
    // MC プロトコルのタイムアウト単位は 250ms
    constexpr std::uint16_t kTicksPerSecond = 4;
    return static_cast<std::uint16_t>(std::max<std::uint16_t>(1, seconds * kTicksPerSecond));
}

std::chrono::milliseconds toMilliseconds(std::uint16_t seconds) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(seconds));
}

bool isWordFormat(ValueType type) {
    switch (type) {
        case ValueType::Int16:
        case ValueType::UInt16:
        case ValueType::RawWords:
            return true;
        default:
            return false;
    }
}

bool isDwordFormat(ValueType type) {
    switch (type) {
        case ValueType::Int32:
        case ValueType::UInt32:
        case ValueType::Float32:
            return true;
        default:
            return false;
    }
}

bool isLwordFormat(ValueType type) {
    switch (type) {
        case ValueType::Int64:
        case ValueType::UInt64:
        case ValueType::Float64:
            return true;
        default:
            return false;
    }
}

bool isBitFormat(ValueType type) {
    return type == ValueType::BitArray;
}

std::string hexUpper(std::uint32_t value, std::size_t width) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setw(static_cast<int>(width)) << std::setfill('0') << value;
    return oss.str();
}

void appendWord(std::vector<std::uint8_t>& binary,
                std::string& ascii,
                CommunicationMode mode,
                std::uint16_t value) {
    if (mode == CommunicationMode::Binary) {
        binary.push_back(static_cast<std::uint8_t>(value & 0xFF));
        binary.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    } else {
        ascii += hexUpper(value, 4);
    }
}

void appendByte(std::vector<std::uint8_t>& binary,
                std::string& ascii,
                CommunicationMode mode,
                std::uint8_t value) {
    if (mode == CommunicationMode::Binary) {
        binary.push_back(value);
    } else {
        ascii += hexUpper(value, 2);
    }
}

std::string rtrimSpaces(const std::string& text) {
    std::string result = text;
    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    return result;
}

} // namespace

struct McClient::Impl {
    SessionConfig base_config{};
    AccessOption access{};
    TcpTransport transport;
    codec::FrameEncoder frame_encoder;
    codec::FrameDecoder frame_decoder;
    ValueCodec value_codec;
    bool connected = false;

    SessionConfig makeEffectiveConfig() const {
        SessionConfig cfg = base_config;
        cfg.mode = access.mode;
        cfg.network = access.network;
        cfg.pc = access.pc;
        cfg.module_io = access.module_io;
        cfg.module_station = access.module_station;
        cfg.timeout_250ms = secondsToTicks(access.timeout_seconds);
        return cfg;
    }

    void ensureConnected() const {
        if (!connected || !transport.isConnected()) {
            throw TransportError("Client is not connected");
        }
    }

    std::vector<std::uint8_t> receiveFrame(const SessionConfig& cfg) {
        if (cfg.mode == CommunicationMode::Ascii) {
            return transport.receiveFrame(
                22, [](const std::uint8_t* header, std::size_t) {
                    std::string length(reinterpret_cast<const char*>(header + 14), 4);
                    return static_cast<std::size_t>(std::stoul(length, nullptr, 16));
                });
        }
        return transport.receiveFrame(
            9, [](const std::uint8_t* header, std::size_t) {
                return static_cast<std::size_t>(header[7] | (header[8] << 8));
            });
    }

    void ensureCompletion(std::uint16_t code,
                          const std::vector<std::uint8_t>& diag,
                          CommunicationMode mode) const {
        if (code == 0) {
            return;
        }

        std::ostringstream oss;
        oss << "MC completion error 0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << code;
        if (!diag.empty()) {
            oss << " diag=";
            if (mode == CommunicationMode::Ascii) {
                oss << std::string(diag.begin(), diag.end());
            } else {
                for (auto byte : diag) {
                    oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << ' ';
                }
            }
        }
        throw std::runtime_error(oss.str());
    }
};

McClient::McClient() : impl_(new Impl) {}

McClient::McClient(TcpTransport&& transport,
                   std::unique_ptr<codec::FrameEncoder> encoder,
                   std::unique_ptr<codec::FrameDecoder> decoder)
    : impl_(new Impl) {
    impl_->transport = std::move(transport);
    if (encoder) {
        impl_->frame_encoder = std::move(*encoder);
    }
    if (decoder) {
        impl_->frame_decoder = std::move(*decoder);
    }
}

McClient::~McClient() = default;

void McClient::connect(const SessionConfig& config) {
    impl_->base_config = config;
    impl_->access.mode = config.mode;
    impl_->access.network = config.network;
    impl_->access.pc = config.pc;
    impl_->access.module_io = config.module_io;
    impl_->access.module_station = config.module_station;
    impl_->access.timeout_seconds = std::max<std::uint16_t>(1, config.timeout_250ms / 4);

    impl_->transport.connect(config);
    impl_->transport.setTimeout(toMilliseconds(impl_->access.timeout_seconds),
                                toMilliseconds(impl_->access.timeout_seconds));

    impl_->connected = true;
}

void McClient::disconnect() {
    impl_->transport.disconnect();
    impl_->connected = false;
}

bool McClient::isConnected() const noexcept {
    return impl_->connected && impl_->transport.isConnected();
}

void McClient::setAccessOption(const AccessOption& option) {
    impl_->access = option;
    impl_->transport.setTimeout(toMilliseconds(option.timeout_seconds),
                                toMilliseconds(option.timeout_seconds));
}

std::vector<std::uint16_t> McClient::readWords(const DeviceRange& range) {
    impl_->ensureConnected();

    SessionConfig cfg = impl_->makeEffectiveConfig();
    auto request = impl_->frame_encoder.makeBatchReadRequest(cfg, range);
    impl_->transport.sendAll(request);
    auto frame = impl_->receiveFrame(cfg);
    auto response = impl_->frame_decoder.parseBatchReadResponse(frame);
    impl_->ensureCompletion(response.completion_code, response.diagnostic_data, cfg.mode);

    std::vector<std::uint16_t> words;
    if (cfg.mode == CommunicationMode::Ascii) {
        words = ValueCodec::fromAsciiWords(response.device_data);
    } else {
        words = ValueCodec::fromBinaryBytes(response.device_data);
    }

    if (words.size() < range.length) {
        throw std::runtime_error("Insufficient data size for word read");
    }
    words.resize(range.length);
    return words;
}

std::vector<bool> McClient::readBits(const DeviceRange& range) {
    impl_->ensureConnected();

    SessionConfig cfg = impl_->makeEffectiveConfig();
    auto request = impl_->frame_encoder.makeBatchReadRequest(cfg, range);
    impl_->transport.sendAll(request);
    auto frame = impl_->receiveFrame(cfg);
    auto response = impl_->frame_decoder.parseBatchReadResponse(frame);
    impl_->ensureCompletion(response.completion_code, response.diagnostic_data, cfg.mode);

    std::vector<bool> bits(range.length);
    if (cfg.mode == CommunicationMode::Ascii) {
        if (response.device_data.size() < range.length) {
            throw std::runtime_error("Insufficient ASCII data for bit read");
        }
        for (std::size_t i = 0; i < range.length; ++i) {
            bits[i] = response.device_data[i] == '1';
        }
    } else {
        const auto& packed = response.device_data;
        std::size_t bit_index = 0;
        for (std::size_t i = 0; i < packed.size() && bit_index < range.length; ++i) {
            std::uint8_t byte = packed[i];
            if (bit_index < range.length) {
                bits[bit_index++] = (byte & 0x10) != 0;
            }
            if (bit_index < range.length) {
                bits[bit_index++] = (byte & 0x01) != 0;
            }
        }
    }
    return bits;
}

void McClient::writeWords(const DeviceRange& range, const std::vector<std::uint16_t>& values) {
    impl_->ensureConnected();
    if (values.size() < range.length) {
        throw std::invalid_argument("Insufficient word data for write");
    }

    SessionConfig cfg = impl_->makeEffectiveConfig();
    auto request = impl_->frame_encoder.makeBatchWriteRequest(cfg, range, values);
    impl_->transport.sendAll(request);
    auto frame = impl_->receiveFrame(cfg);
    auto response = impl_->frame_decoder.parseBatchWriteResponse(frame);
    impl_->ensureCompletion(response.completion_code, response.diagnostic_data, cfg.mode);
}

void McClient::writeBits(const DeviceRange& range, const std::vector<bool>& values) {
    impl_->ensureConnected();
    if (values.size() < range.length) {
        throw std::invalid_argument("Insufficient bit data for write");
    }

    std::vector<std::uint16_t> bit_words(values.size());
    std::transform(values.begin(), values.end(), bit_words.begin(),
                   [](bool bit) { return bit ? 1U : 0U; });

    SessionConfig cfg = impl_->makeEffectiveConfig();
    auto request = impl_->frame_encoder.makeBatchWriteRequest(cfg, range, bit_words);
    impl_->transport.sendAll(request);
    auto frame = impl_->receiveFrame(cfg);
    auto response = impl_->frame_decoder.parseBatchWriteResponse(frame);
    impl_->ensureCompletion(response.completion_code, response.diagnostic_data, cfg.mode);
}

std::vector<DeviceValue> McClient::randomRead(const DeviceReadPlan& plan) {
    impl_->ensureConnected();
    RandomDeviceRequest request;
    for (const auto& entry : plan) {
        if (isWordFormat(entry.format.type)) {
            request.word_devices.push_back(entry.address);
        } else if (isDwordFormat(entry.format.type)) {
            request.dword_devices.push_back(entry.address);
        } else if (isLwordFormat(entry.format.type)) {
            request.lword_devices.push_back(entry.address);
        } else if (isBitFormat(entry.format.type)) {
            request.bit_devices.push_back(entry.address);
        } else {
            throw std::invalid_argument("Unsupported format in randomRead plan");
        }
    }

    SessionConfig cfg = impl_->makeEffectiveConfig();
    auto frame_request = impl_->frame_encoder.makeRandomReadRequest(cfg, request);
    impl_->transport.sendAll(frame_request);
    auto frame = impl_->receiveFrame(cfg);
    auto response = impl_->frame_decoder.parseRandomReadResponse(frame);
    impl_->ensureCompletion(response.completion_code, response.diagnostic_data, cfg.mode);

    std::vector<std::uint16_t> words;
    if (cfg.mode == CommunicationMode::Ascii) {
        words = ValueCodec::fromAsciiWords(response.device_data);
    } else {
        words = ValueCodec::fromBinaryBytes(response.device_data);
    }
    return impl_->value_codec.decode(plan, words);
}

void McClient::randomWrite(const DeviceWritePlan& plan) {
    impl_->ensureConnected();

    RandomDeviceRequest request;
    std::vector<std::uint16_t> word_data;
    std::vector<std::uint32_t> dword_data;
    std::vector<std::uint64_t> lword_data;
    std::vector<bool> bit_data;

    for (const auto& entry : plan) {
        DeviceWritePlan single{entry};
        auto encoded = impl_->value_codec.encode(single);
        if (isWordFormat(entry.format.type)) {
            if (encoded.size() != 1) {
                throw std::runtime_error("Unexpected word encoding size");
            }
            request.word_devices.push_back(entry.address);
            word_data.push_back(encoded[0]);
        } else if (isDwordFormat(entry.format.type)) {
            if (encoded.size() != 2) {
                throw std::runtime_error("Unexpected dword encoding size");
            }
            request.dword_devices.push_back(entry.address);
            std::uint32_t value = static_cast<std::uint32_t>(encoded[0]) |
                                  (static_cast<std::uint32_t>(encoded[1]) << 16);
            dword_data.push_back(value);
        } else if (isLwordFormat(entry.format.type)) {
            if (encoded.size() != 4) {
                throw std::runtime_error("Unexpected lword encoding size");
            }
            request.lword_devices.push_back(entry.address);
            std::uint64_t value = static_cast<std::uint64_t>(encoded[0]) |
                                  (static_cast<std::uint64_t>(encoded[1]) << 16) |
                                  (static_cast<std::uint64_t>(encoded[2]) << 32) |
                                  (static_cast<std::uint64_t>(encoded[3]) << 48);
            lword_data.push_back(value);
        } else if (isBitFormat(entry.format.type)) {
            if (!std::holds_alternative<std::vector<bool>>(entry.value)) {
                throw std::invalid_argument("BitArray format requires vector<bool> value");
            }
            const auto& bits = std::get<std::vector<bool>>(entry.value);
            if (bits.size() != 1) {
                throw std::invalid_argument("Random bit write only supports single bit per device");
            }
            request.bit_devices.push_back(entry.address);
            bit_data.push_back(bits[0]);
        } else {
            throw std::invalid_argument("Unsupported format in randomWrite plan");
        }
    }

    SessionConfig cfg = impl_->makeEffectiveConfig();
    auto frame_request = impl_->frame_encoder.makeRandomWriteRequest(cfg, request, word_data, dword_data, lword_data, bit_data);
    impl_->transport.sendAll(frame_request);
    auto frame = impl_->receiveFrame(cfg);
    auto response = impl_->frame_decoder.parseRandomWriteResponse(frame);
    impl_->ensureCompletion(response.completion_code, response.diagnostic_data, cfg.mode);
}

CpuInfo McClient::readCpuType() {
    impl_->ensureConnected();

    SessionConfig cfg = impl_->makeEffectiveConfig();
    auto request = impl_->frame_encoder.makeSimpleCommand(cfg, 0x0101, 0x0000, {}, "");
    impl_->transport.sendAll(request);
    auto frame = impl_->receiveFrame(cfg);
    auto response = impl_->frame_decoder.parseBatchReadResponse(frame);
    impl_->ensureCompletion(response.completion_code, response.diagnostic_data, cfg.mode);

    CpuInfo info{};
    const auto& data = response.device_data;
    if (cfg.mode == CommunicationMode::Binary) {
        if (data.size() < 18) {
            throw std::runtime_error("CPU type response too short");
        }
        std::string type(reinterpret_cast<const char*>(data.data()), 16);
        info.cpu_type = rtrimSpaces(type);
        std::uint16_t code_val = static_cast<std::uint16_t>(data[16] | (data[17] << 8));
        info.cpu_code = hexUpper(code_val, 4);
    } else {
        if (data.size() < 20) {
            throw std::runtime_error("CPU type response too short");
        }
        std::string type(data.begin(), data.begin() + 16);
        info.cpu_type = rtrimSpaces(type);
        info.cpu_code = std::string(data.begin() + 16, data.end());
    }
    return info;
}

void McClient::applyRuntimeControl(const RuntimeControl& command) {
    impl_->ensureConnected();

    SessionConfig cfg = impl_->makeEffectiveConfig();
    auto mode = cfg.mode;
    std::vector<std::uint8_t> payload_binary;
    std::string payload_ascii;

    auto sendCommand = [&](std::uint16_t cmd, std::uint16_t sub) {
        auto frame = impl_->frame_encoder.makeSimpleCommand(cfg, cmd, sub, payload_binary, payload_ascii);
        impl_->transport.sendAll(frame);
        auto resp = impl_->receiveFrame(cfg);
        auto decoded = impl_->frame_decoder.parseBatchWriteResponse(resp);
        impl_->ensureCompletion(decoded.completion_code, decoded.diagnostic_data, cfg.mode);
    };

    switch (command.type) {
        case RuntimeCommandType::Run: {
            RuntimeRunOption opt = command.run_option.value_or(RuntimeRunOption{});
            std::uint16_t mode_value = opt.force_exec ? 0x0003 : 0x0001;
            payload_binary.clear();
            payload_ascii.clear();
            appendWord(payload_binary, payload_ascii, mode, mode_value);
            appendByte(payload_binary, payload_ascii, mode, static_cast<std::uint8_t>(opt.clear_mode));
            appendByte(payload_binary, payload_ascii, mode, 0x00);
            sendCommand(0x1001, 0x0000);
            break;
        }
        case RuntimeCommandType::Stop: {
            payload_binary.clear();
            payload_ascii.clear();
            appendWord(payload_binary, payload_ascii, mode, 0x0001);
            sendCommand(0x1002, 0x0000);
            break;
        }
        case RuntimeCommandType::Pause: {
            RuntimeRunOption opt = command.run_option.value_or(RuntimeRunOption{});
            std::uint16_t mode_value = opt.force_exec ? 0x0003 : 0x0001;
            payload_binary.clear();
            payload_ascii.clear();
            appendWord(payload_binary, payload_ascii, mode, mode_value);
            sendCommand(0x1003, 0x0000);
            break;
        }
        case RuntimeCommandType::LatchClear: {
            payload_binary.clear();
            payload_ascii.clear();
            appendWord(payload_binary, payload_ascii, mode, 0x0001);
            sendCommand(0x1005, 0x0000);
            break;
        }
        case RuntimeCommandType::Reset: {
            payload_binary.clear();
            payload_ascii.clear();
            appendWord(payload_binary, payload_ascii, mode, 0x0001);
            try {
                sendCommand(0x1006, 0x0000);
            } catch (const TransportTimeoutError&) {
                // リセットでは応答が返らず切断される構成もあるため、タイムアウトは許容する。
            }
            break;
        }
        case RuntimeCommandType::Unlock:
        case RuntimeCommandType::Lock: {
            if (!command.lock_option || !command.lock_option->password) {
                throw std::invalid_argument("password is required for lock/unlock");
            }
            std::string password = *command.lock_option->password;
            if (!std::all_of(password.begin(), password.end(), [](unsigned char c) { return c < 0x80; })) {
                throw std::invalid_argument("password must be ASCII");
            }
            if (cfg.series == PlcSeries::IQ_R) {
                if (password.size() < 6 || password.size() > 32) {
                    throw std::invalid_argument("password length must be 6-32 for iQ-R series");
                }
            } else if (password.size() != 4) {
                throw std::invalid_argument("password length must be 4 for non iQ-R series");
            }
            payload_binary.clear();
            payload_ascii.clear();
            appendWord(payload_binary, payload_ascii, mode, static_cast<std::uint16_t>(password.size()));
            if (mode == CommunicationMode::Binary) {
                payload_binary.insert(payload_binary.end(), password.begin(), password.end());
            } else {
                payload_ascii += password;
            }
            std::uint16_t cmd = (command.type == RuntimeCommandType::Unlock) ? 0x1630 : 0x1631;
            sendCommand(cmd, 0x0000);
            break;
        }
    }
}

} // namespace cpmcprotocol
