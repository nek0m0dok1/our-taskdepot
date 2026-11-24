---
name: Build system selection
discription: Records the supported build system and compiler choice for the application foundation.
---

# ADR-0002：ビルドシステムの選定

**ステータス：採用決定**  
**決定日：2026年10月26日**

## 背景

開発メンバーが使用するIDEやエディタに依存せず、共通の手順でアプリケーションをビルドできる環境を構築する。

開発メンバーの環境構築負担を減らすため、初期設定や依存関係の管理は開発環境の整備担当者が中心となって行う。

## 検討した選択肢

- Visual Studioのプロジェクトファイルによるビルド
- CMake + MSVC + Ninja

## 決定

以下の構成を採用する。

- ビルドシステム：CMake
- 開発用コンパイラ：MSVC
- ジェネレーター：Visual Studio 18 2026 (x64)
- ツールセット：v145（MSVC 19.50 以上）
- Visual Studio インスタンス：Visual Studio 2026 18.x
- CMake：Visual Studio に同梱された 4.2 以上
- Windows SDK：10.0.22000.0 以上
- 共通設定：CMakePresets.json

## 決定理由

- 特定のIDEに依存せずビルドできる。
- Win32 APIを利用するWindowsアプリケーションの開発に適している。
- 共通のビルド設定をGitで管理できる。
- メンバーごとの設定作業を減らせる。
- GUIと内部処理を分離したプロジェクト構成に対応できる。

## 懸念点と対応

### 開発ツールの導入

Visual Studio 2026、Windows SDK と同梱 CMake が必要となる。セットアップは既存の18.xインスタンスだけを選び、MSVC 19.50以降、v145、SDK 10.0.22000.0以降、CMake 4.2以降を確認する。条件を満たさない環境は明示的に拒否し、Visual Studioを自動導入・変更しない。Visual Studio generatorを使い、Ninja generatorは採用しない。

必要なツールと導入手順をREADMEにまとめる。

### 環境差異

ツールのバージョンや設定の違いによってビルドできない可能性がある。

CMakePresets.jsonでビルド設定を共通化し、Visual Studio 2026 のみをサポートする。

### 学習コスト

CMakeの設定や依存関係の管理には学習が必要となる。

初期設定は開発環境の整備担当者が中心となって行い、ほかのメンバーは共通のコマンドでビルドできるようにする。

## 今後の作業

- CMakeLists.txtを作成する。
- CMakePresets.jsonを作成する。
- Visual Studio 2026 ジェネレーターを利用したビルドを検証する。
- ScintillaのDLLを組み込む。
- READMEに環境構築手順を記載する。
- メンバーのPCでビルドを確認する。
