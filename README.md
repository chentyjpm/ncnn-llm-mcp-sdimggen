# ncnn-llm-mcp-sdimggen

基于 **ncnn** 的 Stable Diffusion 推理小工具，用于给 **ncnn-llm** 适配“图片生成”能力（作为 MCP 工具/后端可执行程序被调用）。

本项目的实现主要参考：
- ncnn：https://github.com/Tencent/ncnn
- Stable-Diffusion-NCNN：https://github.com/EdVince/Stable-Diffusion-NCNN

## 目标

- 提供一个可离线运行的 Stable Diffusion（text-to-image）推理程序
- 通过简单的文件输入（`magic.txt`）/文件输出（`result_*.png`）方式，方便被 `ncnn-llm` 的 MCP 层集成与调度

## 当前实现

可执行程序：`stable-diffusion-ncnn`

- 输入：运行目录下的 `magic.txt`
- 模型：`assets/` 下的 ncnn `*.param` 与 `*.bin`（注意：仓库默认忽略提交 `assets/*.bin`）
- 输出：`result_{step}_{seed}_{height}x{width}.png`

## magic.txt 协议（与 MCP 集成的“最小接口”）

程序启动时会读取当前工作目录中的 `magic.txt`，共 7 行：

1. `height`（建议为 64 的倍数）
2. `width`（建议为 64 的倍数）
3. `mode`（0/1：影响 ncnn 的 winograd/sgemm 选项）
4. `step`（采样步数）
5. `seed`（随机种子，填 0 表示用当前时间生成）
6. `positive_prompt`
7. `negative_prompt`

示例：见仓库根目录 `magic.txt`。

> 集成思路：MCP 服务端生成/写入 `magic.txt` → 调用本项目产物 `stable-diffusion-ncnn` → 读取生成的 `result_*.png` 并回传给上层。

## 构建

依赖：
- CMake
- C++17 编译器
- ncnn（`CMakeLists.txt` 里通过 `CMAKE_PREFIX_PATH` 查找）
- `third_party/stb_image_write.h`（用于写出 PNG，本仓库已包含）

构建命令：

```bash
cmake -S . -B build
cmake --build build -j
```

`CMakeLists.txt` 会把 `assets/` 复制到 `build/` 目录。

## 运行

方式 1：在仓库根目录运行（默认会读取根目录的 `magic.txt` 与 `assets/`）

```bash
./build/stable-diffusion-ncnn
```

方式 2：在 `build/` 目录运行（需要把 `magic.txt` 放到 `build/` 中）

```bash
cp magic.txt build/magic.txt
cd build
./stable-diffusion-ncnn
```

运行完成后会在当前目录生成 `result_*.png`。

## 模型与资产说明

`assets/` 目录包含推理所需的 `*.param`、`vocab.txt`、`log_sigmas.bin` 等文件；权重 `*.bin`（如 UNet/CLIP/VAE）体积较大，仓库默认在 `.gitignore` 中忽略 `assets/*.bin`。

如需重新导出/替换模型，请参考上游仓库 `EdVince/Stable-Diffusion-NCNN` 的模型准备说明与脚本。

## 致谢

- Tencent/ncnn
- EdVince/Stable-Diffusion-NCNN
