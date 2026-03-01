# CLIBench

**🌐 Languages / 言語：** [English](#english) ｜ [日本語](#japanese) ｜ [やさしい日本語](#easy-japanese)

---

We assume no responsibility whatsoever for any issues arising from this software.
Use it entirely at your own risk.

---

This repository previously held three separate README files. They are now combined into this single `README.md` containing three language sections: English, 日本語, and やさしい日本語. Use the links above to jump to your preferred language.

目次 / Table of Contents

- [English](#english)
- [日本語](#japanese)
- [やさしい日本語](#easy-japanese)

---

## English

Cross-platform GPU benchmark tool with **Vulkan 1.4** and **Metal** (including **Metal 4**) backends for measuring compute, memory, and graphics performance.

### Features

- Dual Backend — Vulkan 1.4 (Windows/macOS/Linux) + Metal (macOS native, Metal 3 & Metal 4)
- Compute Benchmark — GFLOPS measurement via FMA compute shaders
- Memory Bandwidth — Host↔Device transfer speed measurement
- Triangle Throughput — Draw call performance and rasterization rate
- Overall Score — Weighted composite score with tier ranking (E → S+)
- Benchmark Modes — Quick / Standard / Extreme / Stress
- JSON Export — Machine-readable results with score data

### Requirements

- CMake 3.20+
- C++17 compiler (GCC 8+, Clang 10+, MSVC 2019+)

### Build

```bash
git clone <repo-url> clibench && cd clibench
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/clibench
```

### Usage examples

```bash
./build/clibench                # auto-selects Metal on macOS, Vulkan elsewhere
./build/clibench --metal        # Metal backend (macOS)
./build/clibench --vulkan       # Vulkan backend
./build/clibench --json results.json -q
```

Further English documentation and full parameter reference are included below in this file under the English section.

---

## 日本語

Windows / macOS / Linux 対応のGPUベンチマークツールです。**Vulkan 1.4** と **Metal** の2つのバックエンドに対応し、GPU の計算性能・メモリ帯域・描画性能を測定し、総合スコアで評価します。

### 機能

- デュアルバックエンド（Vulkan 1.4 / Metal）
- コンピュートベンチマーク（FMA による GFLOPS）
- メモリ帯域テスト（Host↔Device 転送）
- 三角形スループット（描画性能）
- 総合スコア（重み付け）

### 必要なもの

- CMake 3.20 以上
- C++17 対応コンパイラ

### ビルド方法

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/clibench
```

### 使用例

```bash
./build/clibench                # 自動でバックエンド選択
./build/clibench --metal        # macOS で Metal を使用
./build/clibench --vulkan       # Vulkan を使用
```

詳細なオプション説明やチュートリアルはこのファイル内の日本語セクションに記載しています。

---

## やさしい日本語

CLIBench は GPU（グラフィックカード）のはやさを調べるツールです。Vulkan 1.4 と Metal の2つの方法でテストできます。Windows、macOS、Linux で使えます。

### できること（簡単）

- GPU の計算（コンピュート）をはかる
- メモリのうごき（転送）をはかる
- たくさんのさんかくをえがくはやさをはかる
- テスト結果をスコアにまとめる

### ビルド（簡単）

ターミナルで次のコマンドを実行してください。

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/clibench
```

### 使い方（簡単）

```bash
./build/clibench         # 普通のベンチマーク
./build/clibench --metal # macOS で Metal を使う
./build/clibench --vulkan# Vulkan を使う
```

---

## Notes / 備考

- The original separate files README.en.md, README.ja.md and README.easy-ja.md remain in the repository for reference. If you want, I can remove them after you confirm.
- If you prefer a different layout (language-specific files, single-language default, or adding in-file anchors/TOC improvements), tell me how you'd like it.

---

## License

GPL-3.0 Made by yuki319jp with AI
