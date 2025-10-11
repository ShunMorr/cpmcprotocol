#pragma once

#include <cstdint>

#include "cpmcprotocol/communication_mode.hpp"

namespace cpmcprotocol {

/// アクセスオプション
/// 接続後に通信パラメータを動的に変更する際に使用
/// setAccessOption()で設定を上書きできる
struct AccessOption {
    CommunicationMode mode = CommunicationMode::Binary;  // 通信モード
    std::uint8_t network = 0;         // ネットワーク番号
    std::uint8_t pc = 0xFF;           // PC番号
    std::uint16_t module_io = 0x03FF; // モジュールI/O番号
    std::uint8_t module_station = 0;  // モジュール局番
    std::uint16_t timeout_seconds = 1; // タイムアウト（秒単位）
                                       // 注: McClient::setAccessOption()内で250ms単位に変換される
};

} // namespace cpmcprotocol
