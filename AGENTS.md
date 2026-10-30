# プロジェクト指示

- GitHubリポジトリ（Issue・PRの作成先）: https://github.com/nek0m0dok1/our-taskdepot
- 開発基盤の要件は `docs/dev-prepare.md`、設計判断は `docs/adr/` に従う。
- 対象はWindows 11 x64。アプリ本体はC/MSVC、利用者コードはLLVM-MinGW UCRTでコンパイルする。
- 今回は開発基盤だけを整備し、アプリの機能は実装しない。
- 既存のユーザー変更を保持し、検証済みの公式URL・バージョン・チェックサムだけを登録する。
- 検証できない項目を成功扱いせず、実行条件と未完了事項を記録する。

## Workspace反映とコミット

- 実装依頼ではグローバルの worktree-implementation スキルを使い、変更はOrca workspace/worktreeで行う。
- このプロジェクトでエージェントはコミットを作成しない。作業完了後は sync-workspace-to-repository スキルを使い、レビュー済みのworkspace変更を C:\Users\MimiC\our-taskdepot に反映する。
- 元repoへの反映後も変更は未ステージ・未コミットのまま残す。既存の作業や未追跡ファイルを消さず、差分が競合するファイルは上書きせず報告する。
