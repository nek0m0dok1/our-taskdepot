# 開発手順

## 対象環境

- 利用者向けの対応環境は Windows 11 x64 です。
- アプリ本体は MSVC x64、利用者が入力する C コードは LLVM-MinGW UCRT でコンパイルします。
- 開発用の Visual Studio は Visual Studio 2022 17.8 以降、または Visual Studio 2026 を想定します。「C++ によるデスクトップ開発」、MSVC 19.38 以降の x64/x86 build tools と Windows 11 SDK 10.0.22000.0 以降が必要です。
- MSVC と Windows SDK は Visual Studio Installer が管理するため、`tools/versions.json` では配布・更新しません。セットアップは `vswhere` で既存の Visual Studio を検出し、その `VsDevCmd.bat` を現在の PowerShell プロセスに適用します。見つからない場合は理由と Installer で必要なコンポーネントを表示して停止します。自動インストール・修復・更新は行いません。

## 初回セットアップ

1. リポジトリをチェックアウトします。
2. Visual Studio Installer で上記の MSVC と Windows SDK が入っていることを確認します。
3. エクスプローラーから `setup.cmd` を実行します。
4. すべての工程が成功したら VS Code でリポジトリを開き、作業ブランチを作成します。

`setup.cmd` は Windows PowerShell 5.1 を起動し、Windows 11 x64、MSVC x64、ツール、pre-commit、CMake configure/build、CTest、コード形式と clang-tidy の実行を確認します。完了メッセージが出るまで VS Code を開く必要はありません。

ツールは `.tools/`、Python と pre-commit の仮想環境は `.setup/venv/`、取得したアーカイブは `.setup/downloads/`、ログは `.setup/logs/` に保存します。`.setup/` と `.tools/` は Git に登録されません。インストール済みファイルと再利用するキャッシュは実行ごとにハッシュ・バージョンを検証します。失敗したダウンロードや不完全な展開は成功扱いせず、次の実行で再取得・再展開します。

すべてのツールのバージョンと配布物 SHA-256 は [tools/versions.json](../tools/versions.json) が管理元です。出典 URL も同じファイルに記録しています。セットアップは公式・確認済みの SHA-256 と一致するまでアーカイブを展開したり実行したりしません。

## VS Code と Git

セットアップで Git の pre-commit hook を登録します。コミット前に以下を確認します。

- clang-format による C/C++ の整形
- 行末の空白と最終改行
- JSON 構文
- マージ競合マーカー
- 追加するファイルのサイズ上限

自動整形で差分が変わった場合、hook はコミットを止めます。差分を確認してから、必要に応じてもう一度コミットしてください。フックを手動で実行する場合は次のとおりです。

```powershell
.setup/venv/Scripts/python.exe -m pre_commit run --all-files
```

Git worktree は通常 `.git/hooks` を共有します。`setup.cmd` の標準 `pre-commit install` も共通の hook ディレクトリに登録されるため、同じリポジトリのほかの worktree にも hook が適用されます。worktree ごとに hook を分ける必要がある場合は、共通 `.git/config` を変更せず、独立した clone を使用してください。

`.vscode/settings.json` と `.vscode/extensions.json` は任意の VS Code 設定です。VS Code を使わない場合も同じ `setup.cmd` と CI を利用できます。

VS Code で C/C++ ファイルを保存すると、推奨拡張の Clang-Format が `.tools/clang-format/clang_format/data/bin/clang-format.exe` を実行します。この実行ファイルは `tools/versions.json` で固定した clang-format 20.1.8 です。clangd 拡張は補完・診断などの言語機能用であり、保存時の整形には使いません。

## ビルドとテスト

セットアップは既定の `default` CMake preset を使います。手動で再実行する場合は、Visual Studio Developer PowerShell で次を実行します。

```powershell
& .tools/cmake/bin/cmake.exe --preset default
& .tools/cmake/bin/cmake.exe --build build --config Debug --parallel
& .tools/cmake/bin/ctest.exe --test-dir build -C Debug --output-on-failure --no-tests=error
```

CTest が 0 件の場合も失敗です。構成、ビルド、テストのどれかが失敗したときは setup 全体が失敗します。`.setup/logs/` の該当ログにはコマンド、外部ツールの出力、失敗の詳細が残ります。ログを共有する前に、個人情報が含まれていないか確認してください。

## コード品質と CI

clang-format は `.clang-format` に従い、CI では差分を作らない検査モードで実行します。clang-tidy は `clang-analyzer-*` と `bugprone-*` を有効にします。診断警告はレビュー情報として出力し、警告だけでは CI を失敗させません。実行不能、異常終了、内部エラーは CI を失敗させます。

GitHub Actions の workflow は同じ `tools/versions.json` と PowerShell のセットアップ・チェック手順を使う構成です。ホストランナーは Windows Server 2025 のため、CI だけ Windows 11 固有の OS 名チェックを省略しますが、AMD64、既存 MSVC、SDK、ツール、構成、ビルド、CTest の検証を行う設定です。現在の基準コミットでは GitHub Actions は未実行です。ランナーの Visual Studio/SDK の細かなリビジョンは GitHub が管理するため、manifest の固定ツールとは別に記録・確認します。

## 依存物の保管場所とライセンス

Scintilla の `Scintilla.dll` は `.tools/scintilla/wscite/Scintilla.dll` に展開します。LLVM-MinGW は利用者コードの検証専用で、アプリ本体の MSVC ビルドとは独立しています。cJSON と Unity は `tools/versions.json` の `libraries` の固定版を CMake FetchContent が SHA-256 検証して取得します。

Python は Astral の `python-build-standalone` 専用展開物を `.tools/python/` に置きます。Python 本体の PSF License と配布プロジェクトのライセンス文書は、取得物のライセンス資料および [python-build-standalone](https://github.com/astral-sh/python-build-standalone) を参照してください。clang-format / clang-tidy は PyPI の Windows wheel を個別に SHA-256 検証し、展開後の実行ファイルを検証します。これらの開発ツールはアプリへ同梱しません。配布物のライセンス・再配布条件は [third-party-notices.md](third-party-notices.md) に記録します。

## セットアップ検証記録

現在の検証基準は `aaed34d7800175e826ddb1a240ecae75712cba37` です。Windows 11 x64、既存 Visual Studio Community 2026 / MSVC 19.51.36260 / SDK 10.0.26100 のコーディネーターの独立 clone で、修正後の `setup.cmd` 2 回がともに exit 0、setup 内で CMake configure/build 成功、CTest 1/1、clang-tidy 完了を確認しました。Windows SDK の解析警告は advisory です。`pre-commit run --all-files` も exit 0 でしたが、その実行の clang-format hook は対象ファイルなしで skip でした。setup 自体の format check は実行済みです。

実装ワーカーの確認、独立 Sol レビュー、コーディネーターの独立 clone の結果、実装ワーカーの専用 retry fixture は [tooling-validation.md](tooling-validation.md)、全体の基準と履歴は [development-validation.md](development-validation.md) に区別して記録しています。既存 Visual Studio、グローバル PATH、永続的な環境設定は変更していません。GitHub Actions、新しいクリーンな Windows PC、Visual Studio 不在のケース、別の VS/SDK 構成は未検証です。アプリのライセンスと第三者コンポーネントの再配布判断も未解決であり、今回の対象は開発基盤のみです。
