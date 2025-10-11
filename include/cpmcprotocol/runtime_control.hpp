#pragma once

#include <optional>
#include <string>

namespace cpmcprotocol {

/// ランタイムコマンドの種類
/// PLCの動作状態を遠隔制御するためのコマンド
enum class RuntimeCommandType {
    Run,         // PLCを運転状態にする
    Stop,        // PLCを停止状態にする
    Pause,       // PLCを一時停止状態にする
    Reset,       // PLCをリセットする
    LatchClear,  // ラッチをクリアする
    Unlock,      // PLCのロックを解除する
    Lock         // PLCをロックする（パスワード設定）
};

/// クリアモード（RUNコマンド用）
/// PLCを運転状態にする際に、メモリをクリアするかどうかを指定
enum class ClearMode {
    NoClear = 0,           // クリアしない（通常起動）
    ClearExceptLatch = 1,  // ラッチデバイス以外をクリア
    ClearAll = 2           // ラッチデバイスを含めてすべてクリア
};

/// RUN/PAUSEコマンドのオプション
struct RuntimeRunOption {
    ClearMode clear_mode = ClearMode::NoClear;  // クリアモード
    bool force_exec = false;  // true: エラー状態でも強制実行する
};

/// LOCK/UNLOCKコマンドのオプション
struct RuntimeLockOption {
    std::optional<std::string> password;  // パスワード（4文字、iQ-Rは6-32文字）
};

/// ランタイム制御コマンド
/// PLCの運転状態を制御するためのコマンド情報を保持
struct RuntimeControl {
    RuntimeCommandType type = RuntimeCommandType::Run;  // コマンドの種類
    std::optional<RuntimeRunOption> run_option;   // RUN/PAUSE用オプション
    std::optional<RuntimeLockOption> lock_option; // LOCK/UNLOCK用オプション
};

/// CPU情報
/// PLCのCPU型番とコードを保持
struct CpuInfo {
    std::string cpu_type;  // CPU型番（例: "Q03UDECPU"）
    std::string cpu_code;  // CPUコード（16進数文字列）
};

} // namespace cpmcprotocol
