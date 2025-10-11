#pragma once

#include "cpmcprotocol/device.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace cpmcprotocol {

/// 値の型
/// PLCデバイスから読み取った/書き込むデータの型を指定する
enum class ValueType {
    Int16,       // 16bit符号付き整数
    UInt16,      // 16bit符号なし整数
    Int32,       // 32bit符号付き整数
    UInt32,      // 32bit符号なし整数
    Float32,     // 32bit浮動小数点数
    Float64,     // 64bit浮動小数点数
    Int64,       // 64bit符号付き整数
    UInt64,      // 64bit符号なし整数
    AsciiString, // ASCII文字列
    RawWords,    // 生のワードデータ（解釈なし）
    BitArray     // ビット配列
};

/// 値のフォーマット
/// データ型と、型に応じた追加パラメータ（長さなど）を保持
struct ValueFormat {
    ValueType type;          // データ型
    std::size_t parameter = 0;  // 型に応じたパラメータ
                                // - AsciiString: 文字列長
                                // - RawWords: ワード数
                                // - BitArray: ビット数

    // ========================================
    // ファクトリメソッド（値フォーマットの作成）
    // ========================================

    static ValueFormat Int16() { return {ValueType::Int16, 0}; }
    static ValueFormat UInt16() { return {ValueType::UInt16, 0}; }
    static ValueFormat Int32() { return {ValueType::Int32, 0}; }
    static ValueFormat UInt32() { return {ValueType::UInt32, 0}; }
    static ValueFormat Float32() { return {ValueType::Float32, 0}; }
    static ValueFormat Float64() { return {ValueType::Float64, 0}; }
    static ValueFormat Int64() { return {ValueType::Int64, 0}; }
    static ValueFormat UInt64() { return {ValueType::UInt64, 0}; }
    static ValueFormat AsciiString(std::size_t length) { return {ValueType::AsciiString, length}; }
    static ValueFormat RawWords(std::size_t words) { return {ValueType::RawWords, words}; }
    static ValueFormat BitArray(std::size_t bit_count) { return {ValueType::BitArray, bit_count}; }
};

/// デバイス値
/// PLCデバイスから読み取った値、または書き込む値を保持する
/// std::variantで複数の型を扱えるようにしている
using DeviceValue = std::variant<int16_t,                    // 16bit整数
                                 uint16_t,                    // 16bit符号なし整数
                                 int32_t,                     // 32bit整数
                                 uint32_t,                    // 32bit符号なし整数
                                 float,                       // 浮動小数点数
                                 double,                      // 倍精度浮動小数点数
                                 int64_t,                     // 64bit整数
                                 uint64_t,                    // 64bit符号なし整数
                                 std::string,                 // 文字列
                                 std::vector<std::uint16_t>,  // ワード配列
                                 std::vector<bool>>;          // ビット配列

/// デバイス読み取りプランのエントリ
/// どのデバイスをどのフォーマットで読み取るかを指定
struct DeviceReadPlanEntry {
    DeviceAddress address;  // 読み取り対象のデバイスアドレス
    ValueFormat format;     // 読み取るデータのフォーマット
};

/// デバイス読み取りプラン
/// 複数のデバイスを一度に読み取る際の計画
using DeviceReadPlan = std::vector<DeviceReadPlanEntry>;

/// デバイス書き込みプランのエントリ
/// どのデバイスにどの値を書き込むかを指定
struct DeviceWritePlanEntry {
    DeviceAddress address;  // 書き込み対象のデバイスアドレス
    ValueFormat format;     // 書き込むデータのフォーマット
    DeviceValue value;      // 書き込む値
};

/// デバイス書き込みプラン
/// 複数のデバイスを一度に書き込む際の計画
using DeviceWritePlan = std::vector<DeviceWritePlanEntry>;

/// 値のエンコード/デコードを行うクラス
/// PLCプロトコルのワードデータと、C++の型の間で変換を行う
class ValueCodec {
public:
    /// ワードデータをデコードして値のリストに変換する
    /// @param plan 読み取りプラン（各デバイスのフォーマット指定）
    /// @param words PLCから読み取った生のワードデータ
    /// @return デコードされた値のリスト
    /// @throws std::invalid_argument データサイズが不足している場合
    std::vector<DeviceValue> decode(const DeviceReadPlan& plan, const std::vector<std::uint16_t>& words) const;

    /// 値のリストをエンコードしてワードデータに変換する
    /// @param plan 書き込みプラン（各デバイスのフォーマットと値）
    /// @return エンコードされたワードデータ
    /// @throws std::invalid_argument 値の型がフォーマットと一致しない場合
    std::vector<std::uint16_t> encode(const DeviceWritePlan& plan) const;

    /// 指定されたフォーマットに必要なワード数を計算する
    /// @param format 値のフォーマット
    /// @return 必要なワード数
    /// @throws std::invalid_argument 不正なフォーマットの場合
    static std::size_t requiredWords(const ValueFormat& format);

    // ========================================
    // Binary/ASCII変換ヘルパー
    // ========================================

    /// バイナリバイト列からワード列に変換する
    /// @param bytes バイナリバイト列（リトルエンディアン）
    /// @return ワード列
    static std::vector<std::uint16_t> fromBinaryBytes(const std::vector<std::uint8_t>& bytes);

    /// ASCIIワード表現（16進数文字列）からワード列に変換する
    /// @param ascii ASCII表現のバイト列（4文字=1ワード）
    /// @return ワード列
    static std::vector<std::uint16_t> fromAsciiWords(const std::vector<std::uint8_t>& ascii);

    /// ワード列をバイナリバイト列に変換する
    /// @param words ワード列
    /// @return バイナリバイト列（リトルエンディアン）
    static std::vector<std::uint8_t> toBinaryBytes(const std::vector<std::uint16_t>& words);

    /// ワード列をASCIIワード表現（16進数文字列）に変換する
    /// @param words ワード列
    /// @return ASCII表現のバイト列（4文字=1ワード）
    static std::vector<std::uint8_t> toAsciiWords(const std::vector<std::uint16_t>& words);
};

} // namespace cpmcprotocol
