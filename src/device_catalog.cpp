#include "cpmcprotocol/device.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace cpmcprotocol {

namespace {

// デバイス名からプレフィックスを抽出
std::string extractDevicePrefix(const std::string& device_name) {
    auto pos = device_name.find_first_of("0123456789");
    if (pos == std::string::npos) {
        return device_name;
    }
    return device_name.substr(0, pos);
}

// デバイス名からデバイス番号を抽出（数値部分）
bool hasDeviceNumber(const std::string& device_name) {
    return device_name.find_first_of("0123456789") != std::string::npos;
}

} // namespace

// デバイスタイプの判定ヘルパー
DeviceType getDeviceType(const std::string& device_name) {
    std::string prefix = extractDevicePrefix(device_name);

    // ビットデバイス
    if (prefix == "X" || prefix == "Y" || prefix == "M" ||
        prefix == "L" || prefix == "F" || prefix == "B" ||
        prefix == "T" || prefix == "C") {
        return DeviceType::Bit;
    }

    // ワードデバイス
    if (prefix == "D" || prefix == "W" || prefix == "R" ||
        prefix == "Z" || prefix == "ZR" || prefix == "RD") {
        return DeviceType::Word;
    }

    // 不明な場合はWordとして扱う（後でエラーになる）
    return DeviceType::Word;
}

// デバイス名の検証
bool isValidDeviceName(const std::string& device_name, std::string* error_message) {
    if (device_name.empty()) {
        if (error_message) {
            *error_message = "Device name is empty";
        }
        return false;
    }

    // プレフィックスの抽出
    std::string prefix = extractDevicePrefix(device_name);
    if (prefix.empty()) {
        if (error_message) {
            *error_message = "Device name has no prefix";
        }
        return false;
    }

    // 既知のデバイスプレフィックスかチェック
    static const char* known_prefixes[] = {
        "X", "Y", "M", "L", "F", "B", "T", "C",  // Bit devices
        "D", "W", "R", "Z", "ZR", "RD"           // Word devices
    };

    bool found = false;
    for (const auto* known : known_prefixes) {
        if (prefix == known) {
            found = true;
            break;
        }
    }

    if (!found) {
        if (error_message) {
            *error_message = "Unknown device prefix: " + prefix;
        }
        return false;
    }

    // 数値部分の検証
    if (!hasDeviceNumber(device_name)) {
        if (error_message) {
            *error_message = "Device name missing numeric part: " + device_name;
        }
        return false;
    }

    return true;
}

// デバイス名の正規化（大文字化）
std::string normalizeDeviceName(const std::string& device_name) {
    std::string normalized = device_name;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return normalized;
}

// デバイスアドレスの作成ヘルパー
DeviceAddress makeDeviceAddress(const std::string& device_name) {
    std::string normalized = normalizeDeviceName(device_name);

    std::string error;
    if (!isValidDeviceName(normalized, &error)) {
        throw std::invalid_argument("Invalid device name: " + error);
    }

    DeviceAddress addr;
    addr.name = normalized;
    addr.type = getDeviceType(normalized);
    return addr;
}

// DeviceRangeの作成ヘルパー
DeviceRange makeDeviceRange(const std::string& device_name, std::uint16_t length) {
    if (length == 0) {
        throw std::invalid_argument("Device range length must be greater than 0");
    }

    DeviceRange range;
    range.head = makeDeviceAddress(device_name);
    range.length = length;
    return range;
}

} // namespace cpmcprotocol
