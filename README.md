# cpmcprotocol

三菱電機製PLC（プログラマブル・ロジック・コントローラー）との通信を行うための、MCプロトコル（MELSEC Communication Protocol）C++17実装ライブラリです。

## 目次

- [対応環境](#対応環境)
- [ビルド方法](#ビルド方法)
- [クイックスタート](#クイックスタート)
- [API詳細](#api詳細)
  - [接続管理](#接続管理)
  - [バッチアクセス](#バッチアクセス)
  - [ランダムアクセス](#ランダムアクセス)
  - [ランタイム制御](#ランタイム制御)
- [応用例](#応用例)
- [エラーハンドリング](#エラーハンドリング)
- [テスト](#テスト)

## 対応環境

- **C++標準**: C++17以上
- **ビルドシステム**: CMake 3.15以上
- **対応OS**: Linux、Windows（POSIX/Winsockソケット対応）
- **対応PLCシリーズ**: iQ-R、Q、L、QnA、iQ-L（3Eフレーム対応機種）

## ビルド方法

### ライブラリのビルド

```bash
# リポジトリのクローン
git clone https://github.com/your-repo/cpmcprotocol.git
cd cpmcprotocol

# ビルドディレクトリの作成
mkdir build && cd build

# CMake設定とビルド
cmake ..
cmake --build .

# テストの実行（オプション）
ctest --output-on-failure
```

### システムへのインストール

```bash
# ビルド後、システムにインストール
sudo cmake --install build

# またはインストール先を指定
cmake --install build --prefix /path/to/install
```

### 他のプロジェクトでの使用

インストール後、他のCMakeプロジェクトから以下のように利用できます：

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.18)
project(your_app)

set(CMAKE_CXX_STANDARD 17)

# cpmcprotocolパッケージを検索
find_package(cpmcprotocol REQUIRED)

add_executable(your_app main.cpp)

# cpmcprotocolライブラリをリンク
target_link_libraries(your_app PRIVATE cpmcprotocol::cpmcprotocol)
```

**main.cpp:**
```cpp
#include <cpmcprotocol/mc_client.hpp>
#include <cpmcprotocol/session_config.hpp>

int main() {
    cpmcprotocol::McClient client;
    // ... あなたのコード ...
    return 0;
}
```

標準以外の場所にインストールした場合は、ビルド時に`CMAKE_PREFIX_PATH`を指定します：

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/install
cmake --build build
```

## クイックスタート

以下は、PLCに接続してデータレジスタ（Dデバイス）を読み書きする基本的な例です：

```cpp
#include <cpmcprotocol/mc_client.hpp>
#include <cpmcprotocol/device.hpp>
#include <iostream>

using namespace cpmcprotocol;

int main() {
    try {
        // 1. 接続設定の構成
        SessionConfig config;
        config.host = "192.168.1.10";        // PLCのIPアドレス
        config.port = 5000;                   // 通常5000または5001
        config.network = 0;                   // ネットワーク番号
        config.pc = 0xFF;                     // PC番号
        config.dest_moduleio = 0x03FF;        // モジュールI/O番号
        config.dest_modulesta = 0;            // モジュールステーション番号
        config.timeout_ms = 5000;             // タイムアウト（ミリ秒）
        config.plc_series = PlcSeries::iQR;   // PLCシリーズ
        config.comm_format = CommFormat::Binary; // 通信フォーマット

        // 2. クライアントの作成と接続
        McClient client;
        client.connect(config);
        std::cout << "PLCに接続しました" << std::endl;

        // 3. デバイスの読み取り（D100から10ワード）
        DeviceRange range = makeDeviceRange("D100", 10);
        auto values = client.readWords(range);

        std::cout << "読み取った値: ";
        for (auto val : values) {
            std::cout << val << " ";
        }
        std::cout << std::endl;

        // 4. デバイスへの書き込み
        std::vector<std::uint16_t> write_data = {100, 200, 300};
        client.writeWords(makeDeviceRange("D200", 3), write_data);
        std::cout << "D200-D202に値を書き込みました" << std::endl;

        // 5. 切断
        client.disconnect();
        std::cout << "PLCから切断しました" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "エラー: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

## API詳細

### 接続管理

#### SessionConfig - 接続設定

```cpp
SessionConfig config;
config.host = "192.168.1.10";           // PLCのIPアドレスまたはホスト名（必須）
config.port = 5000;                      // ポート番号（必須、通常5000または5001）
config.network = 0;                      // ネットワーク番号（0-255）
config.pc = 0xFF;                        // PC番号（0-255、0xFFはローカル）
config.dest_moduleio = 0x03FF;           // モジュールI/O番号（0x0000-0xFFFF）
config.dest_modulesta = 0;               // モジュールステーション番号（0-255）
config.timeout_ms = 5000;                // タイムアウト（ミリ秒、0=無制限）
config.plc_series = PlcSeries::iQR;      // PLCシリーズ（iQR/Q/L/QnA/iQL）
config.comm_format = CommFormat::Binary; // 通信フォーマット（Binary/ASCII）

// 設定の検証
if (!config.isValid()) {
    auto errors = config.getValidationErrors();
    for (const auto& error : errors) {
        std::cerr << "設定エラー: " << error << std::endl;
    }
}

// または例外による検証
config.validate(); // 不正な場合は std::invalid_argument をスロー
```

#### connect() - PLC への接続

```cpp
McClient client;
client.connect(config);
// 接続に失敗した場合は std::runtime_error がスローされます
```

#### disconnect() - PLC からの切断

```cpp
client.disconnect();
// 接続が確立されていない場合でも安全に呼び出せます
```

#### setAccessOption() - アクセスオプションの設定

```cpp
AccessOption option;
option.timeout_seconds = 10;  // コマンドタイムアウト（秒単位、0=無制限）

client.setAccessOption(option);
// このメソッドは接続後に呼び出す必要があります
```

### バッチアクセス

バッチアクセスは、連続したデバイスアドレスの読み書きに使用します。

#### readWords() - ワードデバイスの連続読み取り

```cpp
// D100から10ワード読み取り
auto values = client.readWords(makeDeviceRange("D100", 10));

// その他のワードデバイス例
auto data_regs = client.readWords(makeDeviceRange("D0", 100));    // Dデバイス
auto timers = client.readWords(makeDeviceRange("T0", 50));        // タイマ現在値
auto counters = client.readWords(makeDeviceRange("C0", 50));      // カウンタ現在値
auto registers = client.readWords(makeDeviceRange("R0", 100));    // Rデバイス
auto zr_regs = client.readWords(makeDeviceRange("ZR0", 100));     // ZRデバイス

// 戻り値: std::vector<std::uint16_t>（各要素は0-65535）
```

#### readBits() - ビットデバイスの連続読み取り

```cpp
// X100から16ビット読み取り
auto bits = client.readBits(makeDeviceRange("X100", 16));

// その他のビットデバイス例
auto inputs = client.readBits(makeDeviceRange("X0", 64));     // 入力リレー
auto outputs = client.readBits(makeDeviceRange("Y0", 64));    // 出力リレー
auto internals = client.readBits(makeDeviceRange("M0", 128)); // 内部リレー
auto latches = client.readBits(makeDeviceRange("L0", 64));    // ラッチリレー
auto links = client.readBits(makeDeviceRange("B0", 32));      // リンクリレー

// 戻り値: std::vector<bool>（true=ON、false=OFF）
```

#### writeWords() - ワードデバイスへの連続書き込み

```cpp
std::vector<std::uint16_t> data = {100, 200, 300, 400, 500};
client.writeWords(makeDeviceRange("D100", 5), data);

// データサイズがデバイス数と一致しない場合は std::invalid_argument がスローされます
```

#### writeBits() - ビットデバイスへの連続書き込み

```cpp
std::vector<bool> bits = {true, false, true, true, false, false, true, false};
client.writeBits(makeDeviceRange("M100", 8), bits);

// データサイズがデバイス数と一致しない場合は std::invalid_argument がスローされます
```

### ランダムアクセス

ランダムアクセスは、非連続なデバイスアドレスを1つのリクエストで読み書きする機能です。異なるデータ型を混在させることができます。

#### randomRead() - ランダムデバイス読み取り

ランダム読み取りでは、`DeviceReadPlan`を使用してデバイスアドレスと期待するデータ型を指定します。

```cpp
// 読み取りプランの作成
DeviceReadPlan plan{
    // {デバイスアドレス, データフォーマット}
    {makeDeviceAddress("D100"), ValueFormat::Int16()},      // D100を符号付き16bit整数として
    {makeDeviceAddress("D200"), ValueFormat::UInt32()},     // D200-D201を符号なし32bit整数として
    {makeDeviceAddress("D300"), ValueFormat::Float32()},    // D300-D301を単精度浮動小数点として
    {makeDeviceAddress("D400"), ValueFormat::Int64()},      // D400-D403を符号付き64bit整数として
    {makeDeviceAddress("X100"), ValueFormat::BitArray(1)},  // X100を1ビットとして
};

// ランダム読み取りの実行
auto values = client.randomRead(plan);

// 値の取り出し（std::variantから型を取得）
int16_t val1 = std::get<int16_t>(values[0]);
uint32_t val2 = std::get<uint32_t>(values[1]);
float val3 = std::get<float>(values[2]);
int64_t val4 = std::get<int64_t>(values[3]);
std::vector<bool> bits = std::get<std::vector<bool>>(values[4]);

// 戻り値: std::vector<DeviceValue>
// DeviceValue は std::variant<int8_t, int16_t, int32_t, int64_t,
//                              uint8_t, uint16_t, uint32_t, uint64_t,
//                              float, double, std::vector<bool>>
```

##### サポートされるデータフォーマット

| フォーマット | サイズ | 説明 | 使用例 |
|------------|--------|------|--------|
| `Int8()` | 1ワード | 符号付き8bit整数 (-128～127) | `ValueFormat::Int8()` |
| `Int16()` | 1ワード | 符号付き16bit整数 (-32768～32767) | `ValueFormat::Int16()` |
| `Int32()` | 2ワード | 符号付き32bit整数 | `ValueFormat::Int32()` |
| `Int64()` | 4ワード | 符号付き64bit整数 | `ValueFormat::Int64()` |
| `UInt8()` | 1ワード | 符号なし8bit整数 (0～255) | `ValueFormat::UInt8()` |
| `UInt16()` | 1ワード | 符号なし16bit整数 (0～65535) | `ValueFormat::UInt16()` |
| `UInt32()` | 2ワード | 符号なし32bit整数 | `ValueFormat::UInt32()` |
| `UInt64()` | 4ワード | 符号なし64bit整数 | `ValueFormat::UInt64()` |
| `Float32()` | 2ワード | 単精度浮動小数点（IEEE754） | `ValueFormat::Float32()` |
| `Float64()` | 4ワード | 倍精度浮動小数点（IEEE754） | `ValueFormat::Float64()` |
| `BitArray(n)` | nビット | ビット配列（n個のbool値） | `ValueFormat::BitArray(16)` |

#### randomWrite() - ランダムデバイス書き込み

ランダム書き込みでは、`DeviceWritePlan`を使用してデバイスアドレスと書き込む値を指定します。

```cpp
// 書き込みプランの作成
DeviceWritePlan plan{
    // {デバイスアドレス, 書き込む値}
    {makeDeviceAddress("D100"), static_cast<int16_t>(-1234)},
    {makeDeviceAddress("D200"), static_cast<uint32_t>(1000000)},
    {makeDeviceAddress("D300"), 3.14159f},
    {makeDeviceAddress("D400"), static_cast<int64_t>(9876543210LL)},
};

// ランダム書き込みの実行
client.randomWrite(plan);
```

### ランタイム制御

PLCのランタイム状態をリモートから制御するためのAPIです。

#### readCpuType() - CPU情報の読み取り

```cpp
auto cpu_info = client.readCpuType();
std::cout << "CPUタイプ: " << cpu_info.cpu_type << std::endl;
std::cout << "CPUコード: " << std::hex << cpu_info.cpu_code << std::endl;
```

#### applyRuntimeControl() - ランタイム制御コマンドの実行

```cpp
RuntimeControl control;

// 1. RUN（運転開始）
control.type = RuntimeControlType::Run;
control.run_option.clear_mode = ClearMode::NoClear;        // クリアしない
control.run_option.force_exec = false;                     // エラー時は実行しない
client.applyRuntimeControl(control);

// 2. STOP（運転停止）
control.type = RuntimeControlType::Stop;
client.applyRuntimeControl(control);

// 3. PAUSE（一時停止）
control.type = RuntimeControlType::Pause;
client.applyRuntimeControl(control);

// 4. RESET（リセット）
control.type = RuntimeControlType::Reset;
client.applyRuntimeControl(control);

// 5. ラッチクリア
control.type = RuntimeControlType::LatchClear;
client.applyRuntimeControl(control);
```

##### ClearMode - 運転開始時のクリアモード

| 値 | 説明 |
|----|------|
| `ClearMode::NoClear` | クリアしない（通常起動） |
| `ClearMode::ClearExceptLatch` | ラッチデバイス以外をクリア |
| `ClearMode::ClearAll` | ラッチデバイスを含めてすべてクリア |

```cpp
// 例：ラッチデバイス以外をクリアして運転開始
RuntimeControl control;
control.type = RuntimeControlType::Run;
control.run_option.clear_mode = ClearMode::ClearExceptLatch;
control.run_option.force_exec = false;
client.applyRuntimeControl(control);
```

## 応用例

### 例1: センサーデータの周期的監視

```cpp
#include <cpmcprotocol/mc_client.hpp>
#include <iostream>
#include <chrono>
#include <thread>

using namespace cpmcprotocol;

void monitorSensors(McClient& client) {
    DeviceReadPlan plan{
        {makeDeviceAddress("D100"), ValueFormat::Float32()},  // 温度センサー
        {makeDeviceAddress("D102"), ValueFormat::Float32()},  // 圧力センサー
        {makeDeviceAddress("D104"), ValueFormat::UInt16()},   // 状態フラグ
    };

    while (true) {
        try {
            auto values = client.randomRead(plan);

            float temperature = std::get<float>(values[0]);
            float pressure = std::get<float>(values[1]);
            uint16_t status = std::get<uint16_t>(values[2]);

            std::cout << "温度: " << temperature << "℃, "
                      << "圧力: " << pressure << "kPa, "
                      << "状態: 0x" << std::hex << status << std::endl;

            // アラーム判定
            if (temperature > 80.0f) {
                std::cout << "警告: 温度が高すぎます！" << std::endl;
            }
            if (pressure > 500.0f) {
                std::cout << "警告: 圧力が高すぎます！" << std::endl;
            }

        } catch (const std::exception& e) {
            std::cerr << "エラー: " << e.what() << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
```

### 例2: レシピデータの一括設定

```cpp
#include <cpmcprotocol/mc_client.hpp>
#include <vector>

using namespace cpmcprotocol;

struct Recipe {
    int16_t step_count;
    float target_temperature;
    float target_pressure;
    uint32_t duration_ms;
};

void uploadRecipe(McClient& client, const Recipe& recipe) {
    DeviceWritePlan plan{
        {makeDeviceAddress("D1000"), recipe.step_count},
        {makeDeviceAddress("D1001"), recipe.target_temperature},
        {makeDeviceAddress("D1003"), recipe.target_pressure},
        {makeDeviceAddress("D1005"), recipe.duration_ms},
    };

    client.randomWrite(plan);
    std::cout << "レシピをアップロードしました" << std::endl;
}

int main() {
    SessionConfig config;
    config.host = "192.168.1.10";
    config.port = 5000;
    // ... その他の設定 ...

    McClient client;
    client.connect(config);

    Recipe recipe{
        .step_count = 5,
        .target_temperature = 75.5f,
        .target_pressure = 250.0f,
        .duration_ms = 60000,
    };

    uploadRecipe(client, recipe);

    client.disconnect();
    return 0;
}
```

### 例3: エラーチェック付きバッチ処理

```cpp
#include <cpmcprotocol/mc_client.hpp>
#include <iostream>

using namespace cpmcprotocol;

bool checkPlcReady(McClient& client) {
    // ステータスワードを読み取り
    auto status = client.readWords(makeDeviceRange("D9000", 1));

    const uint16_t READY_BIT = 0x0001;
    const uint16_t ERROR_BIT = 0x8000;

    if (status[0] & ERROR_BIT) {
        std::cerr << "エラー: PLCがエラー状態です" << std::endl;
        return false;
    }

    if (!(status[0] & READY_BIT)) {
        std::cerr << "警告: PLCが準備完了していません" << std::endl;
        return false;
    }

    return true;
}

void executeBatchProcess(McClient& client) {
    // 1. PLC準備確認
    if (!checkPlcReady(client)) {
        return;
    }

    // 2. バッチ開始フラグを立てる
    client.writeBits(makeDeviceRange("M1000", 1), {true});
    std::cout << "バッチ処理を開始しました" << std::endl;

    // 3. パラメータ設定
    std::vector<uint16_t> params = {100, 200, 300, 400, 500};
    client.writeWords(makeDeviceRange("D2000", 5), params);

    // 4. 完了待ち
    while (true) {
        auto done = client.readBits(makeDeviceRange("M1001", 1));
        if (done[0]) {
            std::cout << "バッチ処理が完了しました" << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 5. バッチ開始フラグをクリア
    client.writeBits(makeDeviceRange("M1000", 1), {false});
}
```

### 例4: リトライロジック付き接続

```cpp
#include <cpmcprotocol/mc_client.hpp>
#include <iostream>
#include <thread>

using namespace cpmcprotocol;

bool connectWithRetry(McClient& client, const SessionConfig& config,
                      int max_retries = 3, int retry_delay_ms = 1000) {
    for (int i = 0; i < max_retries; ++i) {
        try {
            std::cout << "接続試行 " << (i + 1) << "/" << max_retries << "..." << std::endl;
            client.connect(config);
            std::cout << "接続成功" << std::endl;
            return true;

        } catch (const std::exception& e) {
            std::cerr << "接続失敗: " << e.what() << std::endl;

            if (i < max_retries - 1) {
                std::cout << retry_delay_ms << "ms後に再試行します..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
            }
        }
    }

    std::cerr << "最大試行回数に達しました。接続できません。" << std::endl;
    return false;
}

int main() {
    SessionConfig config;
    config.host = "192.168.1.10";
    config.port = 5000;
    config.timeout_ms = 3000;
    // ... その他の設定 ...

    McClient client;

    if (!connectWithRetry(client, config)) {
        return 1;
    }

    // 通常の処理
    // ...

    client.disconnect();
    return 0;
}
```

## エラーハンドリング

すべてのAPIは、エラー時に例外をスローします。主な例外型は以下の通りです：

```cpp
try {
    client.connect(config);
    auto values = client.readWords(makeDeviceRange("D100", 10));

} catch (const std::invalid_argument& e) {
    // 引数が不正な場合（デバイス名の形式エラー、データサイズ不一致など）
    std::cerr << "引数エラー: " << e.what() << std::endl;

} catch (const std::runtime_error& e) {
    // 通信エラー、PLCからのエラー応答など
    std::cerr << "実行時エラー: " << e.what() << std::endl;

} catch (const std::exception& e) {
    // その他のすべての例外
    std::cerr << "予期しないエラー: " << e.what() << std::endl;
}
```

### よくあるエラーと対処法

| エラーメッセージ | 原因 | 対処法 |
|----------------|------|--------|
| `Host address is empty` | SessionConfig.hostが空 | 有効なIPアドレスまたはホスト名を設定 |
| `Port must be non-zero` | SessionConfig.portが0 | 通常は5000または5001を設定 |
| `Connection failed` | PLCに接続できない | ネットワーク接続、IPアドレス、ポート番号を確認 |
| `Invalid device name` | デバイス名の形式が不正 | 正しい形式（例：D100、X10）で指定 |
| `Data size mismatch` | 書き込みデータサイズがデバイス数と不一致 | データ配列のサイズを確認 |
| `Timeout` | PLCからの応答がタイムアウト | timeout_msを増やす、PLCの状態を確認 |

## テスト

プロジェクトには包括的なテストスイートが含まれています：

```bash
cd build

# すべてのテストを実行
ctest --output-on-failure

# または個別に実行
./tests/unit/test_value_codec
./tests/unit/test_frame_codec
./tests/unit/test_session_config
./tests/integration/test_mc_client
./tests/integration/test_transport_loopback
```

### テストの種類

- **ユニットテスト**: 個別コンポーネントの動作検証
  - `test_value_codec`: データ型エンコード/デコードのテスト
  - `test_frame_codec`: フレームエンコード/デコードのテスト
  - `test_session_config`: 接続設定の検証テスト

- **統合テスト**: コンポーネント間の連携テスト
  - `test_mc_client`: McClientの全機能テスト（モックサーバー使用）
  - `test_transport_loopback`: トランスポート層のループバックテスト

## トラブルシューティング

### 接続できない場合

1. **ネットワーク接続を確認**
   ```bash
   ping 192.168.1.10  # PLCのIPアドレス
   telnet 192.168.1.10 5000  # ポート接続確認
   ```

2. **PLC側の設定を確認**
   - Ethernet通信ユニットの設定
   - ポート番号（通常5000または5001）
   - プロトコル設定（MCプロトコル3Eフレーム有効化）

3. **ファイアウォール設定を確認**
   - PC側のファイアウォールでポートが開いているか
   - PLC側のセキュリティ設定

### データが正しく読み取れない場合

1. **デバイス番号を確認**
   - PLCプログラムで使用しているデバイス番号と一致しているか
   - デバイス種別（D、M、X、Yなど）が正しいか

2. **データフォーマットを確認**
   - PLCでのデータ格納形式（符号付き/なし、サイズ）
   - ランダム読み取りでのValueFormat指定

3. **通信モードを確認**
   - バイナリ/ASCIIモードがPLC設定と一致しているか

### タイムアウトが発生する場合

1. **タイムアウト値を増やす**
   ```cpp
   config.timeout_ms = 10000;  // 10秒に延長
   ```

2. **PLC負荷を確認**
   - PLC側のスキャンタイムが長すぎないか
   - 同時接続数が多すぎないか

3. **ネットワーク品質を確認**
   - パケットロスやレイテンシが高くないか

## ライセンス

このプロジェクトのライセンスについては、LICENSEファイルを参照してください。

