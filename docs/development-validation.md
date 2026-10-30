---
name: development foundation validation
discription: Records the final validation baseline, evidence owners, and remaining limits for the development foundation.
---

# 開発基盤の検証記録

現在の検証基準コミット: `aaed34d7800175e826ddb1a240ecae75712cba37` (`foundation-resume-fixes`)。
本記録は修正済みコードについて報告された最終結果をまとめたものです。今回の文書更新で setup やビルドを再実行したという意味ではありません。

## 最終状態

ツール取得・SHA-256 検証・キャッシュ再利用・展開・実行版検証、既存 Visual Studio 検出、pre-commit 導入と hook 登録、CMake configure/build、CTest、clang-format 検査、clang-tidy の連携は実装済みです。cJSON/Unity は固定版と SHA-256 を指定した CMake FetchContent で取得します。Scintilla 5.6.7 の x64 DLL も固定・検証対象です。アプリ本体の機能開発は行っていません。

## 実施主体ごとの確認結果

| 実施主体 | 確認内容と結果 |
| --- | --- |
| 実装ワーカー | manifest の配布物取得・SHA-256・展開後の版、Scintilla の PE64/版、取得・キャッシュの正常系と異常系 fixture を確認。途中段階の setup 停止も記録し、成功とは扱わなかった。 |
| 独立 Sol レビュー | 最終修正に指摘なし。旧コードの未定義 `$_` による例外と、修正後の `$line` による期待したログ出力を独立に再現。これはレビューと不具合の再現確認であり、setup 全体をレビュー担当が再実行したとの主張ではない。 |
| コーディネーターの独立 clone | 修正後の `setup.cmd` を 2 回実行し、両方 exit 0。setup 内で CMake configure/build 成功、CTest 1/1、clang-tidy 完了（Windows SDK の警告は advisory）。`pre-commit run --all-files` は exit 0。この実行では clang-format hook は対象ファイルなしで skip、setup 自体の format check は実行済み。 |
| 実装ワーカーの専用 retry fixture | resume が exit 33、その後の full retry が exit 0。期待するログ、SHA-256、検証後のキャッシュ昇格、展開と実行ファイルを確認。 |

最終検証では PowerShell と JSON の構文解析、`git diff --check` も PASS です。実行条件と各確認の詳細は [tooling-validation.md](tooling-validation.md) に記録しています。

## 実行条件と未確認事項

Windows 11 x64、既存 Visual Studio Community 2026、MSVC 19.51.36260、Windows SDK 10.0.26100 で検証しました。既存 Visual Studio、グローバル PATH、永続的な環境設定は変更していません。

- GitHub Actions は未実行です。ローカルの成功を CI 成功とは扱いません。
- 新しいクリーンな Windows PC、Visual Studio が存在しない場合、別の Visual Studio/SDK 構成は未検証です。
- アプリのライセンス選定と第三者コンポーネントの再配布判断は未解決です。[third-party-notices.md](third-party-notices.md) の記載はこれらの判断の完了を意味しません。

## 履歴（現在の状態ではない）

旧基準 `a1f155847ac375798d11d6e65a85a8e9b97a8716` と実装コミット `6fecdfe4e859f6138b35e8375b7f411108ec73c4` の段階では、manifest 未設定、取得・展開・Visual Studio 検出の未実装、Ninja 不在による configure 前の失敗、setup/build/CTest 未確認を記録していました。これらは途中段階の制約であり、上記の最終結果によって更新されています。
