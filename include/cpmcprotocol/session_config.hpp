#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "cpmcprotocol/communication_mode.hpp"
#include "cpmcprotocol/device.hpp"

namespace cpmcprotocol {

/// セッション設定
/// PLCとの接続に必要な設定情報を保持する
/// connect()を呼ぶ前に適切な値を設定すること
struct SessionConfig {
    // ========================================
    // ネットワーク設定
    // ========================================

    std::string host;             // PLCのIPアドレスまたはホスト名（例: "192.168.1.10"）
    std::uint16_t port = 0;       // PLCのポート番号（通常5000または5001）

    // ========================================
    // MCプロトコル設定
    // ========================================

    std::uint8_t network = 0;     // ネットワーク番号（0=ローカル、1-239=リモート）
    std::uint8_t pc = 0xFF;       // PC番号（0xFF=直接接続、1-120=局番）
    std::uint16_t module_io = 0x03FF;  // モジュールI/O番号（0x03FF=直接接続）
    std::uint8_t module_station = 0;   // モジュール局番（0=CPUユニット）

    std::uint16_t timeout_250ms = 4;   // タイムアウト（250ms単位、デフォルト4=1秒）

    PlcSeries series = PlcSeries::IQ_R;           // PLCシリーズ
    CommunicationMode mode = CommunicationMode::Binary;  // 通信モード

    // ========================================
    // バリデーションヘルパー
    // ========================================

    /// 設定が妥当かどうかをチェックする
    /// @param error_message エラー時にメッセージを格納するポインタ（オプション）
    /// @return 妥当な場合true、不正な場合false
    bool isValid(std::string* error_message = nullptr) const;

    /// 設定を検証し、不正な場合は例外を投げる
    /// @throws std::invalid_argument 設定が不正な場合
    void validate() const;

    /// すべてのバリデーションエラーをリストで取得する
    /// @return エラーメッセージのリスト（エラーがない場合は空）
    std::vector<std::string> getValidationErrors() const;
};

} // namespace cpmcprotocol
