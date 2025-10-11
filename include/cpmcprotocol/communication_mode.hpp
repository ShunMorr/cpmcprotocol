#pragma once

namespace cpmcprotocol {

/// MCプロトコルの通信モード
/// PLCとの通信でバイナリ形式かASCII形式を選択できる
enum class CommunicationMode {
    Binary,  // バイナリモード: データをバイナリ形式で送受信（推奨、効率的）
    Ascii    // ASCIIモード: データをASCII文字列形式で送受信（可読性高い、デバッグ用）
};

} // namespace cpmcprotocol
