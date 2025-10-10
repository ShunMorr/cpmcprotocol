# MC Protocol C++ Communication Package Specification

## 1. 背景と目的
- MC プロトコル (MELSEC Communication Protocol) を用いて PC アプリケーションから三菱電機製 PLC を制御できる C++ 向け通信パッケージを作成する。
- 本パッケージは iQ-R シリーズの 3E フレームインターフェースを主対象としつつ、同プロトコルに対応した他シリーズ (Q/L/QnA/iQ-L) への拡張性を維持する。
- 既存の C/C++ 実装である `sample/libslmp2` (libslmp / libmelcli) および Python 実装 `sample/pymcprotocol` を参照し、互換性と機能範囲を整理する。

## 2. スコープ
- 3E フレーム (TCP/IP) を用いた iQ-R シリーズ PLC との通信確立・維持。
- ランタイム操作 (遠隔 RUN/STOP、ロック/アンロック等) のサポート。
- 非連続デバイスを含む複数ブロック読出し・書込み (ランダムアクセス)。
- 内部デバイス (X, Y, M, D など) およびバッファメモリ領域のワード/ビット単位 I/O。
- バイナリ／ASCII 両モードへの対応方針定義 (実装優先度はバイナリ)。
- プロトコルスタック、エラー処理、ロギング、テスト戦略の整理。

## 3. 参考資料
- `sample/libslmp2/doc/mainpage.dox` — SLMP コマンド/フレーム構造および MELSEC クライアントライブラリの提供機能。
- `sample/pymcprotocol/README.md`, `sample/pymcprotocol/src/pymcprotocol/type3e.py` — 3E/4E 実装の具体的な要求値、デバイスアクセス API、リモート操作手順。
- 三菱電機「SLMP (Seamless Message Protocol) Specifications (Overview) BAP-C2006ENG-001-J」および MC プロトコル Q/L/iQ-R 3E Frame Reference。

## 4. 用語定義
- **3E フレーム**: Ethernet 経由で MC プロトコルを実装する際の標準フレーム形式。
- **ネットワーク番号 / PC 番号**: マルチドロップ構成時の宛先識別子。
- **デバイスコード**: MC プロトコルで使用する内部デバイス種別識別子。
- **ランダムアクセス**: 非連続アドレスの複数デバイスを単一要求で読み書きする操作。

## 5. 機能要件
- **接続管理**
  - TCP/IP ベースのソケット接続 (`connect`, `disconnect`, `reconnect`).
  - タイムアウト、再試行、キープアライブ設定の管理。
  - ネットワーク番号、PC 番号、モジュール I/O 番号、モジュールステーション番号の設定 API。
- **通信プロトコル**
  - サブヘッダー 0x5000/0x5003 (3E バイナリ/ASCII) への対応。
  - 要求フレーム生成および応答フレーム解析 (libslmp `slmpcodec` 相当)。
  - バイナリモードをデフォルトとし、ASCII モード切替 API を提供。
- **デバイスアクセス**
  - 逐次読出し (`batchReadWord`, `batchReadBit`) および書込み。
  - 非連続デバイスへのランダム読出し／書込み (Type3E `randomread`, `randomwrite` 参照)。
  - バッファメモリ(F/デバイス) のブロックアクセス。
  - 設定可能なデータ型: ビット、ワード、ダブルワード、シングル/ダブルプリシジョン (必要に応じ 2's complement 変換)。
- **ランタイム操作**
  - `remote_run`, `remote_stop`, `remote_pause`, `remote_reset`, `remote_latch_clear`。
  - `remote_unlock`/`remote_lock` を含むパスワード管理 (iQ-R は 32 文字まで)。
  - CPU タイプ読出し (`read_cputype`)。
- **デバイス定義**
  - デバイスコードと基数マップを libslmp `cmdcode.h` および pymcprotocol `mcprotocolconst.py` を基に整理。
  - iQ-R 系の 4 バイトデバイス番号、その他シリーズの 3 バイト番号を判別。
- **エラー処理**
  - MC 応答コード(終了コード)のデコードと例外化。
  - ソケット例外・タイムアウトの体系化。
  - 再送戦略 (必要に応じ指数バックオフ)。
- **診断 / ロギング**
  - 送受信フレームの HEX ダンプ (デバッグ用)。
  - 操作ログ、エラーコード、リトライ履歴。

## 6. 非機能要件
- **パフォーマンス**: 連続読出しで 10ms オーダーのラウンドトリップを目標 (LAN 内)。
- **スレッドセーフティ**: セッション単位での排他制御。複数接続の並列利用を想定し、ステートフル情報をインスタンスに内包。
- **移植性**: POSIX/Windows 双方の TCP ソケットで動作可能な抽象化。
- **コード標準**: C++17 以上、エラーハンドリングは例外ベース。CI で clang-format/clang-tidy 運用を想定。

## 7. システム構成
- **Transport 層**: ソケット接続、再接続管理、受信ループ。
- **SLMP Codec 層**: フレーム組立・パース、エンディアン変換、デバイスコード変換 (libslmp の `slmp_frame`, `slmpcodec` 設計を参照)。
- **Session 管理**: 接続パラメータ、タイムアウト、リクエスト ID 管理。
- **Value Codec 層 (中位レイヤ)**:
  - ワード列/ビット列 ⇔ アプリケーション値(整数/浮動小数点/文字列など) の相互変換。
  - Device 名や指定フォーマットに応じて、2's complement・IEEE754・ASCII/BINARY などを統一的に扱う。
  - 型情報 (`ValueFormat`) を保持し、ランダムアクセスで混在する値のデコードを支援。
- **DeviceAccess API (高位レイヤ)**: Value Codec を活用した逐次/ランダム操作の公開インターフェース。複数種混在デバイスを1リクエストで取得し、型タグ付きの `DeviceValue` として返却。
- **RuntimeControl API**: RUN/STOP/ロック関連操作。
- **Diagnostics**: ロギング、トレース、プロファイル。
- **Configuration**: YAML/JSON 等のロード対応を検討 (初版はプログラム API のみ)。

## 8. API デザイン指針
- `McSessionConfig` — 接続パラメータ構造体 (IP, Port, Network, PC, DestIO, DestSta, Timeout)。
- `McClient` クラス
  - `connect(const McSessionConfig&)`
  - `disconnect()`
  - `setAccessOption(const AccessOption&)` — `Type3E.setaccessopt` に相当。
  - `readWords(DeviceRange range)` / `readBits(DeviceRange range)`
  - `writeWords(DeviceRange range, span<uint16_t> values)`
  - `randomRead(const DeviceReadPlan&)` / `randomWrite(const DeviceWritePlan&)`
  - `remoteRun(RemoteRunOption)`
  - `remoteStop()`, `remotePause()`, `remoteReset()`, `remoteLatchClear()`
  - `remoteLock(const std::string& password)`, `remoteUnlock(...)`
  - `readCpuType() -> CpuInfo`
- `DeviceRange` / `RandomDeviceRequest` 等のヘルパ構造を用意。
- 例外型: `McProtocolError` (終了コード付き)、`TransportError`、`TimeoutError` など。
- `ValueFormat` 列挙 (例: `Int16`, `UInt16`, `Int32`, `UInt32`, `Float32`, `Float64`, `AsciiString(n)`, `RawWords(n)` など) と `DeviceValue` (`std::variant` ベース) を定義。
- Value Codec 層の公開インターフェース:
  - `std::vector<std::uint16_t> encode(const DeviceWritePlan&)`
  - `std::vector<DeviceValue> decode(const DeviceReadPlan&, std::span<const std::uint16_t>)`
  - 文字列や浮動小数点のエンコード/デコードでエラーチェック (桁数、NaN、範囲) を実施。

## 9. プロトコル詳細 (3E バイナリ)
- フレーム構成: サブヘッダー (2B) / ネットワーク (1B) / PC (1B) / モジュール I/O (2B) / ステーション (1B) / データ長 (2B) / タイマー (2B) / コマンド + サブコマンド (各2B) / 要求データ。
- タイマー単位: 250ms。`setAccessOption` では秒単位で指定し、4 倍した値をエンコード。
- 応答解析: オフセット 9 (バイナリ) で完了コード、11 バイト目以降がデータ (pymcprotocol `Type3E._get_answerdata_index` 参照)。
- デバイス番号: iQ-R 系は 32bit little-endian、他シリーズは 24bit little-endian + 8bit デバイスコード。
- 値データ: Value Codec 層でワード配列を `ValueFormat` に従って復号。例: `Int16`=1ワード、`Float32`=2ワード、`AsciiString(n)`=nワード (ASCII 2 文字/ワード)。

## 10. ランダムアクセス仕様
- MC コマンド `0x0406` (ランダム読出し)、`0x1406` (ランダム書込み) を使用。libslmp `intmem_codec.h`／pymcprotocol `randomread` 実装を参照。
- 単一要求で複数デバイス (ビット/ワード混在可能) を扱い、最大デバイス数とデータ長はプロトコル仕様に従う (iQ-R では 960 ワード程度)。
- 応答データは要求順で返却されるため、コール側で整列。
- `DeviceReadPlan`: `(DeviceAddress, ValueFormat)` の配列。Value Codec 層がフレームペイロードから `std::vector<DeviceValue>` (内部は `std::variant<int16_t, std::uint32_t, float, double, std::string, std::vector<std::byte>>` など) を生成。
- `DeviceWritePlan`: 書込時も `(DeviceAddress, DeviceValue)` で受け取り、Value Codec 層が MC プロトコルのワード列へ変換。
- 異なる型が混在した要求を許容し、Value Codec 層がそれぞれの `ValueFormat` を用いて変換することでアプリケーションからは統一的な API で扱えるようにする。
- 現行実装ではランダムビット書き込みは未対応（例外で通知）。ワード／ダブルワードのランダム書き込み／読出しはサポート済みで、ビット対応は今後の拡張候補。

## 11. ランタイム操作仕様
- コマンドコードは libslmp `remctrl.h` および pymcprotocol `remote_run` 実装を参照。
- `remote_run` は `clear_mode` や `force_exec` をオプション化。
- `remote_unlock` は ASCII/Binary の両表現に対応し、パスワードは ASCII コードで送信。
- iQ-R でのロック解除時は 16 文字 (32 文字) の ASCII 送信が可能である点を考慮。
- E71 経由でない直接接続の場合、`C059` (機能未サポート) が返却されるため、エラーコードに応じたメッセージを提供。

## 12. エラーと例外処理
- SLMP 終了コードを列挙 (libslmp `slmperr.h` を参照) し、アプリケーション例外へマッピング。
- ソケット切断・タイムアウト時は接続状態を `Disconnected` に遷移し、必要に応じ再接続 API を提供。
- コマンドレベルの検証 (デバイス名形式、値範囲、ASCII 時の桁数) を事前に実施。

## 13. ロギングと監視
- ログカテゴリ: `transport`, `protocol`, `runtime`.
- フレームダンプはデバッグレベルでのみ出力し、パスワード等の秘匿情報はマスク。
- 遅延・再送回数・エラーコードを計測し、外部メトリクス連携を想定。

## 14. テスト戦略
- **ユニットテスト**: フレームエンコード／デコード、デバイスコード変換、エラー処理。
- **モック通信テスト**: TCP ダミーサーバを用いた往復試験。libslmp `svrskel` サンプルを参考に擬似応答を実装。
- **実機テスト**: iQ-R (3E) での基本操作、ランダムアクセス、ランタイム操作を手順化。py-based スモーク (`pymcprotocol`) と比較。
- **互換性テスト**: 既存 Python 実装と同一シーケンスを送り、応答一致を確認。

## 15. 拡張検討
- 4E フレーム対応、UDP (局所ネットワーク) への拡大。
- 複数セッションの共通接続プール化。
- OPC UA / MQTT ブリッジとの連携。
- CLI / GUI ツール提供によるデバッグ支援。

## 16. マイルストーン案
1. プロトコルコア (Transport + Codec) 実装とユニットテスト整備。
2. 逐次読書き API とランタイム操作の実装、モックテスト。
3. ランダムアクセス API と実機評価。
4. ロギング／設定機構、ドキュメント整備。
