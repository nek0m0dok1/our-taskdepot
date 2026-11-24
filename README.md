# our-taskdepot

Windows 11 x64 向けの C 言語学習支援アプリです。アプリ本体は MSVC でビルドし、利用者コードには UCRT 版 LLVM-MinGW を使用します。

## 開発環境の準備

1. Visual Studio Installer で Visual Studio 2026 18.x に「C++ によるデスクトップ開発」とそのCMakeツール、MSVC 19.50以降のx64/x86 build tools、Windows 11 SDK 10.0.22000.0 以降を追加してください。VS2022などの別MSVC環境はサポートしません。
2. リポジトリ直下の `setup.cmd` を実行してください。ツールのダウンロードと SHA-256 検証、pre-commit の設定、MSVC による構成・ビルド、CTest まで行います。
3. 完了後に VS Code でこのフォルダーを開き、推奨拡張機能をインストールしてください。C/C++ ファイルの保存時に clang-format が整形し、clang-tidy が問題を報告します。

初回のセットアップでは、固定バージョンのツールとライブラリを取得するため、インターネット接続が必要です。セットアップはツールをこのチェックアウトの `.tools/`、Python 仮想環境・ダウンロードキャッシュ・実行ログを `.setup/` に置きます。Visual Studio のインストールや既存 Python の登録、ユーザー・システム PATH は変更しません。繰り返し実行できます。詳しくは [docs/development.md](docs/development.md) を参照してください。

プロジェクト管理ツールの固定バージョン、公式ダウンロード先、確認元、SHA-256 は [tools/versions.json](tools/versions.json) にあります。CMakeはVisual Studio 2026同梱の4.2以降を使用し、Visual Studio 18 2026 generator、x64、v145を選びます。ライブラリのダウンロードとハッシュ検証はCMakeのFetchContentが行います。

## ローカル確認

`setup.cmd` が configure、build、CTest、コード形式、clang-tidy を実行します。失敗時は `.setup/logs/` の最新ログを確認してください。現在の実行結果と未確認項目は [docs/vs2026-foundation-verification.md](docs/vs2026-foundation-verification.md) に、以前の基盤に関する検証履歴は [docs/tooling-validation.md](docs/tooling-validation.md) にあります。

## 導入するツールと必要な理由

MSVCとWindows SDKは、各PCのVisual Studio Installerで先に確認してください。それ以外の固定ツールは`setup.cmd`がプロジェクト内へ用意します。

| ツール | 用途と必要な理由 |
| --- | --- |
| MSVC | アプリ本体のCコードをWindowsの実行ファイルへ変換するために必要なコンパイラです。 |
| Windows SDK | Win32 APIのヘッダーとライブラリを提供し、Windows向けのコードをビルドするために必要です。 |
| Git（MinGit） | ソースの変更履歴・ブランチ・共有を扱い、6人の共同開発とGitフックを動かすために必要です。 |
| CMake／CTest | 共通設定からビルド手順を生成し、登録したテストを同じ手順で実行するために必要です。 |
| Python | コミット前チェックを管理するpre-commitを、プロジェクト専用の環境で実行するために必要です。 |
| pre-commit | コミット前の空白・JSONなどの軽量チェックを自動で実行するために必要です。 |
| clang-format | Cコードの書式を統一し、初心者同士のレビューで書式の差に時間を取られないようにするために必要です。 |
| clang-tidy | Cコードを静的解析して不具合の候補を見つけ、レビューを補助するために必要です。 |
| LLVM-MinGW UCRT | 利用者が入力するCコードをコンパイルするために必要な、本体開発用MSVCとは別のコンパイラです。 |
| Scintilla | 将来のアプリのコード編集欄を構成するために必要なGUI部品です。 |
| cJSON | JSON形式で管理する日本語解説データをC言語から読み込むために必要なライブラリです。 |
| Unity | GUIに依存しないCコードの単体テストを実装するために必要なライブラリです。 |

最初のcloneには既存のGit、または[Git for Windows](https://git-scm.com/downloads/win)を使用してください。GitHubのZIPだけではGitフックを登録できないため、Gitのチェックアウトとして用意します。
学校PCで追加導入やスクリプト実行が禁止されている場合は、担当教員・管理者へ確認してください。

## 6人での開始確認

GitHubへ変更が反映された後は、PowerShellで次のように取得して開始します。未公開の間は、チームで共有した作業フォルダーを使用してください。

```powershell
git clone https://github.com/nek0m0dok1/our-taskdepot.git
cd our-taskdepot
.\setup.cmd
```

1. 全員が同じ作業ブランチを取得し、リポジトリ直下で`.\setup.cmd`を実行します。
2. 成功メッセージとテスト結果を確認し、[チーム確認表](docs/team-checklist.md)へ記録します。
3. 小さな練習変更で、ブランチ作成・コミット・PR・別メンバーのレビューまでを体験します。

現在の基盤はビルド・依存物・テストの確認用です。アプリ本体の画面やエラー解説機能は今後実装します。
今回の再確認範囲は[環境構築記録](docs/environment-setup.md)を参照してください。

## 開発方針

- 設計判断: [docs/adr/](docs/adr/)
- 開発手順: [docs/development.md](docs/development.md)
- 変更は Pull Request で統合し、CI とレビューを通します。
