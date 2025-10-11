#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cpmcprotocol {

/// PLCシリーズの種類
/// MCプロトコルはシリーズによって一部の仕様が異なる
enum class PlcSeries {
    Q,      // Qシリーズ
    L,      // Lシリーズ
    QnA,    // QnAシリーズ
    IQ_L,   // iQ-Lシリーズ
    IQ_R    // iQ-Rシリーズ（最新世代、デフォルト推奨）
};

/// デバイスのデータ型
/// PLCのデバイスはビット単位かワード単位でアクセスする
enum class DeviceType {
    Word,       // ワードデバイス（16bit単位）: D, W, R等
    Bit,        // ビットデバイス（1bit単位）: X, Y, M, L等
    DoubleWord  // ダブルワードデバイス（32bit単位）
};

/// デバイスアドレスの表現
/// PLCの特定のデバイス（メモリ位置）を指定する
struct DeviceAddress {
    std::string name;        // デバイス名（例: "D1000", "X10", "M100"）
    DeviceType type = DeviceType::Word;  // デバイスの型
};

/// デバイス範囲の表現
/// 連続した複数のデバイスを一度に読み書きする際に使用
struct DeviceRange {
    DeviceAddress head;      // 範囲の先頭デバイスアドレス
    std::uint16_t length = 0;  // 読み書きする個数（ワード数またはビット数）
};

/// ランダムアクセス要求
/// 非連続な複数のデバイスを一度のリクエストで読み書きする際に使用
/// 各データ型ごとにデバイスのリストを保持する
struct RandomDeviceRequest {
    std::vector<DeviceAddress> word_devices;   // 16bitワードデバイスのリスト
    std::vector<DeviceAddress> dword_devices;  // 32bitダブルワードデバイスのリスト
    std::vector<DeviceAddress> lword_devices;  // 64bitロングワードデバイスのリスト
    std::vector<DeviceAddress> bit_devices;    // ビットデバイスのリスト
};

// ========================================
// デバイスカタログユーティリティ関数
// ========================================

/// デバイス名からデバイス型を自動判定する
/// @param device_name デバイス名（例: "D100", "X10"）
/// @return デバイス型（Word, Bit, DoubleWord）
DeviceType getDeviceType(const std::string& device_name);

/// デバイス名が妥当かどうかを検証する
/// @param device_name 検証するデバイス名
/// @param error_message エラー時にメッセージを格納するポインタ（オプション）
/// @return 妥当な場合true、不正な場合false
bool isValidDeviceName(const std::string& device_name, std::string* error_message = nullptr);

/// デバイス名を正規化する（大文字化）
/// @param device_name 正規化するデバイス名
/// @return 正規化されたデバイス名（大文字）
std::string normalizeDeviceName(const std::string& device_name);

/// デバイス名からDeviceAddressを作成する
/// @param device_name デバイス名（自動で正規化・型判定される）
/// @return 作成されたDeviceAddress
/// @throws std::invalid_argument デバイス名が不正な場合
DeviceAddress makeDeviceAddress(const std::string& device_name);

/// デバイス名と長さからDeviceRangeを作成する
/// @param device_name 先頭デバイス名
/// @param length 範囲の長さ
/// @return 作成されたDeviceRange
/// @throws std::invalid_argument デバイス名が不正または長さが0の場合
DeviceRange makeDeviceRange(const std::string& device_name, std::uint16_t length);

} // namespace cpmcprotocol
