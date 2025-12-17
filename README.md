# ncnn-llm-mcp-sdimggen

基于 **ncnn** 的 Stable Diffusion 推理小工具，用于给 **ncnn-llm** 适配“图片生成”能力（作为 MCP 工具/后端可执行程序被调用）。

本项目的实现主要参考：
- ncnn：https://github.com/Tencent/ncnn
- Stable-Diffusion-NCNN：https://github.com/EdVince/Stable-Diffusion-NCNN

## 目标

- 提供一个可离线运行的 Stable Diffusion（text-to-image）推理程序方便被 `ncnn-llm` 的 MCP 层集成与调度

## 当前实现

可执行程序：
- `stable-diffusion-ncnn-mcp`：MCP stdio 服务端（stdin/stdout JSON-RPC）

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

## MCP（stdio）使用

启动 MCP 服务端（建议传绝对路径的 `assets_dir`，避免工作目录变化导致找不到模型）：

```bash
./build/stable-diffusion-ncnn-mcp --assets-dir ./assets
```

工具名：`sd_txt2img`

- 输入：`prompt`/`negative_prompt`/`width`/`height`/`steps`/`seed`/`mode`/`assets_dir`
- 输出：
  - 默认返回 `image/png` 的 base64（`content[].type == "image"`）
  - 可选 `output`：`base64` / `file` / `both`
  - 当 `output` 包含 `file` 时，可用 `out_path` 指定写出路径；未指定则写到 `mcp_outputs/` 下，并在 `content` 里返回该路径（text）

## 模型与资产说明

`assets/` 目录包含推理所需的 `*.param`、`vocab.txt`、`log_sigmas.bin` 等文件；权重 `*.bin`（如 UNet/CLIP/VAE）体积较大，仓库默认在 `.gitignore` 中忽略 `assets/*.bin`。

如需重新导出/替换模型，请参考上游仓库 `EdVince/Stable-Diffusion-NCNN` 的模型准备说明与脚本。

## 致谢

- Tencent/ncnn
- EdVince/Stable-Diffusion-NCNN
