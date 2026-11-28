# 開発ガイド

この文書は、ソースコードを取得してビルド・テストする人向けです。アプリの概要と公開状況は [README](../README.md) を参照してください。

## 対象環境

- 利用者向けの対応環境は Windows 11 x64 です。
- アプリ本体は MSVC x64、利用者が入力する C コードは LLVM-MinGW UCRT でコンパイルします。
- 開発用IDEは Visual Studio 2026 のみをサポートします。「C++ によるデスクトップ開発」と同ワークロードのCMakeツール、MSVC 19.50 以降のx64/x86 build tools、Windows 11 SDK 10.0.22000.0 以降が必要です。VS2022やその他のMSVCはsetupで明示的に拒否します。
- MSVC と Windows SDK は Visual Studio Installer が管理するため、`tools/versions.json` では配布・更新しません。セットアップは `vswhere` で既存の Visual Studio を検出し、その `VsDevCmd.bat` を現在の PowerShell プロセスに適用します。見つからない場合は理由と Installer で必要なコンポーネントを表示して停止します。自動インストール・修復・更新は行いません。

## 初回セットアップ

Visual Studio Installer で上記のコンポーネントを確認してから、PowerShell で次のコマンドを実行します。初回は固定ツールとライブラリを取得するため、インターネット接続が必要です。

```powershell
git clone https://github.com/nek0m0dok1/our-taskdepot.git
cd our-taskdepot
.\setup.cmd
```

取得には手元にある Git、または [Git for Windows](https://git-scm.com/downloads/win) を使います。pre-commit のフックを登録するには Git のチェックアウトが必要です。学校の PC でツールの導入やスクリプト実行に制限がある場合は、担当教員か管理者に確認してください。

すべての工程が成功したら作業ブランチを作成します。VS Code を使う場合は、このフォルダーを開いて推奨拡張機能を入れてください。

`setup.cmd` は Windows PowerShell 5.1 を起動し、Windows 11 x64、Visual Studio 2026と同梱CMake、MSVC x64、プロジェクト管理ツール、pre-commit、CMake configure/build、CTest、コード形式と clang-tidy の実行を確認します。完了メッセージが出るまで VS Code を開く必要はありません。

ツールは `.tools/`、Python と pre-commit の仮想環境は `.setup/venv/`、取得したアーカイブは `.setup/downloads/`、ログは `.setup/logs/` に保存します。`.setup/` と `.tools/` は Git に登録されません。インストール済みファイルと再利用するキャッシュは実行ごとにハッシュ・バージョンを検証します。失敗したダウンロードや不完全な展開は成功扱いせず、次の実行で再取得・再展開します。

すべてのツールのバージョンと配布物 SHA-256 は [tools/versions.json](../tools/versions.json) が管理元です。出典 URL も同じファイルに記録しています。セットアップは公式・確認済みの SHA-256 と一致するまでアーカイブを展開したり実行したりしません。

## VS Code と Git

セットアップで Git の pre-commit hook を登録します。コミット前に以下を確認します。

- 行末の空白と最終改行
- JSON 構文
- マージ競合マーカー
- 追加するファイルのサイズ上限

フックを手動で実行する場合は次のとおりです。

```powershell
.setup/venv/Scripts/python.exe -m pre_commit run --all-files
```

Git worktree は通常 `.git/hooks` を共有します。`setup.cmd` の標準 `pre-commit install` も共通の hook ディレクトリに登録されるため、同じリポジトリのほかの worktree にも hook が適用されます。worktree ごとに hook を分ける必要がある場合は、共通 `.git/config` を変更せず、独立した clone を使用してください。

`.vscode/settings.json` と `.vscode/extensions.json` は任意の VS Code 設定です。VS Code を使わない場合も同じ `setup.cmd` と CI を利用できます。

VS Code で C/C++ ファイルを保存すると、C/C++ 拡張機能が `.tools/clang-format/clang_format/data/bin/clang-format.exe` を実行します。この実行ファイルは `tools/versions.json` で固定した clang-format 20.1.8 です。C/C++ 拡張機能は、ファイルを開いたときと保存したときに固定版の clang-tidy も実行します。診断は「問題」パネルに表示され、検査項目は .clang-tidy で管理します。CMake Tools がビルド設定を C/C++ 拡張機能に渡します。設定が取得できない場合は VS Code で「CMake: Configure」を実行してください。

## ビルドとテスト

セットアップと `scripts/check.ps1` は既定の `default` CMake preset と同じく選定したVisual Studio 2026インスタンスの絶対パスを使い、generator instanceを明示します。ビルドキャッシュのgeneratorまたはVS instanceが異なる場合、既存ディレクトリを変更せず停止します。内容を確認してから `tools/clean-build.ps1` で `build-old-<timestamp>` に退避するか、手動で別の場所へ移して再実行してください。古い`.tools/`インストールの任意の確認・隔離は[旧ツールの整理手順](cleanup-legacy-build.md)に従ってください。CMake build cacheは同手順の対象外です。

```powershell
.\scripts\setup.ps1
```

このセットアップは pinned project tools と pre-commit も検証します。構成・ビルド・CTestだけを直接再確認する場合は、VS2026環境を選択して同じ絶対パスを使う `scripts/check.ps1` を実行してください。

CTest が 0 件の場合も失敗です。構成、ビルド、テストのどれかが失敗したときは setup 全体が失敗します。`.setup/logs/` の該当ログにはコマンド、外部ツールの出力、失敗の詳細が残ります。ログを共有する前に、個人情報が含まれていないか確認してください。

## コンパイラを含む配布 ZIP の作成

`setup.cmd` を完了し、CMake 4.2 以降が使える PowerShell で実行します。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package.ps1
```

既定の `build/` を Release 構成でビルドし、`out/our-taskdepot-win-x64.zip` を作成します。別の構成済みビルドディレクトリを使う場合は `-BuildDir` を指定します。ZIP がすでにある場合は先に移動してください。

パッケージ作成時は、セットアップが保存した LLVM-MinGW UCRT x64 の ZIP を `tools/versions.json` の SHA-256 と照合します。配布 ZIP を展開すると、`taskdepot_gui.exe` の隣に `compiler/bin/clang.exe` があり、ツールチェーンのヘッダー、ライブラリ、上流のライセンス文書も `compiler/` に入ります。利用者は ZIP の内容をまとめて展開してください。

2026-11-27 の検証では、Windows 11 x64 で Release ビルド、同梱の Clang 23.1.4 による C23 のコンパイル・リンク・実行、UTF-8 エラー診断を確認しました。Visual Studio のない別の Windows 11 PC での起動と、公開配布に必要な再配布条件の確認は未実施です。

## チームで開発を始める

1. 全員が同じ作業ブランチで `setup.cmd` を実行します。
2. 成功メッセージとテスト結果を確認し、[チーム確認表](team-checklist.md) に自分の結果を記入します。
3. 小さな変更で、ブランチ作成からコミット、Pull Request、別のメンバーによるレビューまでを確認します。

変更は Pull Request で共有し、CI とレビューを通して統合します。ブランチ運用は [GitHub ワークフローの ADR](adr/0009-github-workflow.md)、その他の設計判断は [ADR 一覧](adr/) を参照してください。

## コード品質と CI

clang-format は `.clang-format` に従い、CI では差分を作らない検査モードで実行します。clang-tidy は `clang-analyzer-*` と `bugprone-*` を有効にします。診断警告はレビュー情報として出力し、警告だけでは CI を失敗させません。実行不能、異常終了、内部エラーは CI を失敗させます。

GitHub Actions は同じ `tools/versions.json` と PowerShell のセットアップ・チェック手順を使います。ホストランナーは Windows Server 2025 のため、CI では Windows 11 固有の OS 名チェックを省略し、AMD64、MSVC、SDK、ツール、構成、ビルド、CTest を確認します。ランナーの Visual Studio と SDK の細かなリビジョンは GitHub が管理するため、manifest の固定ツールとは別に確認します。

## 依存物の保管場所とライセンス

Scintilla の `Scintilla.dll` は `.tools/scintilla/wscite/Scintilla.dll` に展開します。LLVM-MinGW は利用者コードのコンパイルと検証に使い、アプリ本体の MSVC ビルドとは独立しています。cJSON と Unity は `tools/versions.json` の `libraries` の固定版を CMake FetchContent が SHA-256 検証して取得します。

Python は Astral の `python-build-standalone` 専用展開物を `.tools/python/` に置きます。Python 本体の PSF License と配布プロジェクトのライセンス文書は、取得物のライセンス資料および [python-build-standalone](https://github.com/astral-sh/python-build-standalone) を参照してください。clang-format / clang-tidy は PyPI の Windows wheel を個別に SHA-256 検証し、展開後の実行ファイルを検証します。これらの開発ツールはアプリへ同梱しません。配布物のライセンス・再配布条件は [third-party-notices.md](third-party-notices.md) に記録します。

## セットアップ検証記録

以下は基準 `aaed34d7800175e826ddb1a240ecae75712cba37` の旧構成に対する履歴記録です。VS2026専用のVisual Studio 18 generatorと厳格なインスタンス選定を加えた現在の変更を検証した結果ではありません。現変更の結果は[VS2026 foundation verification record](vs2026-foundation-verification.md)を参照してください。

Windows 11 x64、既存 Visual Studio Community 2026 / MSVC 19.51.36260 / SDK 10.0.26100 のコーディネーターの独立 clone で、当時の `setup.cmd` 2 回がともに exit 0、setup 内で CMake configure/build 成功、CTest 1/1、clang-tidy 完了を確認しました。Windows SDK の解析警告は advisory です。`pre-commit run --all-files` も exit 0 でしたが、その実行の clang-format hook は対象ファイルなしで skip でした。setup 自体の format check は実行済みです。

実装ワーカーの確認、独立 Sol レビュー、コーディネーターの独立 clone の結果、実装ワーカーの専用 retry fixture は [tooling-validation.md](tooling-validation.md)、全体の基準と履歴は [development-validation.md](development-validation.md) に区別して記録しています。既存 Visual Studio、グローバル PATH、永続的な環境設定は変更していません。GitHub Actions、新しいクリーンな Windows PC、Visual Studio 不在のケース、別の VS/SDK 構成は未検証です。アプリのライセンスと第三者コンポーネントの再配布判断も未解決であり、今回の対象は開発基盤のみです。
