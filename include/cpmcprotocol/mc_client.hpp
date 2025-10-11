#pragma once

#include "cpmcprotocol/device.hpp"
#include "cpmcprotocol/value_codec.hpp"

#include <memory>
#include <string>
#include <vector>

namespace cpmcprotocol {

// 前方宣言
struct SessionConfig;
struct AccessOption;
struct RuntimeControl;
struct CpuInfo;

class TcpTransport;

namespace codec {
class FrameEncoder;
class FrameDecoder;
}

/// MCプロトコルクライアント
/// 三菱電機製PLCとの通信を行うメインクラス
/// MCプロトコル（3Eフレーム）を使用してPLCとデータをやり取りする
///
/// 使用例:
/// @code
/// SessionConfig config;
/// config.host = "192.168.1.10";
/// config.port = 5000;
///
/// McClient client;
/// client.connect(config);
///
/// // ワードデバイスの読み取り
/// auto values = client.readWords(makeDeviceRange("D100", 10));
///
/// // ビットデバイスの書き込み
/// client.writeBits(makeDeviceRange("M0", 5), {true, false, true, false, true});
///
/// client.disconnect();
/// @endcode
class McClient {
public:
    /// デフォルトコンストラクタ
    /// 内部コンポーネント（トランスポート、エンコーダー、デコーダー）を自動生成
    McClient();

    /// デストラクタ
    ~McClient();

    /// 依存性注入用コンストラクタ（テスト用）
    /// トランスポート層とコーデック層をカスタマイズする際に使用
    /// @param transport TCP通信を行うトランスポートオブジェクト（移動）
    /// @param encoder フレームエンコーダー（nullptrの場合はデフォルト使用）
    /// @param decoder フレームデコーダー（nullptrの場合はデフォルト使用）
    /// @note テストでモックオブジェクトを注入する際に使用
    McClient(TcpTransport&& transport,
             std::unique_ptr<codec::FrameEncoder> encoder,
             std::unique_ptr<codec::FrameDecoder> decoder);

    // ========================================
    // 接続管理
    // ========================================

    /// PLCに接続する
    /// @param config 接続設定（ホスト、ポート、タイムアウト等）
    /// @throws TransportError 接続に失敗した場合
    void connect(const SessionConfig& config);

    /// PLCとの接続を切断する
    void disconnect();

    /// 接続状態を確認する
    /// @return 接続中の場合true、切断中の場合false
    bool isConnected() const noexcept;

    /// アクセスオプションを設定する
    /// 接続後に通信パラメータを動的に変更する際に使用
    /// @param option アクセスオプション（タイムアウト、通信モード等）
    void setAccessOption(const AccessOption& option);

    // ========================================
    // バッチアクセス（連続デバイスの読み書き）
    // ========================================

    /// ワードデバイスを連続読み取りする
    /// @param range 読み取り範囲（先頭デバイスと個数）
    /// @return 読み取った値のリスト（16bit符号なし整数）
    /// @throws std::runtime_error 通信エラーまたはPLCエラーの場合
    std::vector<std::uint16_t> readWords(const DeviceRange& range);

    /// ビットデバイスを連続読み取りする
    /// @param range 読み取り範囲（先頭デバイスと個数）
    /// @return 読み取った値のリスト（bool）
    /// @throws std::runtime_error 通信エラーまたはPLCエラーの場合
    std::vector<bool> readBits(const DeviceRange& range);

    /// ワードデバイスに連続書き込みする
    /// @param range 書き込み範囲（先頭デバイスと個数）
    /// @param values 書き込む値のリスト（16bit符号なし整数）
    /// @throws std::invalid_argument 値の個数が範囲に対して不足している場合
    /// @throws std::runtime_error 通信エラーまたはPLCエラーの場合
    void writeWords(const DeviceRange& range, const std::vector<std::uint16_t>& values);

    /// ビットデバイスに連続書き込みする
    /// @param range 書き込み範囲（先頭デバイスと個数）
    /// @param values 書き込む値のリスト（bool）
    /// @throws std::invalid_argument 値の個数が範囲に対して不足している場合
    /// @throws std::runtime_error 通信エラーまたはPLCエラーの場合
    void writeBits(const DeviceRange& range, const std::vector<bool>& values);

    // ========================================
    // ランダムアクセス（非連続デバイスの読み書き）
    // ========================================

    /// 複数の非連続デバイスを一度に読み取る
    /// @param plan 読み取りプラン（各デバイスのアドレスとフォーマット）
    /// @return 読み取った値のリスト（型はDeviceValue）
    /// @throws std::invalid_argument プランのフォーマットが不正な場合
    /// @throws std::runtime_error 通信エラーまたはPLCエラーの場合
    std::vector<DeviceValue> randomRead(const DeviceReadPlan& plan);

    /// 複数の非連続デバイスに一度に書き込む
    /// @param plan 書き込みプラン（各デバイスのアドレス、フォーマット、値）
    /// @throws std::invalid_argument プランのフォーマットまたは値が不正な場合
    /// @throws std::runtime_error 通信エラーまたはPLCエラーの場合
    void randomWrite(const DeviceWritePlan& plan);

    // ========================================
    // ランタイム制御
    // ========================================

    /// PLCのCPU情報を読み取る
    /// @return CPU情報（型番とコード）
    /// @throws std::runtime_error 通信エラーまたはPLCエラーの場合
    CpuInfo readCpuType();

    /// PLCにランタイム制御コマンドを送信する
    /// RUN/STOP/PAUSE/RESET/LOCK/UNLOCK等の制御を行う
    /// @param command ランタイム制御コマンド
    /// @throws std::invalid_argument コマンドパラメータが不正な場合
    /// @throws std::runtime_error 通信エラーまたはPLCエラーの場合
    void applyRuntimeControl(const RuntimeControl& command);

private:
    // Pimplイディオムで実装の詳細を隠蔽
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cpmcprotocol
