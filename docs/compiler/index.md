---
name: Compiler詳細設計書
discription: Issue #1の合意済み設計を内容別に読むための入口
---

# Compiler詳細設計書（DDD）

対象：our-taskdepot Issue #1。読者：Compiler・非同期制御・GUI担当者とレビュー担当者。
更新日：2026-11-04。状態：元DDDの合意済み設計を再構成した文書。実装・実機検証は未実施。

## 読みたい内容から選ぶ

| 文書 | 内容 |
|---|---|
| [概要](overview.md) | 目的、責務、処理の流れ |
| [技術選定と理由](technology.md) | ツールチェーン、C23、固定オプション |
| [インターフェースと結果状態](interface.md) | 入力、設定、結果、解析への引き渡し |
| [プロセス制御と非同期連携](process.md) | 起動、停止、GUIとの分担 |
| [診断ログと文字コード](diagnostics.md) | パイプ、1MiB制限、UTF-8 |
| [一時ファイルとメモリ管理](resources.md) | 所有権、削除、起動時回収 |
| [異常系とテスト](testing.md) | 失敗分類、検証項目、完成条件 |
| [設計判断と未検証事項](decisions.md) | 不採用案、ADR照合、確認事項 |

## 文書の扱い

元資料は[参照チャット](chatgpt-conversation://6aea8006-56cc-83ee-9580-4f3ffec8ef83)の2026-11-04付「Compilerモジュール 詳細設計書（DDD）」と、同チャットの監査回答・ADR草案。元DDD第1～20章を上記に分割し、繰り返しの要約を統合した。新しい仕様決定は加えていない。

実装手段は調整できるが、合意済みの外部仕様や責務を変更する場合は、理由と影響を整理して合意を得る。資料間の未解決差異も独断で変更しない。

GitHubの[Issue #1](https://github.com/nek0m0dok1/our-taskdepot/issues/1)と既存ADRを2026-11-04に確認した。照合範囲と残る差異は[設計判断](decisions.md)に記載。文書再構成の完了は、Compiler実装の完了を意味しない。
