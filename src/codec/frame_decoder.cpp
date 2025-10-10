#include "cpmcprotocol/codec/frame_decoder.hpp"

// 受信した 3E フレームを解析し、完了コードおよび診断データを抽出する。

#include <stdexcept>
#include <string>

namespace cpmcprotocol::codec {

namespace {

std::uint16_t readLittle16(const std::vector<std::uint8_t>& buffer, std::size_t offset) {
    return static_cast<std::uint16_t>(buffer[offset] | (buffer[offset + 1] << 8));
}

std::uint32_t readHexAscii(const std::vector<std::uint8_t>& buffer, std::size_t offset, std::size_t length) {
    if (offset + length > buffer.size()) {
        throw std::invalid_argument("ASCII slice out of range");
    }
    std::string text(buffer.begin() + offset, buffer.begin() + offset + length);
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 16));
}

bool isAsciiFrame(const std::vector<std::uint8_t>& frame) {
    return frame.size() >= 4 &&
           frame[0] == 'D' && frame[1] == '0' && frame[2] == '0' && frame[3] == '0';
}

// 完了コードと後続データを共通的に取り出すための構造体。
struct FrameData {
    std::uint16_t completion = 0;
    std::vector<std::uint8_t> payload;
};

FrameData parseBinaryFrameData(const std::vector<std::uint8_t>& frame) {
    // バイナリフレームは 3E 仕様の 9 バイトヘッダーを前提とする。
    constexpr std::size_t header_size = 9;
    constexpr std::size_t completion_offset = 9;
    constexpr std::size_t completion_size = 2;

    if (frame.size() < header_size + completion_size) {
        throw std::invalid_argument("Binary frame too short");
    }

    const std::uint16_t subheader = static_cast<std::uint16_t>((frame[0] << 8) | frame[1]);
    if (subheader != 0xD000) {
        throw std::invalid_argument("Unexpected subheader in response frame");
    }

    const std::uint16_t data_length = readLittle16(frame, 7);
    if (data_length < completion_size) {
        throw std::invalid_argument("Binary frame reports shorter data section than completion code");
    }

    if (frame.size() != header_size + data_length) {
        throw std::invalid_argument("Frame size and data length mismatch");
    }

    FrameData fd{};
    fd.completion = readLittle16(frame, completion_offset);

    const std::size_t payload_size = data_length - completion_size;
    if (payload_size > 0) {
        fd.payload.assign(frame.begin() + completion_offset + completion_size,
                          frame.begin() + completion_offset + completion_size + payload_size);
    }

    return fd;
}

FrameData parseAsciiFrameData(const std::vector<std::uint8_t>& frame) {
    // ASCII フレームは 3E ASCII 仕様 (先頭 "D000") を前提に解析する。
    constexpr std::size_t header_size = 18;
    constexpr std::size_t completion_offset = 18;
    constexpr std::size_t completion_size = 4;

    if (frame.size() < header_size + completion_size) {
        throw std::invalid_argument("ASCII frame too short");
    }

    const std::uint32_t data_length = readHexAscii(frame, 14, 4);
    if (data_length < completion_size) {
        throw std::invalid_argument("ASCII frame reports shorter data section than completion code");
    }

    if (frame.size() != header_size + data_length) {
        throw std::invalid_argument("ASCII frame size and data length mismatch");
    }

    FrameData fd{};
    fd.completion = static_cast<std::uint16_t>(readHexAscii(frame, completion_offset, completion_size));

    const std::size_t payload_size = static_cast<std::size_t>(data_length - completion_size);
    if (payload_size > 0) {
        fd.payload.assign(frame.begin() + completion_offset + completion_size,
                          frame.begin() + completion_offset + completion_size + payload_size);
    }

    return fd;
}

} // namespace

FrameDecoder::FrameDecoder() = default;

BatchReadResponse FrameDecoder::parseBatchReadResponse(const std::vector<std::uint8_t>& frame) const {
    FrameData data = isAsciiFrame(frame) ? parseAsciiFrameData(frame) : parseBinaryFrameData(frame);
    BatchReadResponse response{};
    response.completion_code = data.completion;
    if (response.completion_code == 0) {
        response.device_data = std::move(data.payload);
    } else {
        response.diagnostic_data = std::move(data.payload);
    }
    return response;
}

BatchWriteResponse FrameDecoder::parseBatchWriteResponse(const std::vector<std::uint8_t>& frame) const {
    FrameData data = isAsciiFrame(frame) ? parseAsciiFrameData(frame) : parseBinaryFrameData(frame);
    BatchWriteResponse response{};
    response.completion_code = data.completion;
    response.diagnostic_data = std::move(data.payload);
    return response;
}

RandomReadResponse FrameDecoder::parseRandomReadResponse(const std::vector<std::uint8_t>& frame) const {
    FrameData data = isAsciiFrame(frame) ? parseAsciiFrameData(frame) : parseBinaryFrameData(frame);
    RandomReadResponse response{};
    response.completion_code = data.completion;
    if (response.completion_code == 0) {
        response.device_data = std::move(data.payload);
    } else {
        response.diagnostic_data = std::move(data.payload);
    }
    return response;
}

RandomWriteResponse FrameDecoder::parseRandomWriteResponse(const std::vector<std::uint8_t>& frame) const {
    FrameData data = isAsciiFrame(frame) ? parseAsciiFrameData(frame) : parseBinaryFrameData(frame);
    RandomWriteResponse response{};
    response.completion_code = data.completion;
    response.diagnostic_data = std::move(data.payload);
    return response;
}

} // namespace cpmcprotocol::codec
