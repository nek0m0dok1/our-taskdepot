---
name: tooling validation
discription: Results, conditions, and outstanding checks for project-local Windows developer tools.
---

# 開発ツールの検証記録

## 実行条件

- Windows 11 x64、AMD64 build 26200
- Visual Studio Community 2026、MSVC 19.51.36260.0、Windows SDK 10.0.26100.0
- PowerShell 5.1
- 現在の検証基準コミット: `aaed34d7800175e826ddb1a240ecae75712cba37` (`foundation-resume-fixes`)

以下は実装ワーカー、独立 Sol レビュー、コーディネーターの独立 clone による報告を区別した記録です。今回の文書更新でこれらの実行を再現したものではありません。

Visual Studio は既存インストールを `vswhere` で検出し、`VsDevCmd.bat` の設定をセットアッププロセス内だけに適用しました。Visual Studio、Python の既存登録、ユーザー/システム PATH、永続的な環境設定は変更していません。生成した `.tools/`、`.setup/` は各検証を行ったチェックアウト内に配置しました。

## 実装ワーカーが確認したこと

| 項目 | 結果 | 根拠 |
| --- | --- | --- |
| PowerShell 構文 | PASS | `scripts/*.ps1` を PowerShell 5.1 Parser で解析 |
| JSON 構文 | PASS | `tools/versions.json`、`CMakePresets.json` を `ConvertFrom-Json` で解析 |
| 取得物の SHA-256 | PASS | setup 実行で各アーカイブを manifest と照合 |
| 展開後の実行版 | PASS | MinGit、Python、CMake/CTest、Ninja、clang-format、clang-tidy、LLVM-MinGW の版を実行確認 |
| Scintilla | PASS | `wscite567.zip` の SHA-256 `24b6c63f…599f353`、`.tools/scintilla/wscite/Scintilla.dll` の PE64 とファイル版 5.6.7 を確認 |
| 取得・キャッシュ fixture | PASS | 初回 ZIP 展開、正しいキャッシュ再利用、不完全な展開の回復、壊れたキャッシュの拒否、失敗した取得を確認 |
| `git diff --check` | PASS | 空白エラーなし。Git はLFからCRLFへの変換予定を通知 |

実際に版を確認したツールは MinGit `2.50.1.windows.1`、Python `3.13.7`、CMake/CTest `3.31.8`、Ninja `1.13.1`、clang-format `20.1.8`、clang-tidy `21.1.6`、LLVM-MinGW `20261020` (clang `23.1.4`) です。pre-commit `4.3.0` は `.setup/venv` に導入され、初回未導入状態から回復するところまで確認しました。

`scripts/test-install-tools-fixtures.ps1` はネットワークを使わない壊れた/途中状態の fixture と、SHA を照合済みの実 Scintilla 配布物の両方を使います。Scintilla の公式 ZIP には `SciLexer.dll` はなく、`Scintilla.dll` が含まれることを確認したため manifest と配置先に反映しました。

実装ワーカーは修正後の専用 retry fixture も実行し、resume の exit 33 から full retry の exit 0 への回復、期待するログ、SHA-256、検証後のキャッシュ昇格、展開・実行ファイルを確認しました。

## 最終結果：コーディネーターの独立 clone

修正済み `aaed34d` のコードで `setup.cmd` を 2 回と `pre-commit run --all-files` を実行しました。構成・ビルド・テスト・解析は setup 内の結果です。

| 項目 | 結果・確認範囲 |
| --- | --- |
| `setup.cmd` の再実行 | 修正後の 2 回とも exit 0。ツール検証、pre-commit 導入・hook 登録、format check、構成・ビルド・テスト・解析を完了 |
| CMake configure/build | 成功。cJSON/Unity の固定版・SHA-256 付き FetchContent 取得も統合済み |
| CTest | 1/1 成功 |
| clang-tidy | 完了。Windows SDK の警告は advisory として記録 |
| `pre-commit run --all-files` | exit 0。この実行では clang-format hook は対象ファイルなしで skip。setup 自体では format check を実行済みであり、この hook 実行で全 C/C++ の整形を確認したとは扱わない |

最終検証では PowerShell / JSON 解析と `git diff --check` も PASS と報告されています。これらは上記の独立 clone で実行したコマンドとは区別します。

## 独立 Sol レビュー

最終修正に指摘はありませんでした。レビュー担当は旧コードの未定義 `$_` による例外と、修正後の `$line` による期待したログ出力を独立に再現しました。これはレビューと当該不具合の独立再現であり、上表の clone での一連実行はコーディネーターの確認です。

## 履歴：実装ワーカーの途中段階（現在の状態ではない）

旧基準コミットは `c120d18041dbfb024eb7891083115198d316e8d6` でした。

`setup.ps1` は次の段階まで実行しました。

1. 既存 MSVC/SDK を確認し、manifest に登録した全ツールのアーカイブを取得・SHA 照合・展開しました。
2. 各ツールの実行版を照合し、Python の `.setup/venv` と pre-commit を準備しました。
3. clang-format 検査で、担当外の `tests/test_foundation.c` に未整形箇所を検出したため停止しました。初回は PowerShell の `-Include` の挙動により `tests/CMakeLists.txt` も誤って対象になりましたが、拡張子を明示して対象を C/C++ に限定する修正を入れました。

この停止は当時 setup が成功を誤報しなかった記録です。当時は CMake configure、ビルド、CTest、clang-tidy の完了を確認しておらず、CMake 依存取得の別担当変更も未統合でした。これらの制約は上記の最終結果で更新されています。

初回は MSVC 実行ファイルの製品版 `14.51` とコンパイラ版 `19.51` の表記差を検出し、両表記を正しく扱うよう修正しました。次の大容量取得では Windows PowerShell の HTTP 接続がホスト側で中断されたため、Windows 標準 `curl.exe` の再試行/途中ファイル再開を使い、最終的に archive SHA と版を確認できました。途中ファイルは完全アーカイブとして扱わず、SHA が一致しなければ展開・実行しません。

## 未実施・未解決

- GitHub Actions 上の CI 実行。workflow の存在やローカル成功は Actions の成功を意味しません。
- 新しいクリーンな Windows PC と Visual Studio 不在のケース。
- Visual Studio 2022 17.8 以降と Windows SDK 10.0.22000.0 以降を使った別環境での確認。
- アプリのライセンス選定と第三者コンポーネントの再配布判断。開発基盤の検証完了によって確定したものではありません。

実装ワーカーの作業開始時に指定された ADR-0010 は `docs/adr/` と `docs/` のどちらにも存在しませんでした。当時利用できた設計資料と要件は README、[development.md](development.md)、`docs/adr/0002-build-system.md`、`0003-compiler-integration-policy.md`、`0009-github-workflow.md` で確認しました。今回の文書更新では、Git 管理外の元要件書 `C:/Users/MimiC/our-taskdepot/docs/dev-prepare.md` も読み、検証条件と未完了事項を明示する方針を確認しました。

セットアップログの保存先は各実行チェックアウトの `.setup/logs/` です。コマンド、外部ツールの標準出力/標準エラー、終了コードと失敗詳細が記録されます。ログは Git 管理外であり、この文書更新用 worktree に独立 clone のログが存在するとは限りません。
