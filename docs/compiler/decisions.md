---
name: Compilerの設計判断と未検証事項
discription: 不採用案と既存資料との整合および確認すべき残件
---

# 設計判断と未検証事項

更新日：2026-11-04。状態：合意済み判断の再構成・資料照合済み・動作未検証。対象：判断の追跡。

## 不採用案の参照先

仕様と採用理由の重複管理を避け、判断の詳細は各文書に集約する。

| 不採用・見送り案 | 理由と採用方針の参照先 |
|---|---|
| PATH上のコンパイラを使う | 環境と診断が不統一になる：[入力・設定](interface.md) |
| GUIが一時ファイルを生成する | 責務が分散する：[概要](overview.md)・[資源管理](resources.md) |
| GUIがプロセスを直接管理する／CompilerがGUI通知を直接行う | GUI依存と責務重複が増える：[プロセス制御](process.md) |
| GUIまたはCompilerが非同期スレッドを管理する | 専用モジュールで分離・独立テストする：[非同期連携](process.md) |
| ログを無制限保存する／stdout・stderrを別管理する | メモリ制御と初期実装の簡素化：[診断ログ](diagnostics.md) |
| 任意のコンパイルオプションを公開する | 診断条件・設定・テスト負担が増える：[技術選定](technology.md) |
| 終了コードだけで全状態を表す | 起動失敗等を区別できない：[結果状態](interface.md) |
| メモリ解放と出力削除を一体化する | 寿命が異なる：[所有権](resources.md) |
| 不完全な出力を常に次回起動まで残す | 通常処理で不要ファイルを速やかに回収する：[後片付け](resources.md) |

## 既存資料との照合

2026-11-04にGitHubの既定ブランチで次を確認した。既存ADRを今回の文書で置換・変更はしない。

- [ADR-0003](https://github.com/nek0m0dok1/our-taskdepot/blob/main/docs/adr/0003-compiler-integration-policy.md)：LLVM-MinGW UCRT同梱、Windows 11 x64、版は検証後固定。整合する。
- [ADR-0006](https://github.com/nek0m0dok1/our-taskdepot/blob/main/docs/adr/0006-process-execution-and-stdio.md)：主対象は生成プログラムの実行・標準入力。Compiler診断とは別管理と明記されている。今回の統合ログ・終了後表示を実行側へ適用しない。
- [ADR-0007](https://github.com/nek0m0dok1/our-taskdepot/blob/main/docs/adr/0007-module-architecture.md)：機能別・GUI非依存・公開契約を事前共有。非同期制御の追加はこの方針の具体化だが、既存一覧にはまだない。
- [ADR-0008](https://github.com/nek0m0dok1/our-taskdepot/blob/main/docs/adr/0008-testing-strategy.md)：CTest・Unity・実コンパイラ結合テスト・GUI手動確認と整合する。
- [ADR-0011](https://github.com/nek0m0dok1/our-taskdepot/blob/main/docs/adr/0011-gui-implementation-simplification.md)：初期GUIは標準複数行コントロール、共有契約でバックエンドと分離。本設計はScintilla導入を要求しない。
- [PRD](https://github.com/nek0m0dok1/our-taskdepot/blob/main/docs/PRD.md)には実行機能・コンパイラ選定を未決とする記述が残り、後続ADRと差がある。Compilerは出力の受け渡しまでとし、製品全体の実行範囲は本再構成では決め直さない。

参照チャットのADR-0012は草案であり、採用済みのリポジトリ文書とは扱わない。元DDDのローカル既存資料は`docs/specs/compiler.md`。今回は`docs/compiler/`へ分割版を配置し、その既存ファイルは変更していない。

## 未検証事項と確認方法

| 残件 | 影響・確認方法 |
|---|---|
| 正確なLLVM-MinGW版・C23対応・同梱配置と依存 | 必要機能と固定生成オプションを実コンパイラで確認し、版と配布構成を固定する |
| ライセンスと再配布 | 各構成要素の条件と必要な同梱文書・義務を確認する |
| 診断文字コード | 実際の出力条件を確認し、変換・置換・1MiB境界を試験する |
| Job Object・終了競合 | 子孫停止、タイムアウト・中断・自然終了、回収との競合をWindows 11で検証する |
| 安全な一時領域 | 管理情報・使用中判定・リンク対策・3秒目安・失敗時の続行可否を検証する |
| 日本語環境と既存構成 | 日本語・空白パス、CMake・CTestの再現性を実機で確認する |
| モジュール間契約 | 状態名・公開関数・型・同期機構を既存ヘッダーと担当者間で具体化する |

設計採用と動作確認を区別する。未検証事項の結果は検証記録やIssueへ残し、設計変更が必要なら対象箇所・理由・影響を整理して合意を得る。
