# ncnn-llm-mcp-sdimggen

基于 **ncnn** 的 Stable Diffusion 推理小工具，用于给 **ncnn-llm** 适配“图片生成”能力（作为 MCP 工具/后端可执行程序被调用）。

配合来使用的项目
- ncnn_llm：https://github.com/futz12/ncnn_llm

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
- ncnn（通过 `ncnn_DIR` / `CMAKE_PREFIX_PATH` 查找）
- `third_party/stb_image_write.h`（用于写出 PNG，本仓库已包含）

快速构建命令（Linux + 使用仓库内预编译 ncnn 时）：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

`CMakeLists.txt` 会把 `assets/` 复制到 `build/` 目录。

### Linux / macOS / Windows 编译说明

本项目的核心依赖是 **ncnn**：

- Linux 下默认会尝试使用仓库内自带的预编译 ncnn（`SD_USE_BUNDLED_NCNN=ON` 且仅 Linux 生效）
- macOS / Windows 推荐自行编译安装 ncnn，然后通过 `ncnn_DIR` 让 CMake 找到 `ncnnConfig.cmake`

构建开关：
- `SD_BUILD_DEMO`：是否构建本地 demo（默认 ON）
- `SD_BUILD_MCP`：是否构建 MCP server（默认 ON）
- `SD_USE_BUNDLED_NCNN`：Linux 下是否使用仓库内预编译 ncnn（默认 ON）

#### Linux（Ubuntu）

方式 A：使用仓库内预编译 ncnn（默认）

```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

方式 B：自行编译安装 ncnn（推荐用于非 Ubuntu 22.04 或预编译不兼容时）

```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build git

git clone --depth 1 --branch 20230517 https://github.com/Tencent/ncnn.git _deps/ncnn
cmake -S _deps/ncnn -B _deps/ncnn/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DNCNN_VULKAN=OFF \
  -DNCNN_OPENMP=OFF \
  -DNCNN_BUILD_TOOLS=OFF \
  -DNCNN_BUILD_EXAMPLES=OFF \
  -DNCNN_BUILD_BENCHMARK=OFF \
  -DNCNN_BUILD_TESTS=OFF \
  -DNCNN_SHARED_LIB=OFF \
  -DCMAKE_INSTALL_PREFIX="$PWD/_deps/ncnn/install"
cmake --build _deps/ncnn/build -j
cmake --install _deps/ncnn/build

cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DSD_USE_BUNDLED_NCNN=OFF \
  -Dncnn_DIR="$PWD/_deps/ncnn/install/lib/cmake/ncnn"
cmake --build build -j
```

#### macOS（Apple Silicon / Intel）

```bash
brew update
brew install cmake ninja git

git clone --depth 1 --branch 20230517 https://github.com/Tencent/ncnn.git _deps/ncnn
cmake -S _deps/ncnn -B _deps/ncnn/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DNCNN_VULKAN=OFF \
  -DNCNN_OPENMP=OFF \
  -DNCNN_BUILD_TOOLS=OFF \
  -DNCNN_BUILD_EXAMPLES=OFF \
  -DNCNN_BUILD_BENCHMARK=OFF \
  -DNCNN_BUILD_TESTS=OFF \
  -DNCNN_SHARED_LIB=OFF \
  -DCMAKE_INSTALL_PREFIX="$PWD/_deps/ncnn/install"
cmake --build _deps/ncnn/build -j
cmake --install _deps/ncnn/build

cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DSD_USE_BUNDLED_NCNN=OFF \
  -Dncnn_DIR="$PWD/_deps/ncnn/install/lib/cmake/ncnn"
cmake --build build -j
```

#### Windows（MSVC + Ninja）

建议使用 “Developer PowerShell for VS 2022”（或先运行 `vcvars64.bat`）以确保 `cl.exe` 在 PATH 中。

```powershell
choco install -y cmake ninja git

git clone --depth 1 --branch 20230517 https://github.com/Tencent/ncnn.git _deps/ncnn

$prefix = (Resolve-Path ".").Path
$args = @(
  "-S","_deps/ncnn","-B","_deps/ncnn/build","-G","Ninja",
  "-DCMAKE_BUILD_TYPE=Release",
  "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
  "-DCMAKE_C_COMPILER=cl",
  "-DCMAKE_CXX_COMPILER=cl",
  "-DNCNN_VULKAN=OFF",
  "-DNCNN_OPENMP=OFF",
  "-DNCNN_BUILD_TOOLS=OFF",
  "-DNCNN_BUILD_EXAMPLES=OFF",
  "-DNCNN_BUILD_BENCHMARK=OFF",
  "-DNCNN_BUILD_TESTS=OFF",
  "-DNCNN_SHARED_LIB=OFF",
  "-DCMAKE_INSTALL_PREFIX=$prefix/_deps/ncnn/install"
)
cmake @args
cmake --build _deps/ncnn/build --parallel 2
cmake --install _deps/ncnn/build

$args = @(
  "-S",".","-B","build","-G","Ninja",
  "-DCMAKE_BUILD_TYPE=Release",
  "-DSD_USE_BUNDLED_NCNN=OFF",
  "-Dncnn_DIR=$prefix/_deps/ncnn/install/lib/cmake/ncnn"
)
cmake @args
cmake --build build --parallel 2
```

#### 只构建 MCP（可选）

```bash
cmake -S . -B build -DSD_BUILD_DEMO=OFF -DSD_BUILD_MCP=ON
cmake --build build -j
```

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
